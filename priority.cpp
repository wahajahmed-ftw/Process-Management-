#include <iostream>
#include <unordered_map>
#include <vector>
#include <memory>
#include <algorithm>
#include <queue>
#include <mutex> // Added for std::mutex
#include <condition_variable>
#include <thread>
#include <windows.h>

// ProcessControlBlock and Message definitions remain the same
class ProcessControlBlock {
public:
    int pid;
    int arrival_time;
    int burst_time;
    int remaining_time; // Added remaining_time member
    int priority;
    std::string status;

    ProcessControlBlock(int pid, int arrival_time, int burst_time, int priority, const std::string& status)
        : pid(pid), arrival_time(arrival_time), burst_time(burst_time), remaining_time(burst_time), priority(priority), status(status) {}

    void display() const {
        std::cout << "ProcessControlBlock(pid=" << pid
                  << ", arrival_time=" << arrival_time
                  << ", burst_time=" << burst_time
                  << ", remaining_time=" << remaining_time // Updated display method to include remaining_time
                  << ", priority=" << priority
                  << ", status=" << status << ")" << std::endl;
    }
};

struct Message {
    int sender_pid;
    int receiver_pid;
    std::string content;

    Message(int sender, int receiver, const std::string& msg)
        : sender_pid(sender), receiver_pid(receiver), content(msg) {}
};

class MessageQueue {
public:
    MessageQueue() : mutex_(CreateMutex(NULL, FALSE, NULL)) {} // Initialize mutex using Windows API

    void push(const Message& msg) {
        WaitForSingleObject(mutex_, INFINITE);
        queue_.push(msg);
        ReleaseMutex(mutex_);
    }

    Message pop() {
        WaitForSingleObject(mutex_, INFINITE);
        Message msg = queue_.front();
        queue_.pop();
        ReleaseMutex(mutex_);
        return msg;
    }

    void display_queue() const {
    WaitForSingleObject(mutex_, INFINITE); // Wait for mutex to be available
    std::cout << "Message Queue Contents:\n";
    std::queue<Message> tempQueue = queue_; // Create a temporary queue to avoid modifying the original queue
    ReleaseMutex(mutex_); // Release the mutex before printing

    while (!tempQueue.empty()) {
        Message msg = tempQueue.front();
        tempQueue.pop();
        std::cout << "Sender PID: " << msg.sender_pid << ", Receiver PID: " << msg.receiver_pid << ", Content: " << msg.content << std::endl;
    }
}


private:
    std::queue<Message> queue_;
    HANDLE mutex_; // Using Windows API mutex handle
};

class ProcessManager {
public:
    void create_process(int pid, int arrival_time, int burst_time, int priority, const std::string& status = "new") {
        if (processes.find(pid) != processes.end()) {
            throw std::invalid_argument("Process with PID " + std::to_string(pid) + " already exists.");
        }
        auto pcb = std::make_shared<ProcessControlBlock>(pid, arrival_time, burst_time, priority, status);
        processes[pid] = pcb;
        ready_queue.push_back(pcb);
    }

    void terminate_process(int pid) {
        auto it = processes.find(pid);
        if (it == processes.end()) {
            throw std::invalid_argument("Process with PID " + std::to_string(pid) + " does not exist.");
        }
        it->second->status = "terminated";
    }

    void display_process(int pid) const {
        auto it = processes.find(pid);
        if (it != processes.end()) {
            it->second->display();
        } else {
            std::cout << "Process with PID " << pid << " not found." << std::endl;
        }
    }

    void display_all_processes() const {
        for (const auto& pair : processes) {
            pair.second->display();
        }
    }

    void send_message(int sender_pid, int receiver_pid, const std::string& content) {
        Message msg(sender_pid, receiver_pid, content);
        message_queue.push(msg);
    }

