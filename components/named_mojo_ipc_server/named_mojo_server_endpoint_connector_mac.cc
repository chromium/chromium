// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <bsm/libbsm.h>
#include <mach/kern_return.h>
#include <mach/message.h>
#include <mach/port.h>

#include <memory>

#include "base/apple/dispatch_source_mach.h"
#include "base/apple/mach_logging.h"
#include "base/apple/scoped_mach_port.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/mac/scoped_mach_msg_destroy.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"

namespace named_mojo_ipc_server {
namespace {

// Mac implementation for MojoServerEndpointConnector.
class NamedMojoServerEndpointConnectorMac final
    : public NamedMojoServerEndpointConnector {
 public:
  explicit NamedMojoServerEndpointConnectorMac(
      const EndpointOptions& options,
      base::SequenceBound<Delegate> delegate);
  NamedMojoServerEndpointConnectorMac(
      const NamedMojoServerEndpointConnectorMac&) = delete;
  NamedMojoServerEndpointConnectorMac& operator=(
      const NamedMojoServerEndpointConnectorMac&) = delete;
  ~NamedMojoServerEndpointConnectorMac() override;

 private:
  // Called by |dispatch_source_| on an arbitrary thread when a Mach message is
  // ready to be received on |endpoint_|.
  void HandleRequest();
  mach_port_t port();

  // Overrides for NamedMojoServerEndpointConnector.
  bool TryStart() override;

  // Note: |server_endpoint_| must outlive |dispatch_source_|.
  mojo::PlatformChannelServerEndpoint server_endpoint_;
  std::unique_ptr<base::apple::DispatchSourceMach> dispatch_source_;
};

NamedMojoServerEndpointConnectorMac::NamedMojoServerEndpointConnectorMac(
    const EndpointOptions& options,
    base::SequenceBound<NamedMojoServerEndpointConnector::Delegate> delegate)
    : NamedMojoServerEndpointConnector(options, std::move(delegate)) {}

NamedMojoServerEndpointConnectorMac::~NamedMojoServerEndpointConnectorMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NamedMojoServerEndpointConnectorMac::HandleRequest() {
  struct : mach_msg_base_t {
    mach_msg_audit_trailer_t trailer;
  } request{};
  request.header.msgh_size = sizeof(request);
  request.header.msgh_local_port = port();
  kern_return_t kr = mach_msg(
      &request.header,
      MACH_RCV_MSG | MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
          MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT),
      0, sizeof(request), port(), MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_msg";
    return;
  }

  base::ScopedMachMsgDestroy scoped_message(&request.header);

  if (request.header.msgh_size != sizeof(mach_msg_base_t)) {
    LOG(ERROR) << "Invalid message size.";
    return;
  }

  auto info = std::make_unique<ConnectionInfo>();
  info->pid = audit_token_to_pid(request.trailer.msgh_audit);
  info->audit_token = request.trailer.msgh_audit;

  mojo::PlatformChannelEndpoint remote_endpoint(mojo::PlatformHandle(
      base::apple::ScopedMachSendRight(request.header.msgh_remote_port)));
  if (!remote_endpoint.is_valid()) {
    LOG(ERROR) << "Endpoint is invalid.";
    return;
  }

  scoped_message.Disarm();
  delegate_.AsyncCall(&Delegate::OnClientConnected)
      .WithArgs(std::move(remote_endpoint), std::move(info));
}

mach_port_t NamedMojoServerEndpointConnectorMac::port() {
  DCHECK(server_endpoint_.is_valid());
  return server_endpoint_.platform_handle().GetMachReceiveRight().get();
}

bool NamedMojoServerEndpointConnectorMac::TryStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::PlatformChannelServerEndpoint server_endpoint =
      mojo::NamedPlatformChannel({options_.server_name}).TakeServerEndpoint();
  if (!server_endpoint.is_valid() ||
      !server_endpoint.platform_handle().is_valid_mach_receive()) {
    return false;
  }

  server_endpoint_ = std::move(server_endpoint);
  dispatch_source_ = std::make_unique<base::apple::DispatchSourceMach>(
      options_.server_name.c_str(), port(), ^{
        HandleRequest();
      });
  dispatch_source_->Resume();
  delegate_.AsyncCall(&Delegate::OnServerEndpointCreated);
  return true;
}

}  // namespace

// static
base::SequenceBound<NamedMojoServerEndpointConnector>
NamedMojoServerEndpointConnector::Create(
    scoped_refptr<base::SequencedTaskRunner> io_sequence,
    const EndpointOptions& options,
    base::SequenceBound<Delegate> delegate) {
  return base::SequenceBound<NamedMojoServerEndpointConnectorMac>(
      io_sequence, options, std::move(delegate));
}

}  // namespace named_mojo_ipc_server
