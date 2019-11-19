// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/render_process_user_data.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"

namespace performance_manager {
namespace {

const void* const kRenderProcessUserDataKey = &kRenderProcessUserDataKey;

}  // namespace

RenderProcessUserData* RenderProcessUserData::first_ = nullptr;

RenderProcessUserData::RenderProcessUserData(
    content::RenderProcessHost* render_process_host)
    : host_(render_process_host) {
  host_->AddObserver(this);
  process_node_ = PerformanceManagerImpl::GetInstance()->CreateProcessNode(
      RenderProcessHostProxy(host_->GetID()));

  // Push this instance to the list.
  next_ = first_;
  if (next_)
    next_->prev_ = this;
  prev_ = nullptr;
  first_ = this;
}

RenderProcessUserData::~RenderProcessUserData() {
  PerformanceManagerImpl::GetInstance()->DeleteNode(std::move(process_node_));
  host_->RemoveObserver(this);

  if (first_ == this)
    first_ = next_;

  if (prev_) {
    DCHECK_EQ(prev_->next_, this);
    prev_->next_ = next_;
  }
  if (next_) {
    DCHECK_EQ(next_->prev_, this);
    next_->prev_ = prev_;
  }
}

// static
void RenderProcessUserData::DetachAndDestroyAll() {
  while (first_)
    first_->host_->RemoveUserData(kRenderProcessUserDataKey);
}

// static
RenderProcessUserData* RenderProcessUserData::GetForRenderProcessHost(
    content::RenderProcessHost* host) {
  return static_cast<RenderProcessUserData*>(
      host->GetUserData(kRenderProcessUserDataKey));
}

// static
RenderProcessUserData* RenderProcessUserData::GetOrCreateForRenderProcessHost(
    content::RenderProcessHost* host) {
  auto* raw_user_data = GetForRenderProcessHost(host);
  if (raw_user_data)
    return raw_user_data;
  std::unique_ptr<RenderProcessUserData> user_data =
      base::WrapUnique(new RenderProcessUserData(host));
  raw_user_data = user_data.get();
  host->SetUserData(kRenderProcessUserDataKey, std::move(user_data));
  return raw_user_data;
}

void RenderProcessUserData::RenderProcessReady(
    content::RenderProcessHost* host) {
  const base::Time launch_time =
#if defined(OS_ANDROID)
      // Process::CreationTime() is not available on Android. Since this
      // method is called immediately after the process is launched, the
      // process launch time can be approximated with the current time.
      base::Time::Now();
#else
      host->GetProcess().CreationTime();
#endif

  PerformanceManagerImpl::GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProcessNodeImpl::SetProcess,
                                base::Unretained(process_node_.get()),
                                host->GetProcess().Duplicate(), launch_time));
}

void RenderProcessUserData::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  PerformanceManagerImpl::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProcessNodeImpl::SetProcessExitStatus,
                     base::Unretained(process_node_.get()), info.exit_code));
}

void RenderProcessUserData::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  host->RemoveUserData(kRenderProcessUserDataKey);
}

}  // namespace performance_manager
