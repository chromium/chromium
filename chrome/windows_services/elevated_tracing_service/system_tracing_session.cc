// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/elevated_tracing_service/system_tracing_session.h"

#include <windows.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/windows_handle_util.h"
#include "chrome/windows_services/elevated_tracing_service/service_integration.h"
#include "chrome/windows_services/service_program/crash_reporting.h"
#include "chrome/windows_services/service_program/get_calling_process.h"
#include "chrome/windows_services/service_program/scoped_client_impersonation.h"
#include "chrome/windows_services/service_program/user_crash_state.h"
#include "components/tracing/common/etw_system_data_source_win.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/tracing/public/cpp/traced_process.h"

namespace elevated_tracing_service {

namespace {

// Invoked on the main service thread.
void OnTracedProcessReceiver(base::ProcessId client_pid,
                             mojo::ScopedMessagePipeHandle pipe) {
  // Register the ETW data source the first time a handle is received.
  [[maybe_unused]] static const bool etw_data_source_registered = [client_pid] {
    tracing::EtwSystemDataSource::Register(client_pid);
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
HRESULT SystemTracingSession::AcceptInvitation(UINT32 endpoint_handle,
                                               DWORD* pid) {
  if (!pid || endpoint_handle == 0 ||
      base::win::IsPseudoHandle(base::win::Uint32ToHandle(endpoint_handle))) {
    return E_INVALIDARG;
  }
  *pid = base::kNullProcessId;

  if (session_) {
    return kErrorSessionAlreadyActive;
  }

  // Obtain a handle to the client's process. I_RpcOpenClientProcess implicitly
  // verifies the calling client's capabilities. Calling this while
  // impersonating the client can fail or mask the RPC context.
  base::Process client_process = GetCallingProcess(
      PROCESS_DUP_HANDLE | PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE);
  if (!client_process.IsValid()) {
    return kErrorCouldNotObtainCallingProcess;
  }

  // Impersonate the client to get per-user state related to crash handling.
  std::unique_ptr<UserCrashState> user_crash_state;
  if (ScopedClientImpersonation impersonate; impersonate.is_valid()) {
    user_crash_state = UserCrashState::Create(impersonate, client_process);
  } else {
    return impersonate.result();
  }

  const auto client_pid = client_process.Pid();
  if (client_pid == base::kNullProcessId) {
    return kErrorCouldNotGetCallingProcessPid;
  }

  // Duplicate the client's message pipe handle into the service process.
  HANDLE raw_handle = nullptr;
  if (!::DuplicateHandle(client_process.Handle(),
                         base::win::Uint32ToHandle(endpoint_handle),
                         ::GetCurrentProcess(), &raw_handle,
                         /*dwDesiredAccess=*/0,
                         /*bInheritHandle=*/FALSE, DUPLICATE_SAME_ACCESS)) {
    return E_INVALIDARG;  // Invalid endpoint handle.
  }
  mojo::PlatformChannelEndpoint endpoint(mojo::PlatformHandle(
      base::win::ScopedHandle(std::exchange(raw_handle, nullptr))));
  if (::GetFileType(endpoint.platform_handle().GetHandle().get()) !=
      FILE_TYPE_PIPE) {
    return E_INVALIDARG;  // Invalid endpoint handle.
  }

  // Drop down to only SYNCHRONIZE on the child process's handle. Pass ownership
  // of `client_process`, as DUPLICATE_CLOSE_SOURCE ensures that it will be
  // closed even on failure.
  HANDLE raw_client_handle = nullptr;
  if (!::DuplicateHandle(::GetCurrentProcess(), client_process.Release(),
                         ::GetCurrentProcess(), &raw_client_handle, SYNCHRONIZE,
                         /*bInheritHandle=*/FALSE, DUPLICATE_CLOSE_SOURCE)) {
    return kErrorCouldNotDuplicateCallingProcessHandle;
  }
  client_process = base::Process(std::exchange(raw_client_handle, nullptr));

  // This instance is ready to become the active session provided that there
  // isn't one already.
  auto session = SessionRegistry::RegisterActiveSession(
      CastToUnknown(), std::move(client_process));
  if (!session) {
    return kErrorSessionInProgress;
  }

  // Start the crash handler with a user-specific database.
  if (user_crash_state) {
    windows_services::StartCrashHandler(
        std::move(user_crash_state),
        /*directory_name=*/elevated_tracing_service::GetStorageDirBasename(),
        /*process_type=*/"elevated-tracing-service", task_runner_);
  }

  mojo::IncomingInvitation invitation = mojo::IncomingInvitation::Accept(
      std::move(endpoint), MOJO_ACCEPT_INVITATION_FLAG_ELEVATED);
  if (!invitation.is_valid()) {
    return E_INVALIDARG;
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&OnTracedProcessReceiver, client_pid,
                                invitation.ExtractMessagePipe(/*name=*/0)));

  session_ = std::move(session);
  *pid = ::GetCurrentProcessId();
  return S_OK;
}

SystemTracingSession::~SystemTracingSession() = default;

}  // namespace elevated_tracing_service
