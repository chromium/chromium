// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_ACTIVE_PROCESSES_WIN_H_
#define COMPONENTS_TRACING_COMMON_ACTIVE_PROCESSES_WIN_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/win/sid.h"
#include "components/tracing/tracing_export.h"

namespace tracing {

// A helper that tracks active processes on the system and categorizes them,
// providing an efficient determination of a process's category given a thread
// id. An instance of this class is expected to be populated dynamically based
// on Process and Thread events sourced from the Windows system event provider
// via ETW. It provides accurate results to the extent possible, given that
// trace events may be lost.
class TRACING_EXPORT ActiveProcesses {
 public:
  // A process's category. Values of this type must be sequential and begin with
  // zero, but are not persisted or transmitted. Values may be reordered, added,
  // or removed.
  enum class Category {
    // A process that belongs to the tracing client.
    kClient = 0,
    // A process that belongs to the system.
    kSystem = 1,
    // Any process that doesn't meet the criteria above.
    kOther = 2,
  };

  // Constructs an instance for a trace initiated on behalf of the process
  // identified by `client_pid`. This and 1) any of its direct children of it
  // with the same image filename and 2) any other program residing in the same
  // directory tree will be categorized as belonging to the tracing client.
  explicit ActiveProcesses(base::ProcessId client_pid);
  ActiveProcesses(const ActiveProcesses&) = delete;
  ActiveProcesses& operator=(const ActiveProcesses&) = delete;
  ~ActiveProcesses();

  // Adds a process to the collection of active processes. The process is
  // categorized upon addition unless the client process has yet to be added.
  // If `pid` matches `client_pid`, all previously-added processes are
  // categorized.
  void AddProcess(uint32_t pid,
                  uint32_t parent_pid,
                  uint32_t session_id,
                  std::optional<base::win::Sid> sid,
                  std::string image_file_name,
                  std::wstring command_line);

  // Removes a process and all of its threads from the collection.
  void RemoveProcess(uint32_t pid);

  // Adds a process's thread to the collection.
  void AddThread(uint32_t pid, uint32_t tid, std::wstring thread_name);

  // Sets the name of a thread in the collection.
  void SetThreadName(uint32_t pid, uint32_t tid, std::wstring thread_name);

  // Remove's a process's thread from the collection, if it is present.
  void RemoveThread(uint32_t pid, uint32_t tid);

  // Returns the category for the process to which the thread `tid` belongs.
  // Returns kOther if `tid` is unknown.
  Category GetThreadCategory(uint32_t tid) const;

  // Returns a thread's name, or an empty view if not found or unset. The
  // returned view may become invalidated following any other operation on this
  // instance.
  std::wstring_view GetThreadName(uint32_t tid) const;

  // Returns a process's image file name, or an empty view if not found or
  // unset. The returned view may become invalidated following any other
  // operation on this instance.
  std::string_view GetProcessImageFileName(uint32_t pid) const;

 private:
  struct Process {
    Process(uint32_t pid,
            uint32_t parent_pid,
            uint32_t session_id,
            std::optional<base::win::Sid> sid,
            std::string image_file_name,
            std::wstring command_line);
    ~Process();

    uint32_t pid;
    uint32_t parent_pid;
    uint32_t session_id;
    std::optional<base::win::Sid> sid;
    std::string image_file_name;
    std::wstring command_line;
    Category category;
    std::unordered_set<uint32_t> threads;
  };

  // Handles addition of the `Process` corresponding to the tracing client. All
  // previously-added processes of type `kOther` are recategorized.
  void OnClientAdded(Process* client);

  // Handles removal of the `Process` corresponding to the tracing client. All
  // previously-added processes of type `kClient` are recategorized as
  // `kOther`.
  void OnClientRemoved(Process* client);

  // Compute and returns the category of `process`.
  Category DetermineCategory(const Process& process);

  // Returns the first component of `process`'s `command_line.
  static base::FilePath GetProgram(const Process& process);

  // The pid of the client process on behalf of which traces are collected.
  const base::ProcessId client_pid_;

  // The path to the application directory of which the service is a part. In
  // the case of Google Chrome, this is the path to the "Application" directory
  // containing chrome.exe and the version directory.
  const base::FilePath application_dir_;

  // True if the client appears to "belong to" the service, in the sense that it
  // resides either in the same directory as the service or one directory above
  // if the service is in a version directory.
  bool client_in_application_ = false;

  // A mapping of a process's id (pid) to its `Process` struct.
  using PidProcessMap = std::unordered_map<uint32_t, Process>;
  PidProcessMap processes_;

  // A mapping of a thread's id (tid) to its name and corresponding process.
  std::unordered_map<uint32_t, std::pair<std::wstring, raw_ptr<Process>>>
      threads_;

  // The `Process` struct with pid `client_pid_`, or null if the process has
  // not yet been added or has been removed.
  raw_ptr<Process> client_process_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_ACTIVE_PROCESSES_WIN_H_