    void receive_messages(int pid) {
        while (true) {
            Message msg = message_queue.pop();
            if (msg.receiver_pid == pid) {
                std::cout << "Process " << pid << " received message from Process " << msg.sender_pid << ": " << msg.content << std::endl;
            }
        }
    }

    void schedule_srtf() {
        auto compare = [](const std::shared_ptr<ProcessControlBlock>& a, const std::shared_ptr<ProcessControlBlock>& b) {
            if (a->remaining_time != b->remaining_time) {
                return a->remaining_time > b->remaining_time; // Shorter remaining time first
            }
            return a->arrival_time > b->arrival_time; // Lower arrival time first
        };
        std::priority_queue<std::shared_ptr<ProcessControlBlock>, std::vector<std::shared_ptr<ProcessControlBlock>>, decltype(compare)> pq(compare);

        int current_time = 0;

        std::cout << "Executing Shortest Remaining Time First (SRTF) Schedule:\n";

        while (true) {
            bool all_terminated = true;
            for (auto& pcb : ready_queue) {
                if (pcb->status != "terminated") {
                    if (pcb->arrival_time <= current_time && pcb->status == "new") {
                        pq.push(pcb);
                        pcb->status = "waiting";
                    }
                    all_terminated = false;
                }
            }

            if (pq.empty() && all_terminated) {
                break;
            }

            if (!pq.empty()) {
                auto pcb = pq.top();
                pq.pop();
                pcb->status = "running";
                pcb->display();

                pcb->remaining_time -= 1;
                current_time += 1;

                if (pcb->remaining_time > 0) {
                    pcb->status = "waiting";
                    pq.push(pcb);
                } else {
                    pcb->status = "terminated";
                }
            } else {
                current_time += 1; // Advance time if no process is ready
            }
        }
    }

    void show_messages()
    {
        message_queue.display_queue();
    }

    void schedule_round_robin_with_priority(int quantum) {
        auto compare = [](const std::shared_ptr<ProcessControlBlock>& a, const std::shared_ptr<ProcessControlBlock>& b) {
            if (a->priority != b->priority) {
                return a->priority < b->priority; // Higher priority first
            }
            return a->arrival_time > b->arrival_time; // Lower arrival time first
        };
        std::priority_queue<std::shared_ptr<ProcessControlBlock>, std::vector<std::shared_ptr<ProcessControlBlock>>, decltype(compare)> pq(compare);

        // Load all processes into the priority queue
        for (auto& pcb : ready_queue) {
            if (pcb->status != "terminated") {
                pq.push(pcb);
            }
        }

        std::cout << "Executing Round Robin with Priority Scheduling:\n";

        while (!pq.empty()) {
            auto pcb = pq.top();
            pq.pop();

            if (pcb->status != "terminated") {
                pcb->status = "running";
                pcb->display();

                int execution_time = std::min(quantum, pcb->remaining_time);
                pcb->remaining_time -= execution_time;

                if (pcb->remaining_time > 0) {
                    pcb->status = "waiting";
                    pq.push(pcb);  // Push back into the queue if not finished
                } else {
                    pcb->status = "terminated";
                }
            }
        }
    }

private:
    std::unordered_map<int, std::shared_ptr<ProcessControlBlock>> processes;
    std::vector<std::shared_ptr<ProcessControlBlock>> ready_queue;
    MessageQueue message_queue;
};

int main() {
    ProcessManager pm;

    // Create processes
    pm.create_process(1, 0, 10, 1);
    pm.create_process(2, 1, 5, 2);
    pm.create_process(3, 2, 2, 1);
    pm.create_process(4, 3, 1, 3);

    // Display all processes before scheduling
    std::cout << "All Processes:\n";
    pm.display_all_processes();
    pm.schedule_round_robin_with_priority(2);

    // Send messages between processes
    pm.send_message(1, 2, "Hello from Process 1 to Process 2!");
    pm.send_message(3, 1, "Hello from Process 3 to 1!");
    pm.show_messages();
    
    
}
