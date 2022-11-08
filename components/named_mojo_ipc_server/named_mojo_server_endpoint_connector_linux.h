// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_SERVER_ENDPOINT_CONNECTOR_LINUX_H_
#define COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_SERVER_ENDPOINT_CONNECTOR_LINUX_H_

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector.h"

namespace named_mojo_ipc_server {

// Linux implementation for MojoServerEndpointConnector.
class NamedMojoServerEndpointConnectorLinux final
    : public NamedMojoServerEndpointConnector {
 public:
  explicit NamedMojoServerEndpointConnectorLinux(
      base::SequenceBound<Delegate> delegate);
  NamedMojoServerEndpointConnectorLinux(
      const NamedMojoServerEndpointConnectorLinux&) = delete;
  NamedMojoServerEndpointConnectorLinux& operator=(
      const NamedMojoServerEndpointConnectorLinux&) = delete;
  ~NamedMojoServerEndpointConnectorLinux() override;

  // NamedMojoServerEndpointConnector implementation.
  void Connect(mojo::PlatformChannelServerEndpoint server_endpoint) override;

 private:
  void OnFileCanReadWithoutBlocking();

  SEQUENCE_CHECKER(sequence_checker_);

  base::SequenceBound<Delegate> delegate_ GUARDED_BY_CONTEXT(sequence_checker_);

  // These are only valid/non-null when there is a pending connection.
  // Note that `pending_server_endpoint_` must outlive
  // `read_watcher_controller_`; otherwise a bad file descriptor error will
  // occur at destruction.
  mojo::PlatformChannelServerEndpoint pending_server_endpoint_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<base::FileDescriptorWatcher::Controller>
      read_watcher_controller_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<NamedMojoServerEndpointConnectorLinux> weak_factory_{
      this};
};

}  // namespace named_mojo_ipc_server

#endif  // COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_SERVER_ENDPOINT_CONNECTOR_LINUX_H_
