// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector.h"

#include <windows.h>

#include <string.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/process.h"
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
#include "base/win/access_token.h"
#include "base/win/scoped_handle.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "base/win/windows_types.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_channel_server.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace named_mojo_ipc_server {
namespace {

constexpr base::TimeDelta kRetryConnectionTimeout = base::Seconds(3);

// Returns true iff `pipe`'s kernel object owner matches the current process's
// default owner SID or user SID. NPFS binds a pipe name's security descriptor
// (and owner) to the first instance created under that name; subsequent
// CreateNamedPipeW calls open additional instances against the existing FCB
// and silently ignore lpSecurityAttributes.
// Both token->Owner() and token->User() are accepted because on an elevated
// administrator token, the default owner SID (`O:ow` / `token->Owner()`) is
// `BUILTIN\Administrators`, whereas callers that explicitly specify the user
// SID in the SDDL (`O:<user_sid>`, e.g. `ChromotingHostServicesServer`) set
// the owner to `token->User()`.
bool IsPipeNameOwnedByThisProcess(HANDLE pipe) {
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromHandle(
          pipe, base::win::SecurityObjectType::kFile,
          OWNER_SECURITY_INFORMATION);
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess();
  return sd && sd->owner() && token &&
         (*sd->owner() == token->Owner() || *sd->owner() == token->User());
}

std::unique_ptr<ConnectionInfo> GetConnectionInfo(
    HANDLE pipe_handle,
    bool include_peer_process_info) {
  auto info = std::make_unique<ConnectionInfo>();
  if (!GetNamedPipeClientProcessId(pipe_handle, &info->pid)) {
    PLOG(ERROR) << "Failed to get peer PID";
    return nullptr;
  }
  // GetNamedPipeClientSessionId is not available on all Windows SKUs (e.g.
  // Xbox), so load it dynamically to avoid a static import that would prevent
  // the DLL from loading on those platforms.
  using GetNamedPipeClientSessionIdFn = BOOL(WINAPI*)(HANDLE, PULONG);
  static const auto get_session_id =
      reinterpret_cast<GetNamedPipeClientSessionIdFn>(::GetProcAddress(
          ::GetModuleHandle(L"kernel32.dll"), "GetNamedPipeClientSessionId"));
  if (get_session_id) {
    ULONG peer_session_id;
    if (!get_session_id(pipe_handle, &peer_session_id)) {
      PLOG(ERROR) << "Failed to get peer session ID";
      return nullptr;
    }
    info->session_id = peer_session_id;
  }
  if (include_peer_process_info) {
    info->process = base::Process::OpenWithAccess(
        info->pid, PROCESS_QUERY_LIMITED_INFORMATION);
    if (!info->process.IsValid()) {
      PLOG(ERROR) << "Failed to open peer process " << info->pid;
      return nullptr;
    }
  }
  return info;
}

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

  base::OneShotTimer retry_connect_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // True once this connector has created the first pipe instance and verified
  // that the pipe name is owned by this process.
  bool owns_pipe_name_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // True if a pipe claim failure or ownership mismatch has already been logged,
  // to avoid spamming the log on every retry timer tick while squatted.
  bool logged_claim_error_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
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
  if (pending_named_pipe_handle_.is_valid()) {
    CancelIoEx(pending_named_pipe_handle_.Get(), &connect_overlapped_);

    // `CancelIoEx` is asynchronous, so the following code blocks the
    // destruction of `NamedMojoServerEndpointConnectorWin` on the kernel
    // writing the final status into `connect_overlapped_` to avoid a
    // use-after-free.
    DWORD bytes = 0;
    GetOverlappedResult(pending_named_pipe_handle_.Get(), &connect_overlapped_,
                        &bytes, TRUE);
  }
}

void NamedMojoServerEndpointConnectorWin::Connect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!pending_named_pipe_handle_.is_valid());

  mojo::NamedPlatformChannel::Options options;
  options.server_name = options_.server_name;
  options.security_descriptor = options_.security_descriptor;
  options.pipe_name_type = options_.pipe_name_type;

  // The first instance created under a pipe name establishes its security
  // descriptor (FCB) in NPFS. If another process squatted on the name before
  // this server started, enforce_uniqueness ensures CreateNamedPipeW fails with
  // ERROR_ACCESS_DENIED or ERROR_PIPE_BUSY rather than attaching to the
  // squatter's pipe instance and silently discarding this server's DACL.
  // Once the first instance has been claimed, subsequent instances allow
  // multiple clients to connect concurrently.
  options.enforce_uniqueness = !owns_pipe_name_;
  options.max_clients = PIPE_UNLIMITED_INSTANCES;

  mojo::PlatformChannelServerEndpoint server_endpoint =
      mojo::NamedPlatformChannel(options).TakeServerEndpoint();
  if (!server_endpoint.is_valid()) {
    PLOG_IF(ERROR, !std::exchange(logged_claim_error_, true))
        << "Failed to claim pipe name " << options.server_name
        << "; another process may already own it";
    OnError();
    return;
  }

  pending_named_pipe_handle_ =
      server_endpoint.TakePlatformHandle().TakeHandle();
  if (!IsPipeNameOwnedByThisProcess(pending_named_pipe_handle_.Get())) {
    LOG_IF(ERROR, !std::exchange(logged_claim_error_, true))
        << "Pipe " << options.server_name
        << " is not owned by this process; refusing to use it";
    owns_pipe_name_ = false;
    OnError();
    return;
  }
  logged_claim_error_ = false;
  owns_pipe_name_ = true;

  delegate_.AsyncCall(&Delegate::OnServerEndpointCreated);

  // The |lpOverlapped| argument of ConnectNamedPipe() has the annotation of
  // [in, out, optional], so we reset the content before passing it in, just to
  // be safe.
  UNSAFE_TODO(memset(&connect_overlapped_, 0, sizeof(connect_overlapped_)));
  connect_overlapped_.hEvent = client_connected_event_.handle();
  BOOL ok = mojo::ConnectNamedPipeWithoutCompletionPacket(
      pending_named_pipe_handle_.Get(), &connect_overlapped_);
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

  mojo::PlatformChannelEndpoint endpoint(
      mojo::PlatformHandle(std::move(pending_named_pipe_handle_)));
  ResetConnectionObjects();

  if (!endpoint.is_valid()) {
    LOG(ERROR) << "Endpoint is invalid.";
    OnError();
    return;
  }

  const HANDLE pipe_handle = endpoint.platform_handle().GetHandle().Get();
  std::unique_ptr<ConnectionInfo> info =
      GetConnectionInfo(pipe_handle, options_.include_peer_process_info);

  // Duplicate a handle to the connected pipe instance to hold it open across
  // Connect(). This keeps the pipe name claimed in NPFS even if the delegate
  // closes `endpoint` on its sequence before Connect() finishes, while allowing
  // OnClientConnected to be posted before re-arming so callback ordering is
  // strictly preserved across connections.
  HANDLE keep_alive_raw = INVALID_HANDLE_VALUE;
  ::DuplicateHandle(::GetCurrentProcess(), pipe_handle, ::GetCurrentProcess(),
                    &keep_alive_raw, 0, FALSE, DUPLICATE_SAME_ACCESS);
  base::win::ScopedHandle keep_alive(keep_alive_raw);

  if (info) {
    delegate_.AsyncCall(&Delegate::OnClientConnected)
        .WithArgs(std::move(endpoint), std::move(info));
  }

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
