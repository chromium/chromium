// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector_win.h"

#include <string.h>
#include <windows.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_types.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace named_mojo_ipc_server {
namespace {

constexpr base::TimeDelta kRetryConnectionTimeout = base::Seconds(3);

mojo::PlatformChannelServerEndpoint CreateServerEndpoint(
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  mojo::NamedPlatformChannel::Options options;
  options.server_name = server_name;
  options.enforce_uniqueness = false;
  // Create a named pipe owned by the current user (the LocalService account
  // (SID: S-1-5-19) when running in the network process) which is available to
  // all authenticated users.
  // presubmit: allow wstring
  std::wstring user_sid;
  if (!base::win::GetUserSidString(&user_sid)) {
    LOG(ERROR) << "Failed to get user SID string.";
    return mojo::PlatformChannelServerEndpoint();
  }
  options.security_descriptor = base::StringPrintf(
      L"O:%lsG:%lsD:(A;;GA;;;AU)", user_sid.c_str(), user_sid.c_str());

  mojo::NamedPlatformChannel channel(options);
  return channel.TakeServerEndpoint();
}

}  // namespace

NamedMojoServerEndpointConnectorWin::NamedMojoServerEndpointConnectorWin(
    const mojo::NamedPlatformChannel::ServerName& server_name,
    base::SequenceBound<Delegate> delegate)
    : NamedMojoServerEndpointConnector(server_name, std::move(delegate)),
      client_connected_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED) {
  DCHECK(delegate_);
}

NamedMojoServerEndpointConnectorWin::~NamedMojoServerEndpointConnectorWin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NamedMojoServerEndpointConnectorWin::Connect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!pending_named_pipe_handle_.IsValid());

  mojo::PlatformChannelServerEndpoint server_endpoint =
      CreateServerEndpoint(server_name_);
  if (!server_endpoint.is_valid()) {
    OnError();
    return;
  }

  delegate_.AsyncCall(&Delegate::OnServerEndpointCreated);

  pending_named_pipe_handle_ =
      server_endpoint.TakePlatformHandle().TakeHandle();
  // The |lpOverlapped| argument of ConnectNamedPipe() has the annotation of
  // [in, out, optional], so we reset the content before passing it in, just to
  // be safe.
  memset(&connect_overlapped_, 0, sizeof(connect_overlapped_));
  connect_overlapped_.hEvent = client_connected_event_.handle();
  BOOL ok =
      ConnectNamedPipe(pending_named_pipe_handle_.Get(), &connect_overlapped_);
  if (ok) {
    PLOG(ERROR) << "Unexpected success while waiting for pipe connection";
    OnError();
    return;
  }

  const DWORD err = GetLastError();
  switch (err) {
    case ERROR_PIPE_CONNECTED:
      // A client has connected before the server calls ConnectNamedPipe().
      OnReady();
      return;
    case ERROR_IO_PENDING:
      client_connection_watcher_.StartWatching(
          &client_connected_event_,
          base::BindOnce(
              &NamedMojoServerEndpointConnectorWin::OnConnectedEventSignaled,
              base::Unretained(this)),
          base::SequencedTaskRunner::GetCurrentDefault());
      return;
    default:
      PLOG(ERROR) << "Unexpected error: " << err;
      OnError();
      return;
  }
}

void NamedMojoServerEndpointConnectorWin::OnConnectedEventSignaled(
    base::WaitableEvent* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(&client_connected_event_, event);

  OnReady();
}

void NamedMojoServerEndpointConnectorWin::OnReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto info = std::make_unique<ConnectionInfo>();
  if (!GetNamedPipeClientProcessId(pending_named_pipe_handle_.Get(),
                                   &info->pid)) {
    PLOG(ERROR) << "Failed to get peer PID";
    OnError();
    return;
  }
  absl::optional<base::win::ScopedHandle> impersonation_token;
  if (ImpersonateNamedPipeClient(pending_named_pipe_handle_.Get())) {
    HANDLE token = nullptr;
    if (OpenThreadToken(GetCurrentThread(), TOKEN_ALL_ACCESS, TRUE, &token)) {
      info->impersonation_token = base::win::ScopedHandle(token);
    }
    RevertToSelf();
  }
  mojo::PlatformChannelEndpoint endpoint(
      mojo::PlatformHandle(std::move(pending_named_pipe_handle_)));
  if (!endpoint.is_valid()) {
    LOG(ERROR) << "Endpoint is invalid.";
    OnError();
    return;
  }
  ResetConnectionObjects();
  delegate_.AsyncCall(&Delegate::OnClientConnected)
      .WithArgs(std::move(endpoint), std::move(info));
  Connect();
}

void NamedMojoServerEndpointConnectorWin::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ResetConnectionObjects();
  retry_connect_timer_.Start(FROM_HERE, kRetryConnectionTimeout, this,
                             &NamedMojoServerEndpointConnectorWin::Connect);
}

// static
base::SequenceBound<NamedMojoServerEndpointConnector>
NamedMojoServerEndpointConnector::Create(
    scoped_refptr<base::SequencedTaskRunner> io_sequence,
    const mojo::NamedPlatformChannel::ServerName& server_name,
    base::SequenceBound<Delegate> delegate) {
  return base::SequenceBound<NamedMojoServerEndpointConnectorWin>(
      io_sequence, server_name, std::move(delegate));
}

void NamedMojoServerEndpointConnectorWin::ResetConnectionObjects() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  client_connection_watcher_.StopWatching();
  client_connected_event_.Reset();
  pending_named_pipe_handle_.Close();
}

bool NamedMojoServerEndpointConnectorWin::TryStart() {
  Connect();
  return true;
}

}  // namespace named_mojo_ipc_server
