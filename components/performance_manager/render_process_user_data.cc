// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/render_process_user_data.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"

namespace performance_manager {
namespace {

const void* const kRenderProcessUserDataKey = &kRenderProcessUserDataKey;

}  // namespace

RenderProcessUserData::RenderProcessUserData(
    content::RenderProcessHost* render_process_host)
    : host_(render_process_host) {
  host_->AddObserver(this);
  base::TaskPriority initial_priority = host_->IsSpare()
                                            ? base::TaskPriority::LOWEST
                                            : base::TaskPriority::HIGHEST;
  process_node_ = PerformanceManagerImpl::CreateProcessNode(
      RenderProcessHostProxy(RenderProcessHostId(host_->GetID())),
      initial_priority);
}

RenderProcessUserData::~RenderProcessUserData() {
  PerformanceManagerImpl::DeleteNode(std::move(process_node_));
  host_->RemoveObserver(this);

  if (destruction_observer_) {
    destruction_observer_->OnRenderProcessUserDataDestroying(host_);
  }
}

// static
const void* RenderProcessUserData::UserDataKey() {
  return kRenderProcessUserDataKey;
}

// static
RenderProcessUserData* RenderProcessUserData::GetForRenderProcessHost(
    content::RenderProcessHost* host) {
  return static_cast<RenderProcessUserData*>(
      host->GetUserData(kRenderProcessUserDataKey));
}

void RenderProcessUserData::SetDestructionObserver(
    DestructionObserver* destruction_observer) {
  DCHECK(!destruction_observer || !destruction_observer_);
  destruction_observer_ = destruction_observer;
}

void RenderProcessUserData::OnProcessLaunched() {
  DCHECK(host_->GetProcess().IsValid());
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&ProcessNodeImpl::SetProcess,
                                base::Unretained(process_node_.get()),
                                host_->GetProcess().Duplicate(),
                                /* launch_time=*/base::TimeTicks::Now()));
}

// static
RenderProcessUserData* RenderProcessUserData::CreateForRenderProcessHost(
    content::RenderProcessHost* host) {
  DCHECK(!GetForRenderProcessHost(host));
  std::unique_ptr<RenderProcessUserData> user_data =
      base::WrapUnique(new RenderProcessUserData(host));
  RenderProcessUserData* raw_user_data = user_data.get();
  host->SetUserData(kRenderProcessUserDataKey, std::move(user_data));
  return raw_user_data;
}

void RenderProcessUserData::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&ProcessNodeImpl::SetProcessExitStatus,
                     base::Unretained(process_node_.get()), info.exit_code));
}

void RenderProcessUserData::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  host->RemoveUserData(kRenderProcessUserDataKey);
}

}  // namespace performance_manager
