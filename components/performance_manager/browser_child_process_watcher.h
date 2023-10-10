// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_BROWSER_CHILD_PROCESS_WATCHER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_BROWSER_CHILD_PROCESS_WATCHER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/process/process.h"
#include "components/performance_manager/public/browser_child_process_host_id.h"
#include "content/public/browser/browser_child_process_observer.h"

namespace base {
class Process;
}

namespace performance_manager {

class ProcessNodeImpl;

// Responsible for maintaining the process nodes for the browser, the GPU and
// utility process.
class BrowserChildProcessWatcher : public content::BrowserChildProcessObserver {
 public:
  BrowserChildProcessWatcher();

  BrowserChildProcessWatcher(const BrowserChildProcessWatcher&) = delete;
  BrowserChildProcessWatcher& operator=(const BrowserChildProcessWatcher&) =
      delete;

  ~BrowserChildProcessWatcher() override;

  // Initialize this watcher.
  void Initialize();

  // Tear down this watcher and any state it's gathered.
  void TearDown();

  // Returns the ProcessNode for the browser process or nullptr if it does not
  // exist, which can happen in tests.
  ProcessNodeImpl* browser_process_node() {
    return browser_process_node_.get();
  }

  // Returns the ProcessNode for `id` or nullptr if it does not exist.
  ProcessNodeImpl* GetChildProcessNode(BrowserChildProcessHostId id);

  // Allows tests to create a ProcessNode for `data`. In production the
  // ProcessNode is created when the host's child process is launched, but it's
  // not always possible to launch a real process in tests so this can simulate
  // it by passing a running process, possibly base::Process::Current(), in
  // data.GetProcess(). The ProcessNode must not already exist. It will not be
  // tied to the lifetime of the process.
  void CreateChildProcessNodeForTesting(const content::ChildProcessData& data);

  // Allows tests to delete the ProcessNode for `data`. In production this
  // happens when the host's child process exits, but nodes created with
  // CreateChildProcessNodeForTesting() won't be tied to the lifetime of
  // a real process. Must not be called again if the node was already deleted.
  void DeleteChildProcessNodeForTesting(const content::ChildProcessData& data);

  // Allows tests to delete the browser process node, which is created by
  // default in Initialize(). Must not be called again if the node was already
  // deleted.
  void DeleteBrowserProcessNodeForTesting();

 private:
  // BrowserChildProcessObserver overrides.
  void BrowserChildProcessLaunchedAndConnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessCrashed(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;
  void BrowserChildProcessKilled(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;

  void TrackedProcessExited(BrowserChildProcessHostId id, int exit_code);

  static void OnProcessLaunched(const base::Process& process,
                                const std::string& metrics_name,
                                ProcessNodeImpl* process_node);

  std::unique_ptr<ProcessNodeImpl> browser_process_node_;

  // This map keeps track of all GPU and Utility processes by their unique ID
  // from |content::ChildProcessData|. Apparently more than one GPU process can
  // be existent at a time, though secondaries are very transient.
  base::flat_map<BrowserChildProcessHostId, std::unique_ptr<ProcessNodeImpl>>
      tracked_process_nodes_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_BROWSER_CHILD_PROCESS_WATCHER_H_
