// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_MESSAGE_PIPE_SERVER_H_
#define COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_MESSAGE_PIPE_SERVER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/timer.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {
class IsolatedConnection;
}

namespace named_mojo_ipc_server {
// A helper that uses a NamedPlatformChannel to send out mojo invitations and
// maintains multiple concurrent mojo message pipes. It keeps one outgoing
// invitation at a time and will send a new invitation whenever the previous one
// has been accepted by the client. This is similar to NamedMojoIpcServer, but
// it returns the mojo message pipe without binding it to an implementation.
class NamedMojoMessagePipeServer {
 public:
  struct ValidationResult {
    // The client connection will be dropped and OnMessagePipeReady() won't be
    // called, if `is_valid` is false.
    bool is_valid;
    // The value will be passed to OnMessagePipeReady() as-is, if `is_valid` is
    // true.
    raw_ptr<void> context;
  };

  // Called when a client has just connected to the server. The callback should
  // check whether the connection is valid and return the ValidationResult.
  using Validator =
      base::RepeatingCallback<ValidationResult(const ConnectionInfo&)>;

  // Called once the message pipe is open. This happens after Validator is
  // called, with `is_valid` set to true in ValidationResult.
  // Note that IsolatedConnection is nullable. If it is non-null, the
  // implementation should store it to keep the connection alive, otherwise, the
  // implementation can ignore it.
  using OnMessagePipeReady =
      base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle,
                                   std::unique_ptr<ConnectionInfo>,
                                   /* context= */ void*,
                                   std::unique_ptr<mojo::IsolatedConnection>)>;

  // Internal use only.
  struct PendingConnection;

  // Callbacks won't be called once the instance is destroyed.
  NamedMojoMessagePipeServer(const EndpointOptions& options,
                             Validator validator,
                             OnMessagePipeReady on_message_pipe_ready);

  ~NamedMojoMessagePipeServer();

  void StartServer();
  void StopServer();

  // Sets a callback to be run when an invitation is sent. Used by unit tests
  // only.
  void set_on_server_endpoint_created_callback_for_testing(
      const base::RepeatingClosure& callback) {
    on_server_endpoint_created_callback_for_testing_ = callback;
  }

 private:
  class DelegateProxy;

  void OnEndpointConnectorStarted(
      base::SequenceBound<NamedMojoServerEndpointConnector> endpoint_connector);
  void OnClientConnected(mojo::PlatformChannelEndpoint endpoint,
                         std::unique_ptr<ConnectionInfo> info);
  void OnServerEndpointCreated();

  SEQUENCE_CHECKER(sequence_checker_);

  EndpointOptions options_;
  Validator validator_;
  OnMessagePipeReady on_message_pipe_ready_;

  bool server_started_ = false;

  // A task runner to run blocking jobs.
  scoped_refptr<base::SequencedTaskRunner> io_sequence_;

  base::SequenceBound<NamedMojoServerEndpointConnector> endpoint_connector_;

  base::OneShotTimer restart_endpoint_timer_;

  base::RepeatingClosure on_server_endpoint_created_callback_for_testing_;

  base::WeakPtrFactory<NamedMojoMessagePipeServer> weak_factory_{this};
};

}  // namespace named_mojo_ipc_server

#endif  // COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_MESSAGE_PIPE_SERVER_H_
