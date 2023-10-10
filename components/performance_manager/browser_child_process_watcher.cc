// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/browser_child_process_watcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/common/process_type.h"

namespace performance_manager {

BrowserChildProcessWatcher::BrowserChildProcessWatcher() = default;

BrowserChildProcessWatcher::~BrowserChildProcessWatcher() {
  DCHECK(!browser_process_node_);
  DCHECK(tracked_process_nodes_.empty());
}

void BrowserChildProcessWatcher::Initialize() {
  DCHECK(!browser_process_node_);
  DCHECK(tracked_process_nodes_.empty());

  browser_process_node_ =
      PerformanceManagerImpl::CreateProcessNode(BrowserProcessNodeTag{});
  OnProcessLaunched(base::Process::Current(), /*metrics_name=*/{},
                    browser_process_node_.get());
  BrowserChildProcessObserver::Add(this);
}

void BrowserChildProcessWatcher::TearDown() {
  BrowserChildProcessObserver::Remove(this);

  std::vector<std::unique_ptr<NodeBase>> nodes;
  nodes.reserve(tracked_process_nodes_.size() + 1);

  if (browser_process_node_) {
    nodes.push_back(std::move(browser_process_node_));
  }
  for (auto& node : tracked_process_nodes_) {
    nodes.push_back(std::move(node.second));
  }
  tracked_process_nodes_.clear();

  PerformanceManagerImpl::BatchDeleteNodes(std::move(nodes));
}

ProcessNodeImpl* BrowserChildProcessWatcher::GetChildProcessNode(
    BrowserChildProcessHostId id) {
  const auto it = tracked_process_nodes_.find(id);
  return it != tracked_process_nodes_.end() ? it->second.get() : nullptr;
}

void BrowserChildProcessWatcher::CreateChildProcessNodeForTesting(
    const content::ChildProcessData& data) {
  CHECK(!base::Contains(tracked_process_nodes_,
                        BrowserChildProcessHostId(data.id)));
  BrowserChildProcessLaunchedAndConnected(data);
}

void BrowserChildProcessWatcher::DeleteChildProcessNodeForTesting(
    const content::ChildProcessData& data) {
  CHECK(base::Contains(tracked_process_nodes_,
                       BrowserChildProcessHostId(data.id)));
  BrowserChildProcessHostDisconnected(data);
}

void BrowserChildProcessWatcher::DeleteBrowserProcessNodeForTesting() {
  CHECK(browser_process_node_);
  NodeBase* node_base = browser_process_node_.release();
  PerformanceManagerImpl::DeleteNode(base::WrapUnique(node_base));
}

void BrowserChildProcessWatcher::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  if (data.process_type == content::PROCESS_TYPE_GPU ||
      data.process_type == content::PROCESS_TYPE_UTILITY) {
    std::unique_ptr<ProcessNodeImpl> process_node =
        PerformanceManagerImpl::CreateProcessNode(
            static_cast<content::ProcessType>(data.process_type),
            BrowserChildProcessHostProxy(BrowserChildProcessHostId(data.id)));
    OnProcessLaunched(data.GetProcess(), data.metrics_name, process_node.get());
    const auto [_, inserted] = tracked_process_nodes_.emplace(
        BrowserChildProcessHostId(data.id), std::move(process_node));
    CHECK(inserted);
  }
}

void BrowserChildProcessWatcher::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  if (data.process_type == content::PROCESS_TYPE_GPU ||
      data.process_type == content::PROCESS_TYPE_UTILITY) {
    auto it = tracked_process_nodes_.find(BrowserChildProcessHostId(data.id));
    // Apparently there are cases where a disconnect notification arrives here
    // either multiple times for the same process, or else before a
    // launch-and-connect notification arrives.
    // See https://crbug.com/942500.
    if (it != tracked_process_nodes_.end()) {
      PerformanceManagerImpl::DeleteNode(std::move(it->second));
      tracked_process_nodes_.erase(it);
    }
  }
}

void BrowserChildProcessWatcher::BrowserChildProcessCrashed(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  if (data.process_type == content::PROCESS_TYPE_GPU ||
      data.process_type == content::PROCESS_TYPE_UTILITY) {
    TrackedProcessExited(BrowserChildProcessHostId(data.id), info.exit_code);
  }
}

void BrowserChildProcessWatcher::BrowserChildProcessKilled(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  if (data.process_type == content::PROCESS_TYPE_GPU ||
      data.process_type == content::PROCESS_TYPE_UTILITY) {
    TrackedProcessExited(BrowserChildProcessHostId(data.id), info.exit_code);
  }
}

void BrowserChildProcessWatcher::TrackedProcessExited(
    BrowserChildProcessHostId id,
    int exit_code) {
  // It appears the exit code can be delivered either after the host is
  // disconnected, or perhaps before the HostConnected notification,
  // specifically on crash.
  ProcessNodeImpl* process_node = GetChildProcessNode(id);
  if (process_node) {
    DCHECK(PerformanceManagerImpl::IsAvailable());
    PerformanceManagerImpl::CallOnGraphImpl(
        FROM_HERE, base::BindOnce(&ProcessNodeImpl::SetProcessExitStatus,
                                  base::Unretained(process_node), exit_code));
  }
}

// static
void BrowserChildProcessWatcher::OnProcessLaunched(
    const base::Process& process,
    const std::string& metrics_name,
    ProcessNodeImpl* process_node) {
  DCHECK(PerformanceManagerImpl::IsAvailable());

  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(
          [](ProcessNodeImpl* process_node, base::Process process,
             base::TimeTicks launch_time, const std::string& metrics_name) {
            process_node->SetProcessMetricsName(metrics_name);
            process_node->SetProcess(std::move(process), launch_time);
          },
          base::Unretained(process_node), process.Duplicate(),
          base::TimeTicks::Now(), metrics_name));
}

}  // namespace performance_manager
