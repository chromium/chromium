// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_IPC_SERVER_H_
#define COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_IPC_SERVER_H_

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/timer.h"
#include "components/named_mojo_ipc_server/ipc_server.h"
#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace mojo {
class IsolatedConnection;
}

namespace named_mojo_ipc_server {
// Template-less base class to keep implementations in the .cc file. For usage,
// see MojoIpcServer.
class NamedMojoIpcServerBase : public IpcServer {
 public:
  using IsTrustedMojoEndpointCallback =
      base::RepeatingCallback<bool(base::ProcessId)>;

  // Internal use only.
  struct PendingConnection;

  void StartServer() override;
  void StopServer() override;
  void Close(mojo::ReceiverId id) override;

  // Sets a callback to be run when an invitation is sent. Used by unit tests
  // only.
  void set_on_invitation_sent_callback_for_testing(
      const base::RepeatingClosure& callback) {
    on_invitation_sent_callback_for_testing_ = callback;
  }

  size_t GetNumberOfActiveConnectionsForTesting() const {
    return active_connections_.size();
  }

 protected:
  NamedMojoIpcServerBase(
      const mojo::NamedPlatformChannel::ServerName& server_name,
      IsTrustedMojoEndpointCallback is_trusted_endpoint_callback);
  ~NamedMojoIpcServerBase() override;

  void SendInvitation();

  void OnIpcDisconnected();

  virtual mojo::ReceiverId TrackMessagePipe(
      mojo::ScopedMessagePipeHandle message_pipe,
      base::ProcessId peer_pid) = 0;

  virtual void UntrackMessagePipe(mojo::ReceiverId id) = 0;

  virtual void UntrackAllMessagePipes() = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::RepeatingClosure disconnect_handler_;

 private:
  // Forwards callbacks from a NamedMojoServerEndpointConnector to a
  // NamedMojoIpcServerBase. This allows the server to create a SequenceBound
  // interface to post callbacks from the IO sequence to the main sequence.
  class DelegateProxy : public NamedMojoServerEndpointConnector::Delegate {
   public:
    explicit DelegateProxy(base::WeakPtr<NamedMojoIpcServerBase> server);
    ~DelegateProxy() override;

    // Overrides for NamedMojoServerEndpointConnector::Delegate
    void OnServerEndpointConnected(
        std::unique_ptr<mojo::IsolatedConnection> connection,
        mojo::ScopedMessagePipeHandle message_pipe,
        base::ProcessId peer_pid) override;
    void OnServerEndpointConnectionFailed() override;

   private:
    base::WeakPtr<NamedMojoIpcServerBase> server_;
  };

  void OnServerEndpointCreated(mojo::PlatformChannelServerEndpoint endpoint);

  void OnServerEndpointConnected(
      std::unique_ptr<mojo::IsolatedConnection> connection,
      mojo::ScopedMessagePipeHandle message_pipe,
      base::ProcessId peer_pid);
  void OnServerEndpointConnectionFailed();

  using ActiveConnectionMap =
      base::flat_map<mojo::ReceiverId,
                     std::unique_ptr<mojo::IsolatedConnection>>;

  mojo::NamedPlatformChannel::ServerName server_name_;
  IsTrustedMojoEndpointCallback is_trusted_endpoint_callback_;

  bool server_started_ = false;

  // A task runner to run blocking jobs.
  scoped_refptr<base::SequencedTaskRunner> io_sequence_;

  base::SequenceBound<NamedMojoServerEndpointConnector> endpoint_connector_;
  ActiveConnectionMap active_connections_;
  base::OneShotTimer resent_invitation_on_error_timer_;

  base::RepeatingClosure on_invitation_sent_callback_for_testing_ =
      base::DoNothing();

  base::WeakPtrFactory<NamedMojoIpcServerBase> weak_factory_{this};
};

// A helper that uses a NamedPlatformChannel to send out mojo invitations and
// maintains multiple concurrent IPCs. It keeps one outgoing invitation at a
// time and will send a new invitation whenever the previous one has been
// accepted by the client.
//
// Example usage:
//
//   class MyInterfaceImpl: public mojom::MyInterface {
//     void Start() {
//       server_.set_disconnect_handler(
//           base::BindRepeating(&MyInterfaceImpl::OnDisconnected, this));
//       server_.StartServer();
//     }

//     void OnDisconnected() {
//       LOG(INFO) << "Receiver disconnected: " << server_.current_receiver();
//     }

//     // mojom::MyInterface Implementation.
//     void DoWork() override {
//       // Do something...

//       // If you want to close the connection:
//       server_.Close(server_.current_receiver());
//     }

//     static bool IsTrustedMojoEndpoint(base::ProcessId caller_pid) {
//       // Verify the calling process...
//       return true;
//     }

//     MojoIpcServer<mojom::MyInterface> server_{"my_server_name", this,
//         base::BindRepeating(&MyInterfaceImpl::IsTrustedMojoEndpoint)};
//   };
// Note: In unittests base::test:TaskEnvironment run until idle after
// NamedMojoIpcServer is shutdown. Otherwise, memory may leak. E.g:
//  void MyTestFixture::TearDown() {
//    ipc_server_->StopServer();
//    task_environment_.RunUntilIdle();
//  }
template <typename Interface>
class NamedMojoIpcServer final : public NamedMojoIpcServerBase {
 public:
  // server_name: The server name to start the NamedPlatformChannel.
  // is_trusted_endpoint_callback: A predicate which returns true if the process
  // referred to by the caller PID is a trusted mojo endpoint.
  NamedMojoIpcServer(const mojo::NamedPlatformChannel::ServerName& server_name,
                     Interface* interface_impl,
                     IsTrustedMojoEndpointCallback is_trusted_endpoint_callback)
      : NamedMojoIpcServerBase(server_name,
                               std::move(is_trusted_endpoint_callback)),
        interface_impl_(interface_impl) {
    receiver_set_.set_disconnect_handler(base::BindRepeating(
        &NamedMojoIpcServer::OnIpcDisconnected, base::Unretained(this)));
  }

  ~NamedMojoIpcServer() override = default;

  NamedMojoIpcServer(const NamedMojoIpcServer&) = delete;
  NamedMojoIpcServer& operator=(const NamedMojoIpcServer&) = delete;

  void set_disconnect_handler(base::RepeatingClosure handler) override {
    disconnect_handler_ = handler;
  }

  mojo::ReceiverId current_receiver() const override {
    return receiver_set_.current_receiver();
  }

  base::ProcessId current_peer_pid() const override {
    return receiver_set_.current_context();
  }

 private:
  // NamedMojoIpcServerBase implementation.
  mojo::ReceiverId TrackMessagePipe(mojo::ScopedMessagePipeHandle message_pipe,
                                    base::ProcessId peer_pid) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return receiver_set_.Add(
        interface_impl_,
        mojo::PendingReceiver<Interface>(std::move(message_pipe)), peer_pid);
  }

  void UntrackMessagePipe(mojo::ReceiverId id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    receiver_set_.Remove(id);
  }

  void UntrackAllMessagePipes() override { receiver_set_.Clear(); }

  raw_ptr<Interface> interface_impl_;
  mojo::ReceiverSet<Interface, base::ProcessId> receiver_set_;
};

}  // namespace named_mojo_ipc_server

#endif  // COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_IPC_SERVER_H_
