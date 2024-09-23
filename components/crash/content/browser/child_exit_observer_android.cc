// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/child_exit_observer_android.h"

#include <unistd.h>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "components/crash/content/browser/crash_memory_metrics_collector_android.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"

using content::BrowserThread;

namespace crash_reporter {

namespace {

void PopulateTerminationInfo(
    const content::ChildProcessTerminationInfo& content_info,
    ChildExitObserver::TerminationInfo* info) {
  info->binding_state = content_info.binding_state;
  info->threw_exception_during_init = content_info.threw_exception_during_init;
  info->was_killed_intentionally_by_browser =
      content_info.was_killed_intentionally_by_browser;
  info->renderer_has_visible_clients =
      content_info.renderer_has_visible_clients;
  info->renderer_was_subframe = content_info.renderer_was_subframe;
}

}  // namespace

ChildExitObserver::TerminationInfo::TerminationInfo() = default;
ChildExitObserver::TerminationInfo::TerminationInfo(
    const TerminationInfo& other) = default;
ChildExitObserver::TerminationInfo& ChildExitObserver::TerminationInfo::
operator=(const TerminationInfo& other) = default;

ChildExitObserver::ChildExitObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserChildProcessObserver::Add(this);
  scoped_crash_handler_host_observation_.Observe(
      crashpad::CrashHandlerHost::Get());
}

ChildExitObserver::~ChildExitObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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

void ChildExitObserver::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
  // The child process pid isn't available when process is gone, keep a mapping
  // between process_host_id and pid, so we can find it later.
  process_host_id_to_pid_[host->GetID()] = host->GetProcess().Handle();
  if (!render_process_host_observation_.IsObservingSource(host)) {
    render_process_host_observation_.AddObservation(host);
  }
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

void ChildExitObserver::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  ProcessRenderProcessHostLifetimeEndEvent(host, &info);
}

void ChildExitObserver::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  ProcessRenderProcessHostLifetimeEndEvent(host, nullptr);
  render_process_host_observation_.RemoveObservation(host);
}

void ChildExitObserver::ProcessRenderProcessHostLifetimeEndEvent(
    content::RenderProcessHost* rph,
    const content::ChildProcessTerminationInfo* content_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TerminationInfo info;
  info.process_host_id = rph->GetID();
  info.pid = rph->GetProcess().Handle();
  info.process_type = content::PROCESS_TYPE_RENDERER;
  info.app_state = base::android::APPLICATION_STATE_UNKNOWN;
  info.renderer_has_visible_clients = rph->VisibleClientCount() > 0;
  info.renderer_was_subframe = rph->GetFrameDepth() > 0u;
  CrashMemoryMetricsCollector* collector =
      CrashMemoryMetricsCollector::GetFromRenderProcessHost(rph);

  // CrashMemoryMetircsCollector is created in chrome_content_browser_client,
  // and does not exist in non-chrome platforms such as android webview /
  // chromecast.
  if (collector) {
    // SharedMemory creation / Map() might fail.
    DCHECK(collector->MemoryMetrics());
    info.blink_oom_metrics = *collector->MemoryMetrics();
  }

  if (content_info) {
    // We do not care about android fast shutdowns as it is a known case where
    // the renderer is intentionally killed when we are done with it.
    info.normal_termination = rph->FastShutdownStarted();
    info.renderer_shutdown_requested = rph->ShutdownRequested();
    info.app_state = base::android::ApplicationStatusListener::GetState();
    PopulateTerminationInfo(*content_info, &info);
  } else {
    // No |content_info| is provided when the renderer process is cleanly
    // shutdown.
    info.normal_termination = true;
    info.renderer_shutdown_requested = rph->ShutdownRequested();
  }

  const auto& iter = process_host_id_to_pid_.find(rph->GetID());
  if (iter == process_host_id_to_pid_.end()) {
    return;
  }
  if (info.pid == base::kNullProcessHandle) {
    info.pid = iter->second;
  }
  process_host_id_to_pid_.erase(iter);
  OnChildExit(&info);
}

}  // namespace crash_reporter
