// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/devtools_process_observer.h"

#include "base/process/process.h"
#include "components/ui_devtools/tracing_agent.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/common/process_type.h"

DevtoolsProcessObserver::DevtoolsProcessObserver(
    ui_devtools::TracingAgent* agent)
    : tracing_agent_(agent) {
  DCHECK(tracing_agent_);
  BrowserChildProcessObserver::Add(this);
}

DevtoolsProcessObserver::~DevtoolsProcessObserver() {
  BrowserChildProcessObserver::Remove(this);
}

void DevtoolsProcessObserver::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  if (data.process_type == content::PROCESS_TYPE_GPU)
    tracing_agent_->set_gpu_pid(data.GetProcess().Pid());
}

void DevtoolsProcessObserver::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  if (data.process_type == content::PROCESS_TYPE_GPU)
    tracing_agent_->set_gpu_pid(base::kNullProcessId);
}

void DevtoolsProcessObserver::BrowserChildProcessCrashed(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  if (data.process_type == content::PROCESS_TYPE_GPU)
    tracing_agent_->set_gpu_pid(base::kNullProcessId);
}

void DevtoolsProcessObserver::BrowserChildProcessKilled(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  if (data.process_type == content::PROCESS_TYPE_GPU)
    tracing_agent_->set_gpu_pid(base::kNullProcessId);
}
