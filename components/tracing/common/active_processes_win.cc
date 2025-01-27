// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/active_processes_win.h"

#include <algorithm>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/version.h"

namespace tracing {

namespace {

// Returns the directory immediately above the current executable's directory if
// the directory of the current executable is of the form "W.X.Y.Z"; otherwise,
// returns the current executable's directory.
base::FilePath DetermineApplicationDirectory() {
  base::FilePath dir_path = base::PathService::CheckedGet(base::DIR_EXE);
  if (base::Version(dir_path.BaseName().MaybeAsASCII()).IsValid()) {
    // This is likely a production install, where the elevated tracing service
    // is installed in the version directory.
    return dir_path.DirName();
  }
  // Otherwise, this is likely a developer, where the elevated tracing service
  // is in the build output directory next to the browser.
  return dir_path;
}

// Returns true if `one` and `two` are equal or if `one` is a parent of `two`.
bool IsSameOrParent(const base::FilePath& one, const base::FilePath& two) {
  // Use a simple string comparison here, as the expectation is that the paths
  // will be formatted the same if they truly are the same since they share a
  // common origin in that case. There are cases where this will return a false
  // negative, but for now this is acceptable since it will lead to under-
  // rather than over-reporting.
  return one == two || one.IsParent(two);
}

}  // namespace

ActiveProcesses::Process::Process(uint32_t pid,
                                  uint32_t parent_pid,
                                  uint32_t session_id,
                                  std::optional<base::win::Sid> sid,
                                  std::string image_file_name,
                                  std::wstring command_line)
    : pid(pid),
      parent_pid(parent_pid),
      session_id(session_id),
      sid(std::move(sid)),
      image_file_name(std::move(image_file_name)),
      command_line(std::move(command_line)),
      category(Category::kOther) {}

ActiveProcesses::Process::~Process() = default;

ActiveProcesses::ActiveProcesses(base::ProcessId client_pid)
    : client_pid_(client_pid),
      application_dir_(DetermineApplicationDirectory()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ActiveProcesses::~ActiveProcesses() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ActiveProcesses::AddProcess(uint32_t pid,
                                 uint32_t parent_pid,
                                 uint32_t session_id,
                                 std::optional<base::win::Sid> sid,
                                 std::string image_file_name,
                                 std::wstring command_line) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  enum {
    kNonClientAdded,
    kClientAdded,
    kDuplicateClientAdded,
  } addition = kNonClientAdded;
  if (pid != client_pid_) {
    addition = kNonClientAdded;
  } else if (!client_process_) {
    addition = kClientAdded;
  } else {
    // A second process with the same pid as the client is not possible since
    // the tracing service self-terminates when the client process terminates.
    // Conservatively consider this to be an "other" process and forget about
    // the client.
    addition = kDuplicateClientAdded;
    OnClientRemoved(client_process_.get());
  }

  auto [iter, inserted] = processes_.try_emplace(
      pid, pid, parent_pid, session_id, std::move(sid),
      std::move(image_file_name), std::move(command_line));
  auto& process = iter->second;
  if (!inserted) {
    // Duplicate pid. The event for the removal of this pid must have been lost.
    // Forget about the old process's threads before replacing it.
    std::ranges::for_each(process.threads,
                          [this](uint32_t tid) { threads_.erase(tid); });
    process.threads.clear();
    // Move the new process's properties into the instance.
    process.parent_pid = parent_pid;
    process.session_id = session_id;
    process.sid = std::move(sid);
    process.image_file_name = std::move(image_file_name);
    process.command_line = std::move(command_line);
  }

  // Finally, set the process's category.
  switch (addition) {
    case kNonClientAdded:
      process.category = DetermineCategory(process);
      break;
    case kClientAdded:
      process.category = Category::kClient;  // This is the client's process.
      OnClientAdded(&process);
      break;
    case kDuplicateClientAdded:
      process.category = Category::kOther;
      break;
  }
}

void ActiveProcesses::RemoveProcess(uint32_t pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pid == client_pid_ && client_process_) {
    OnClientRemoved(client_process_.get());
  }

  if (auto iter = processes_.find(pid); iter != processes_.end()) {
    // Forget about this process's threads if they haven't already been removed.
    std::ranges::for_each(iter->second.threads,
                          [this](uint32_t tid) { threads_.erase(tid); });
    processes_.erase(iter);
  }  // else the event for the addition of this process must have been lost.
}

