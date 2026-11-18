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
#include "base/check_is_test.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
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

struct BeforeConnectNamedPipeHook {
  base::Lock lock;
  base::OnceClosure hook GUARDED_BY(lock);
};

BeforeConnectNamedPipeHook& GetBeforeConnectNamedPipeHook() {
  static base::NoDestructor<BeforeConnectNamedPipeHook> instance;
  return *instance;
}

// Returns true iff `pipe`'s kernel object owner matches the current process's
// default owner SID or user SID.
//
// NPFS binds a pipe name's security descriptor (owner and DACL) to the first
// instance created under that name; subsequent CreateNamedPipeW calls open
// additional instances against the existing FCB and silently ignore
// lpSecurityAttributes.
//
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
  if (!sd || !sd->owner() || !token) {
    return false;
  }
  return *sd->owner() == token->Owner() || *sd->owner() == token->User();
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

  // Holds a previously connected (or client-closed) pipe instance open until
  // the next instance is actively listening. Because a client has already
  // attached to this instance, no new client can connect to it, while NPFS
  // keeps the pipe name's FCB and security descriptor claimed for as long as
  // this handle remains open.
  base::win::ScopedHandle name_keep_alive_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Signaled by ConnectNamedPipe() once |pending_named_pipe_handle_| is
  // connected to a client.
  base::WaitableEvent client_connected_event_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Object to allow ConnectNamedPipe() to run asynchronously.
  OVERLAPPED connect_overlapped_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::OneShotTimer retry_connect_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

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
  retry_connect_timer_.Stop();
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
  // descriptor (FCB) in NPFS. While `name_keep_alive_` is valid, this process
  // already holds an open instance so the FCB remains pinned and additional
  // instances can be opened concurrently. Otherwise this process holds no
  // instances, so enforce_uniqueness ensures CreateNamedPipeW passes
  // FILE_FLAG_FIRST_PIPE_INSTANCE and fails rather than attaching to another
  // process's pre-existing pipe.
  options.enforce_uniqueness = !name_keep_alive_.is_valid();
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
        << " has unexpected owner; refusing to use it";
    name_keep_alive_.Close();
    OnError();
    return;
  }
  logged_claim_error_ = false;

  base::OnceClosure before_connect_hook;
  {
    auto& instance = GetBeforeConnectNamedPipeHook();
    base::AutoLock auto_lock(instance.lock);
    before_connect_hook = std::move(instance.hook);
  }
  if (before_connect_hook) {
    CHECK_IS_TEST();
    std::move(before_connect_hook).Run();
  }

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
      delegate_.AsyncCall(&Delegate::OnServerEndpointCreated);
      OnReady();
      return;
    case ERROR_IO_PENDING:
      // The new instance is listening in NPFS and holds the pipe name open
      // from here on, so any previous keep-alive handle can be released.
      name_keep_alive_.Close();
      delegate_.AsyncCall(&Delegate::OnServerEndpointCreated);
      client_connection_watcher_.StartWatching(
          &client_connected_event_,
          base::BindOnce(
              &NamedMojoServerEndpointConnectorWin::OnConnectedEventSignaled,
              base::Unretained(this)),
          base::SequencedTaskRunner::GetCurrentDefault());
      return;
    case ERROR_NO_DATA:
      // A client connected and disconnected between CreateNamedPipeW() and
      // ConnectNamedPipe(). This instance can no longer accept a client, but
      // holding it in `name_keep_alive_` keeps the pipe name claimed in NPFS
      // (including on the very first instance) while we immediately re-arm.
      name_keep_alive_ = std::move(pending_named_pipe_handle_);
      ResetConnectionObjects();
      retry_connect_timer_.Start(FROM_HERE, base::TimeDelta(), this,
                                 &NamedMojoServerEndpointConnectorWin::Connect);
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

  // Duplicate a handle to the connected pipe instance into `name_keep_alive_`
  // until a new instance is listening. This keeps the pipe name claimed in
  // NPFS even if the delegate closes `endpoint` on its sequence before the
  // next Connect() finishes (or across an async retry).
  base::win::ScopedHandle keep_alive =
      endpoint.platform_handle().GetHandle().Duplicate();
  if (!keep_alive.is_valid()) {
    PLOG(ERROR) << "Failed to duplicate pipe handle";
    OnError();
    return;
  }
  name_keep_alive_ = std::move(keep_alive);

  const HANDLE pipe_handle = endpoint.platform_handle().GetHandle().Get();
  if (std::unique_ptr<ConnectionInfo> info =
          GetConnectionInfo(pipe_handle, options_.include_peer_process_info)) {
    delegate_.AsyncCall(&Delegate::OnClientConnected)
        .WithArgs(std::move(endpoint), std::move(info));
  }

  retry_connect_timer_.Start(FROM_HERE, base::TimeDelta(), this,
                             &NamedMojoServerEndpointConnectorWin::Connect);
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

base::ScopedClosureRunner SetBeforeConnectNamedPipeHookForTesting(
    base::OnceClosure hook) {
  CHECK_IS_TEST();
  {
    auto& instance = GetBeforeConnectNamedPipeHook();
    base::AutoLock auto_lock(instance.lock);
    instance.hook = std::move(hook);
  }
  return base::ScopedClosureRunner(base::BindOnce([] {
    auto& instance = GetBeforeConnectNamedPipeHook();
    base::AutoLock auto_lock(instance.lock);
    instance.hook.Reset();
  }));
}

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
