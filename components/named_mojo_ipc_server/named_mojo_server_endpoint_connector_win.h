// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_SERVER_ENDPOINT_CONNECTOR_WIN_H_
#define COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_SERVER_ENDPOINT_CONNECTOR_WIN_H_

#include <windows.h>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"
#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector.h"

namespace named_mojo_ipc_server {

// Windows implementation for NamedMojoServerEndpointConnector.
class NamedMojoServerEndpointConnectorWin final
    : public NamedMojoServerEndpointConnector {
 public:
  explicit NamedMojoServerEndpointConnectorWin(
      base::SequenceBound<Delegate> delegate);
  NamedMojoServerEndpointConnectorWin(
      const NamedMojoServerEndpointConnectorWin&) = delete;
  NamedMojoServerEndpointConnectorWin& operator=(
      const NamedMojoServerEndpointConnectorWin&) = delete;
  ~NamedMojoServerEndpointConnectorWin() override;

  void Connect(mojo::PlatformChannelServerEndpoint server_endpoint) override;

 private:
  void OnConnectedEventSignaled(base::WaitableEvent* event);

  void OnReady();
  void OnError();

  void ResetConnectionObjects();

  SEQUENCE_CHECKER(sequence_checker_);

  base::SequenceBound<Delegate> delegate_ GUARDED_BY_CONTEXT(sequence_checker_);
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
};

}  // namespace named_mojo_ipc_server

#endif  // COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_SERVER_ENDPOINT_CONNECTOR_WIN_H_