void ActiveProcesses::AddThread(uint32_t pid,
                                uint32_t tid,
                                std::wstring thread_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto process_iter = processes_.find(pid);
  if (process_iter == processes_.end()) {
    // The event for the addition of this thread's process must have been lost.
    // The reason for tracking threads is to map a tid to its process. Since it
    // will not be possible to do so for this tid, skip adding it to `threads_`.
    return;
  }
  auto [iter, inserted] =
      threads_.try_emplace(tid, std::move(thread_name), &process_iter->second);
  auto& [name, process_ptr] = iter->second;
  if (!inserted) {
    // The event for the removal of this tid must have been lost.
    // Remove the thread from the previous process's collection.
    process_ptr->threads.erase(tid);
    // Associate this thread with its new name and process.
    name = std::move(thread_name);
    process_ptr = &process_iter->second;
  }

  // Add this thread to its process's collection.
  process_ptr->threads.insert(tid);
}

void ActiveProcesses::SetThreadName(uint32_t pid,
                                    uint32_t tid,
                                    std::wstring thread_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto process_iter = processes_.find(pid);
  if (process_iter == processes_.end()) {
    // The event for the addition of this thread's process must have been lost.
    return;
  }
  auto thread_iter = threads_.find(tid);
  if (thread_iter == threads_.end()) {
    // The event for the addition of this thread must have been lost.
    return;
  }
  if (thread_iter->second.second.get() != &process_iter->second) {
    // The pid and tid are both known, but don't relate. Aggressive id reuse
    // and lost events make this possible.
    return;
  }
  thread_iter->second.first = std::move(thread_name);
}

void ActiveProcesses::RemoveThread(uint32_t pid, uint32_t tid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = threads_.find(tid);
  if (iter == threads_.end()) {
    // The event for the addition of this thread must have been lost.
    return;
  }
  auto& process_ptr = iter->second.second;
  if (process_ptr->pid != pid) {
    // Events for the removal of this tid and its addition to a different pid
    // must have been lost. Ignore this removal to avoid corrupting tracking for
    // the other process
    return;
  }
  process_ptr->threads.erase(tid);
  threads_.erase(iter);
}

ActiveProcesses::Category ActiveProcesses::GetThreadCategory(
    uint32_t tid) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (auto iter = threads_.find(tid); iter != threads_.end()) {
    return iter->second.second->category;
  }
  return Category::kOther;
}

std::wstring_view ActiveProcesses::GetThreadName(uint32_t tid) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (auto iter = threads_.find(tid); iter != threads_.end()) {
    return iter->second.first;
  }
  return {};
}

std::string_view ActiveProcesses::GetProcessImageFileName(uint32_t pid) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (auto iter = processes_.find(pid); iter != processes_.end()) {
    return iter->second.image_file_name;
  }
  return {};
}

void ActiveProcesses::OnClientAdded(Process* client) {
  client_process_ = client;
  client_in_application_ =
      IsSameOrParent(application_dir_, GetProgram(*client).DirName());

  // Re-categorize all existing "other" processes to detect client processes.
  std::ranges::for_each(
      processes_,
      [this](auto& process) {
        if (process.category == Category::kOther) {
          process.category = DetermineCategory(process);
        }
      },
      &PidProcessMap::value_type::second);
}

void ActiveProcesses::OnClientRemoved(Process* client) {
  client_process_ = nullptr;
  client_in_application_ = false;

  // All previously-discovered client processes are now "other" processes.
  std::ranges::for_each(
      processes_,
      [](auto& process) {
        if (process.category == Category::kClient) {
          process.category = Category::kOther;
        }
      },
      &PidProcessMap::value_type::second);
}

ActiveProcesses::Category ActiveProcesses::DetermineCategory(
    const Process& process) {
  // The session id for Idle, System, Secure System, Registry, smss.exe, and
  // MemCompression.
  static constexpr uint32_t kSystemSession = 0xFFFFFFFF;

  if (process.session_id == kSystemSession) {
    return Category::kSystem;  // Windows kernel processes.
  }

  if (!client_process_) {
    return Category::kOther;  // Not yet possible to associate with the client.
  }

  const Process& client = *client_process_;
  if (process.session_id != client.session_id) {
    return Category::kOther;  // Not running in the same session as the client.
  }

  if (!process.sid.has_value() || process.sid != client.sid) {
    return Category::kOther;  // Not the same user.
  }

  if (client_in_application_ &&
      IsSameOrParent(application_dir_, GetProgram(process).DirName())) {
    return Category::kClient;  // A program belonging to the client.
  }

  return Category::kOther;
}

// static
base::FilePath ActiveProcesses::GetProgram(const Process& process) {
  return base::CommandLine::FromString(process.command_line).GetProgram();
}

}  // namespace tracing
