// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/elevated_tracing_service/system_tracing_session.h"

#include <windows.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/win_util.h"
#include "chrome/windows_services/service_program/get_calling_process.h"
#include "chrome/windows_services/service_program/scoped_client_impersonation.h"
#include "components/tracing/common/etw_system_data_source_win.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/tracing/public/cpp/traced_process.h"

namespace elevated_tracing_service {

namespace {

// Invoked on the main service thread.
void OnTracedProcessReceiver(mojo::ScopedMessagePipeHandle pipe) {
  // Register the ETW data source the first time a handle is received.
  [[maybe_unused]] static const bool etw_data_source_registered = [] {
    tracing::EtwSystemDataSource::Register();
    return true;
  }();

  // Drop any previous connection before accepting the new one.
  tracing::TracedProcess::ResetTracedProcessReceiver();
  tracing::TracedProcess::OnTracedProcessRequest(
      mojo::PendingReceiver<tracing::mojom::TracedProcess>(std::move(pipe)));
}

}  // namespace

SystemTracingSession::SystemTracingSession() = default;

HRESULT SystemTracingSession::RuntimeClassInitialize(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_ = std::move(task_runner);
  return S_OK;
}

// Invoked on an arbitrary RPC thread.
HRESULT SystemTracingSession::AcceptInvitation(const wchar_t* server_name,
                                               DWORD* pid) {
  if (!pid || !server_name || !*server_name) {
    return E_INVALIDARG;
  }
  *pid = base::kNullProcessId;

  if (session_) {
    return kErrorSessionAlreadyActive;
  }

  // Impersonate the client to get a handle to the client's process.
  base::Process client_process;
  if (ScopedClientImpersonation impersonate; impersonate.is_valid()) {
    client_process = GetCallingProcess();
    if (!client_process.IsValid()) {
      return kErrorCouldNotObtainCallingProcess;
    }
  } else {
    return impersonate.result();
  }

  // Get a handle to the client process with appropriate rights.
  if (const auto client_pid = client_process.Pid();
      client_pid != base::kNullProcessId) {
    if (auto dup = base::Process::OpenWithAccess(client_pid, SYNCHRONIZE);
        dup.IsValid()) {
      std::swap(client_process, dup);
    } else {
      return kErrorCouldNotOpenCallingProcess;
    }
  } else {
    return kErrorCouldNotGetCallingProcessPid;
  }

  // This instance is ready to become the active session provided that there
  // isn't one already.
  auto session = SessionRegistry::RegisterActiveSession(
      CastToUnknown(), std::move(client_process));
  if (!session) {
    return kErrorSessionInProgress;
  }

  auto endpoint = mojo::NamedPlatformChannel::ConnectToServer(server_name);
  if (!endpoint.is_valid()) {
    return E_INVALIDARG;  // Invalid channel name.
  }
  mojo::IncomingInvitation invitation = mojo::IncomingInvitation::Accept(
      std::move(endpoint), MOJO_ACCEPT_INVITATION_FLAG_ELEVATED);
  if (!invitation.is_valid()) {
    return E_INVALIDARG;
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&OnTracedProcessReceiver,
                                invitation.ExtractMessagePipe(/*name=*/0)));

  session_ = std::move(session);
  *pid = ::GetCurrentProcessId();
  return S_OK;
}

SystemTracingSession::~SystemTracingSession() = default;

}  // namespace elevated_tracing_service
