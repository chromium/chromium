// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/test_browser_child_process.h"

#include <utility>

#include "base/check.h"
#include "components/performance_manager/browser_child_process_watcher.h"
#include "components/performance_manager/performance_manager_registry_impl.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_host.h"

namespace performance_manager {

TestBrowserChildProcess::TestBrowserChildProcess(
    content::ProcessType process_type)
    : host_(content::BrowserChildProcessHost::Create(process_type, this)) {}

TestBrowserChildProcess::~TestBrowserChildProcess() {
  if (is_connected) {
    SimulateDisconnect();
  }
}

BrowserChildProcessHostId TestBrowserChildProcess::GetId() const {
  return BrowserChildProcessHostId(host_->GetData().id);
}

BrowserChildProcessHostProxy TestBrowserChildProcess::GetProxy() const {
  return BrowserChildProcessHostProxy::CreateForTesting(GetId());
}

void TestBrowserChildProcess::SimulateLaunch(base::Process process) {
  CHECK(!is_connected);

  PerformanceManagerRegistryImpl::GetInstance()
      ->GetBrowserChildProcessWatcherForTesting()
      .CreateChildProcessNodeForTesting(host_->GetData(), process);
  is_connected = true;
}

void TestBrowserChildProcess::SimulateDisconnect() {
  CHECK(is_connected);
  PerformanceManagerRegistryImpl::GetInstance()
      ->GetBrowserChildProcessWatcherForTesting()
      .DeleteChildProcessNodeForTesting(host_->GetData());
  is_connected = false;
}

void DeleteBrowserProcessNodeForTesting() {
  PerformanceManagerRegistryImpl::GetInstance()
      ->GetBrowserChildProcessWatcherForTesting()
      .DeleteBrowserProcessNodeForTesting();
}

}  // namespace performance_manager
