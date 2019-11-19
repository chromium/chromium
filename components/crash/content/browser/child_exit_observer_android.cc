// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/child_exit_observer_android.h"

#include <unistd.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "components/crash/content/browser/crash_memory_metrics_collector_android.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"

using content::BrowserThread;

namespace crash_reporter {

namespace {

void PopulateTerminationInfoForRenderProcessHost(
    content::RenderProcessHost* rph,
    ChildExitObserver::TerminationInfo* info) {
  info->process_host_id = rph->GetID();
  info->pid = rph->GetProcess().Handle();
  info->process_type = content::PROCESS_TYPE_RENDERER;
  info->app_state = base::android::APPLICATION_STATE_UNKNOWN;
  info->renderer_has_visible_clients = rph->VisibleClientCount() > 0;
  info->renderer_was_subframe = rph->GetFrameDepth() > 0u;
  CrashMemoryMetricsCollector* collector =
      CrashMemoryMetricsCollector::GetFromRenderProcessHost(rph);

  // CrashMemoryMetircsCollector is created in chrome_content_browser_client,
  // and does not exist in non-chrome platforms such as android webview /
  // chromecast.
  if (collector) {
    // SharedMemory creation / Map() might fail.
    DCHECK(collector->MemoryMetrics());
    info->blink_oom_metrics = *collector->MemoryMetrics();
  }
}

void PopulateTerminationInfo(
    const content::ChildProcessTerminationInfo& content_info,
    ChildExitObserver::TerminationInfo* info) {
  info->binding_state = content_info.binding_state;
  info->was_killed_intentionally_by_browser =
      content_info.was_killed_intentionally_by_browser;
  info->remaining_process_with_strong_binding =
      content_info.remaining_process_with_strong_binding;
  info->remaining_process_with_moderate_binding =
      content_info.remaining_process_with_moderate_binding;
  info->remaining_process_with_waived_binding =
      content_info.remaining_process_with_waived_binding;
  info->best_effort_reverse_rank = content_info.best_effort_reverse_rank;
  info->was_oom_protected_status =
      content_info.status == base::TERMINATION_STATUS_OOM_PROTECTED;
  info->renderer_has_visible_clients =
      content_info.renderer_has_visible_clients;
  info->renderer_was_subframe = content_info.renderer_was_subframe;
}

ChildExitObserver* CreateSingletonInstance() {
  static base::NoDestructor<ChildExitObserver> s_instance;
  return s_instance.get();
}

ChildExitObserver* g_instance = nullptr;

}  // namespace

ChildExitObserver::TerminationInfo::TerminationInfo() = default;
ChildExitObserver::TerminationInfo::TerminationInfo(
    const TerminationInfo& other) = default;
ChildExitObserver::TerminationInfo& ChildExitObserver::TerminationInfo::
operator=(const TerminationInfo& other) = default;

// static
void ChildExitObserver::Create() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If this DCHECK fails in a unit test then a previously executing
  // test that makes use of ChildExitObserver forgot to create a
  // ShadowingAtExitManager.
  DCHECK(!g_instance);
  g_instance = CreateSingletonInstance();
}

// static
ChildExitObserver* ChildExitObserver::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

ChildExitObserver::ChildExitObserver() {
  BrowserChildProcessObserver::Add(this);
  scoped_observer_.Add(crashpad::CrashHandlerHost::Get());
}

ChildExitObserver::~ChildExitObserver() {
  BrowserChildProcessObserver::Remove(this);
}

void ChildExitObserver::RegisterClient(std::unique_ptr<Client> client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::AutoLock auto_lock(registered_clients_lock_);
  registered_clients_.push_back(std::move(client));
}

void ChildExitObserver::ChildReceivedCrashSignal(base::ProcessId pid,
                                                 int signo) {
  base::AutoLock lock(crash_signals_lock_);
  bool result =
      child_pid_to_crash_signal_.insert(std::make_pair(pid, signo)).second;
  DCHECK(result);
}

void ChildExitObserver::OnChildExit(TerminationInfo* info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  {
    base::AutoLock lock(crash_signals_lock_);
    auto pid_and_signal = child_pid_to_crash_signal_.find(info->pid);
    if (pid_and_signal != child_pid_to_crash_signal_.end()) {
      info->crash_signo = pid_and_signal->second;
      child_pid_to_crash_signal_.erase(pid_and_signal);
    }
  }

  std::vector<Client*> registered_clients_copy;
  {
    base::AutoLock auto_lock(registered_clients_lock_);
    for (auto& client : registered_clients_)
      registered_clients_copy.push_back(client.get());
  }
  for (auto* client : registered_clients_copy) {
    client->OnChildExit(*info);
  }
}

void ChildExitObserver::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TerminationInfo info;
  auto it = browser_child_process_info_.find(data.id);
  if (it != browser_child_process_info_.end()) {
    info = it->second;
    browser_child_process_info_.erase(it);
  } else {
    info.process_host_id = data.id;
    if (data.GetProcess().IsValid())
      info.pid = data.GetProcess().Pid();
    info.process_type = static_cast<content::ProcessType>(data.process_type);
    info.app_state = base::android::ApplicationStatusListener::GetState();
    info.normal_termination = true;
  }
  OnChildExit(&info);
}

void ChildExitObserver::BrowserChildProcessKilled(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& content_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!base::Contains(browser_child_process_info_, data.id));
  TerminationInfo info;
  info.process_host_id = data.id;
  info.pid = data.GetProcess().Pid();
  info.process_type = static_cast<content::ProcessType>(data.process_type);
  info.app_state = base::android::ApplicationStatusListener::GetState();
  info.normal_termination = content_info.clean_exit;
  PopulateTerminationInfo(content_info, &info);
  browser_child_process_info_.emplace(data.id, info);
  // Subsequent BrowserChildProcessHostDisconnected will call OnChildExit.
}

void ChildExitObserver::OnRenderProcessHostCreated(
    content::RenderProcessHost* rph) {
  process_host_id_to_pid_[rph->GetID()] = rph->GetProcess().Handle();
  rph_observers_.Add(rph);
}

void ChildExitObserver::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& termination_info) {
  OnRenderProcessHostGone(host, termination_info);
}

void ChildExitObserver::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  OnRenderProcessHostGone(host, base::nullopt);
}

void ChildExitObserver::OnRenderProcessHostGone(
    content::RenderProcessHost* host,
    base::Optional<content::ChildProcessTerminationInfo> termination_info) {
  const auto& iter = process_host_id_to_pid_.find(host->GetID());
  if (iter == process_host_id_to_pid_.end()) {
    return;
  }

  rph_observers_.Remove(host);
  TerminationInfo info;
  PopulateTerminationInfoForRenderProcessHost(host, &info);
  if (info.pid == base::kNullProcessHandle) {
    info.pid = iter->second;
  }

  if (termination_info.has_value()) {
    // We do not care about android fast shutdowns as it is a known case where
    // the renderer is intentionally killed when we are done with it.
    info.normal_termination = host->FastShutdownStarted();
    info.app_state = base::android::ApplicationStatusListener::GetState();
    PopulateTerminationInfo(*termination_info, &info);
  } else {
    info.normal_termination = true;
  }

  process_host_id_to_pid_.erase(iter);
  OnChildExit(&info);
}

}  // namespace crash_reporter
