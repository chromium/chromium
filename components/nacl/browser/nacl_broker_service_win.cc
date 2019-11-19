// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/browser/nacl_broker_service_win.h"

#include "components/nacl/browser/nacl_process_host.h"
#include "components/nacl/common/nacl_process_type.h"
#include "content/public/browser/browser_child_process_host_iterator.h"

using content::BrowserChildProcessHostIterator;

namespace nacl {

NaClBrokerService* NaClBrokerService::GetInstance() {
  return base::Singleton<NaClBrokerService>::get();
}

NaClBrokerService::NaClBrokerService()
    : loaders_running_(0) {
}

NaClBrokerService::~NaClBrokerService() {
}

bool NaClBrokerService::StartBroker() {
  NaClBrokerHost* broker_host = new NaClBrokerHost;
  if (!broker_host->Init()) {
    delete broker_host;
    return false;
  }
  return true;
}

bool NaClBrokerService::LaunchLoader(
    base::WeakPtr<nacl::NaClProcessHost> nacl_process_host,
    mojo::ScopedMessagePipeHandle ipc_channel_handle) {
  // Add task to the list
  int launch_id = ++next_launch_id_;
  pending_launches_[launch_id] = nacl_process_host;
  NaClBrokerHost* broker_host = GetBrokerHost();

  if (!broker_host) {
    if (!StartBroker())
      return false;
    broker_host = GetBrokerHost();
  }
  broker_host->LaunchLoader(launch_id, std::move(ipc_channel_handle));

  return true;
}

void NaClBrokerService::OnLoaderLaunched(int launch_id, base::Process process) {
  PendingLaunchesMap::iterator it = pending_launches_.find(launch_id);
  if (pending_launches_.end() == it) {
    NOTREACHED();
    return;
  }

  NaClProcessHost* client = it->second.get();
  if (client)
    client->OnProcessLaunchedByBroker(std::move(process));
  pending_launches_.erase(it);
  ++loaders_running_;
}

void NaClBrokerService::OnLoaderDied() {
  DCHECK(loaders_running_ > 0);
  --loaders_running_;
  // Stop the broker only if there are no loaders running or being launched.
  NaClBrokerHost* broker_host = GetBrokerHost();
  if (loaders_running_ + pending_launches_.size() == 0 && broker_host != NULL) {
    broker_host->StopBroker();
  }
}

bool NaClBrokerService::LaunchDebugExceptionHandler(
    base::WeakPtr<NaClProcessHost> nacl_process_host,
    int32_t pid,
    base::ProcessHandle process_handle,
    const std::string& startup_info) {
  pending_debuggers_[pid] = nacl_process_host;
  NaClBrokerHost* broker_host = GetBrokerHost();
  if (!broker_host)
    return false;
  return broker_host->LaunchDebugExceptionHandler(pid, process_handle,
                                                  startup_info);
}

void NaClBrokerService::OnDebugExceptionHandlerLaunched(int32_t pid,
                                                        bool success) {
  PendingDebugExceptionHandlersMap::iterator it = pending_debuggers_.find(pid);
  if (pending_debuggers_.end() == it)
    NOTREACHED();

  NaClProcessHost* client = it->second.get();
  if (client)
    client->OnDebugExceptionHandlerLaunchedByBroker(success);
  pending_debuggers_.erase(it);
}

NaClBrokerHost* NaClBrokerService::GetBrokerHost() {
  BrowserChildProcessHostIterator iter(PROCESS_TYPE_NACL_BROKER);
  while (!iter.Done()) {
    NaClBrokerHost* host = static_cast<NaClBrokerHost*>(iter.GetDelegate());
    if (!host->IsTerminating())
      return host;
    ++iter;
  }
  return NULL;
}

}  // namespace nacl
