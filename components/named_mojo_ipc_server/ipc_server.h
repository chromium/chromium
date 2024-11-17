// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAMED_MOJO_IPC_SERVER_IPC_SERVER_H_
#define COMPONENTS_NAMED_MOJO_IPC_SERVER_IPC_SERVER_H_

#include "base/functional/callback.h"
#include "base/process/process_handle.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace named_mojo_ipc_server {

// An interface for MojoIpcServer to allow mocking in unittests.
class IpcServer {
 public:
  virtual ~IpcServer() = default;

  // Starts sending out mojo invitations and accepting IPCs. No-op if the server
  // is already started.
  virtual void StartServer() = 0;

  // Stops sending out mojo invitations and accepting IPCs. No-op if the server
  // is already stopped.
  virtual void StopServer() = 0;

  // Close the receiver identified by |id| and disconnect the remote. No-op if
  // |id| doesn't exist or the receiver is already closed.
  virtual void Close(mojo::ReceiverId id) = 0;

  // Sets a callback to be invoked any time a receiver is disconnected. You may
  // find out which receiver is being disconnected by calling
  // |current_receiver()|.
  virtual void set_disconnect_handler(base::RepeatingClosure handler) = 0;

  // Call this method to learn which receiver has received the incoming IPC or
  // which receiver is being disconnected.
  virtual mojo::ReceiverId current_receiver() const = 0;

  // Call this method to learn the connection info.
  virtual const ConnectionInfo& current_connection_info() const = 0;
};

}  // namespace named_mojo_ipc_server

#endif  // COMPONENTS_NAMED_MOJO_IPC_SERVER_IPC_SERVER_H_
