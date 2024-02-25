// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_BROWSER_CHILD_PROCESS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_BROWSER_CHILD_PROCESS_H_

#include <memory>

#include "base/process/process.h"
#include "components/performance_manager/public/browser_child_process_host_id.h"
#include "components/performance_manager/public/browser_child_process_host_proxy.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_delegate.h"

namespace performance_manager {

// A wrapper that owns a BrowserChildProcessHost and acts as a no-op
// BrowserChildProcessHostDelegate.
//
// In production, a process and associated ProcessNode is created for
// this by calling host()->Launch(), but in unit tests this has unsatisfied
// dependencies so a ProcessNode can be created by calling
//
//   PerformanceManagerRegistryImpl::GetInstance()
//       ->CreateBrowserChildProcessNodeForTesting(host());
class TestBrowserChildProcess final
    : public content::BrowserChildProcessHostDelegate {
 public:
  explicit TestBrowserChildProcess(content::ProcessType process_type);
  ~TestBrowserChildProcess() final;

  TestBrowserChildProcess(const TestBrowserChildProcess&) = delete;
  TestBrowserChildProcess& operator=(const TestBrowserChildProcess&) = delete;

  content::BrowserChildProcessHost* host() const { return host_.get(); }

  BrowserChildProcessHostId GetId() const;

  BrowserChildProcessHostProxy GetProxy() const;

  // Simulates a call to host()->Launch(), which causes a
  // BrowserChildProcessWatcher to create a ProcessNode for the host. This can
  // be used to create a ProcessNode in unit tests that don't satisfy all the
  // dependencies to launch a real utility procses.
  void SimulateLaunch(base::Process process = base::Process::Current());

  // Simulates that a process launched with SimulateLaunch() has exited. This
  // causes BrowserChildProcessWatcher to delete the ProcessNode for the host.
  void SimulateDisconnect();

 private:
  std::unique_ptr<content::BrowserChildProcessHost> host_;
  bool is_connected = false;
};

// Helper function for tests to delete the browser process node, which is
// created by default in BrowserChildProcessWatcher::Initialize(). Must not be
// called again if the node was already deleted.
void DeleteBrowserProcessNodeForTesting();

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_BROWSER_CHILD_PROCESS_H_
