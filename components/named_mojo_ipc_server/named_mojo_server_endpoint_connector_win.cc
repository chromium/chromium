// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector.h"

#include <windows.h>

#include <string.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace named_mojo_ipc_server {
namespace {

constexpr base::TimeDelta kRetryConnectionTimeout = base::Seconds(3);

class NamedMojoServerEndpointConnectorWin final
    : public NamedMojoServerEndpointConnector {
 public:
  explicit NamedMojoServerEndpointConnectorWin(
      const EndpointOptions& options,
      base::SequenceBound<Delegate> delegate);
  NamedMojoServerEndpointConnectorWin(
      const NamedMojoServerEndpointConnectorWin&) = delete;
  NamedMojoServerEndpointConnectorWin& operator=(
      const NamedMojoServerEndpointConnectorWin&) = delete;
  ~NamedMojoServerEndpointConnectorWin() override;

 private:
  void OnConnectedEventSignaled(base::WaitableEvent* event);

  void Connect();
  void OnReady();
  void OnError();

  void ResetConnectionObjects();

  // Overrides for NamedMojoServerEndpointConnector.
  bool TryStart() override;

  base::WaitableEventWatcher client_connection_watcher_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Non-null when there is a pending connection.
  base::win::ScopedHandle pending_named_pipe_handle_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Signaled by ConnectNamedPipe() once |pending_named_pipe_handle_| is
  // connected to a client.
  base::WaitableEvent client_connected_event_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Object to allow ConnectNamedPipe() to run asynchronously.
  OVERLAPPED connect_overlapped_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::OneShotTimer retry_connect_timer_;
};

NamedMojoServerEndpointConnectorWin::NamedMojoServerEndpointConnectorWin(
    const EndpointOptions& options,
    base::SequenceBound<Delegate> delegate)
    : NamedMojoServerEndpointConnector(options, std::move(delegate)),
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

  mojo::NamedPlatformChannel::Options options;
  options.server_name = options_.server_name;
  options.security_descriptor = options_.security_descriptor;
  // Must be set to false to allow multiple clients to connect.
  options.enforce_uniqueness = false;
  mojo::PlatformChannelServerEndpoint server_endpoint =
      mojo::NamedPlatformChannel(options).TakeServerEndpoint();
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

}  // namespace

// static
base::SequenceBound<NamedMojoServerEndpointConnector>
NamedMojoServerEndpointConnector::Create(
    scoped_refptr<base::SequencedTaskRunner> io_sequence,
    const EndpointOptions& options,
    base::SequenceBound<Delegate> delegate) {
  return base::SequenceBound<NamedMojoServerEndpointConnectorWin>(
      io_sequence, options, std::move(delegate));
}

}  // namespace named_mojo_ipc_server
