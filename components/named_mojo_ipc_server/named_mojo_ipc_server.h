// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_IPC_SERVER_H_
#define COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_IPC_SERVER_H_

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "components/named_mojo_ipc_server/ipc_server.h"
#include "components/named_mojo_ipc_server/named_mojo_message_pipe_server.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {
class IsolatedConnection;
}

namespace named_mojo_ipc_server {
// Template-less base class to keep implementations in the .cc file. For usage,
// see MojoIpcServer.
class NamedMojoIpcServerBase : public IpcServer {
 public:
  // Internal use only.
  struct PendingConnection;

  void StartServer() override;
  void StopServer() override;
  void Close(mojo::ReceiverId id) override;

  // Sets a callback to be run when an invitation is sent. Used by unit tests
  // only.
  void set_on_server_endpoint_created_callback_for_testing(
      const base::RepeatingClosure& callback) {
    message_pipe_server_.set_on_server_endpoint_created_callback_for_testing(
        callback);
  }

 protected:
  NamedMojoIpcServerBase(
      const EndpointOptions& options,
      base::RepeatingCallback<void*(const ConnectionInfo&)> impl_provider);
  ~NamedMojoIpcServerBase() override;

  void OnIpcDisconnected();

  virtual mojo::ReceiverId TrackMessagePipe(
      mojo::ScopedMessagePipeHandle message_pipe,
      void* impl,
      std::unique_ptr<ConnectionInfo> connection_info) = 0;

  virtual void UntrackMessagePipe(mojo::ReceiverId id) = 0;

  virtual void UntrackAllMessagePipes() = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::RepeatingClosure disconnect_handler_;

 private:
  void OnMessagePipeReady(mojo::ScopedMessagePipeHandle message_pipe,
                          std::unique_ptr<ConnectionInfo> connection_info,
                          void* context,
                          std::unique_ptr<mojo::IsolatedConnection> connection);

  using ActiveConnectionMap =
      base::flat_map<mojo::ReceiverId,
                     std::unique_ptr<mojo::IsolatedConnection>>;

  NamedMojoMessagePipeServer message_pipe_server_;

  // This is only populated if the server uses isolated connections.
  ActiveConnectionMap active_connections_;
};

// A helper that uses a NamedPlatformChannel to send out mojo invitations and
// maintains multiple concurrent IPCs. It keeps one outgoing invitation at a
// time and will send a new invitation whenever the previous one has been
// accepted by the client. Please see README.md for the example usage.
template <typename Interface>
class NamedMojoIpcServer final : public NamedMojoIpcServerBase {
 public:
  // options: Options to start the server endpoint.
  // impl_provider: A function that returns a pointer to an implementation,
  //     or nullptr if the connecting endpoint should be rejected.
  NamedMojoIpcServer(
      const EndpointOptions& options,
      base::RepeatingCallback<Interface*(const ConnectionInfo&)> impl_provider)
      : NamedMojoIpcServerBase(
            options,
            impl_provider.Then(base::BindRepeating([](Interface* impl) {
              // Opacify the type for the base class, which takes no template
              // parameters.
              return reinterpret_cast<void*>(impl);
            }))) {
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

  const ConnectionInfo& current_connection_info() const override {
    return *receiver_set_.current_context();
  }

  size_t GetNumberOfActiveConnectionsForTesting() const {
    return receiver_set_.size();
  }

 private:
  // NamedMojoIpcServerBase implementation.
  mojo::ReceiverId TrackMessagePipe(
      mojo::ScopedMessagePipeHandle message_pipe,
      void* impl,
      std::unique_ptr<ConnectionInfo> connection_info) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return receiver_set_.Add(
        reinterpret_cast<Interface*>(impl),
        mojo::PendingReceiver<Interface>(std::move(message_pipe)),
        std::move(connection_info));
  }

  void UntrackMessagePipe(mojo::ReceiverId id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    receiver_set_.Remove(id);
  }

  void UntrackAllMessagePipes() override { receiver_set_.Clear(); }

  mojo::ReceiverSet<Interface, std::unique_ptr<ConnectionInfo>> receiver_set_;
};

}  // namespace named_mojo_ipc_server

#endif  // COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_IPC_SERVER_H_
