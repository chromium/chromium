// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_SERVER_ENDPOINT_CONNECTOR_MAC_H_
#define COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_SERVER_ENDPOINT_CONNECTOR_MAC_H_

#include <mach/port.h>

#include <memory>

#include "base/mac/dispatch_source_mach.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"

namespace named_mojo_ipc_server {

// Mac implementation for MojoServerEndpointConnector.
class NamedMojoServerEndpointConnectorMac final
    : public NamedMojoServerEndpointConnector {
 public:
  explicit NamedMojoServerEndpointConnectorMac(
      base::SequenceBound<Delegate> delegate,
      const mojo::NamedPlatformChannel::ServerName& server_name);
  NamedMojoServerEndpointConnectorMac(
      const NamedMojoServerEndpointConnectorMac&) = delete;
  NamedMojoServerEndpointConnectorMac& operator=(
      const NamedMojoServerEndpointConnectorMac&) = delete;
  ~NamedMojoServerEndpointConnectorMac() override;

  // NamedMojoServerEndpointConnector implementation.
  void Connect(mojo::PlatformChannelServerEndpoint server_endpoint) override;

 private:
  // Called by |dispatch_source_| when a Mach message is ready to be received
  // on |endpoint_|.
  void HandleRequest();
  mach_port_t port();

  SEQUENCE_CHECKER(sequence_checker_);

  base::SequenceBound<Delegate> delegate_;
  const mojo::NamedPlatformChannel::ServerName server_name_;

  // These are only valid/non-null when there is a pending connection.
  mojo::PlatformChannelServerEndpoint pending_server_endpoint_;
  std::unique_ptr<base::DispatchSourceMach> dispatch_source_;
};

}  // namespace named_mojo_ipc_server
#endif  // COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_SERVER_ENDPOINT_CONNECTOR_MAC_H_
