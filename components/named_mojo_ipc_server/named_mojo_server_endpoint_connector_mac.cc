// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector_mac.h"

#include <bsm/libbsm.h>
#include <mach/kern_return.h>
#include <mach/message.h>
#include <mach/port.h>

#include "base/functional/bind.h"
#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_msg_destroy.h"
#include "base/mac/scoped_mach_port.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"

namespace named_mojo_ipc_server {

NamedMojoServerEndpointConnectorMac::NamedMojoServerEndpointConnectorMac(
    base::SequenceBound<NamedMojoServerEndpointConnector::Delegate> delegate,
    const mojo::NamedPlatformChannel::ServerName& server_name)
    : delegate_(std::move(delegate)), server_name_(server_name) {
  DCHECK(delegate_);
}

NamedMojoServerEndpointConnectorMac::~NamedMojoServerEndpointConnectorMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_server_endpoint_.reset();
  dispatch_source_.reset();
}

void NamedMojoServerEndpointConnectorMac::Connect(
    mojo::PlatformChannelServerEndpoint server_endpoint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(server_endpoint.is_valid());
  DCHECK(server_endpoint.platform_handle().is_valid_mach_receive());
  pending_server_endpoint_ = std::move(server_endpoint);

  dispatch_source_ = std::make_unique<base::DispatchSourceMach>(
      server_name_.c_str(), port(), ^{
        HandleRequest();
      });
  dispatch_source_->Resume();
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
  dispatch_source_.release();
  pending_server_endpoint_.reset();

  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_msg";
    delegate_.AsyncCall(&Delegate::OnServerEndpointConnectionFailed);
    return;
  }

  base::ScopedMachMsgDestroy scoped_message(&request.header);

  if (request.header.msgh_size != sizeof(mach_msg_base_t)) {
    LOG(ERROR) << "Invalid message size.";
    delegate_.AsyncCall(&Delegate::OnServerEndpointConnectionFailed);
    return;
  }

  pid_t sender_pid = audit_token_to_pid(request.trailer.msgh_audit);

  mojo::PlatformChannelEndpoint remote_endpoint(mojo::PlatformHandle(
      base::mac::ScopedMachSendRight(request.header.msgh_remote_port)));
  if (!remote_endpoint.is_valid()) {
    LOG(ERROR) << "Endpoint is invalid.";
    delegate_.AsyncCall(&Delegate::OnServerEndpointConnectionFailed);
    return;
  }

  scoped_message.Disarm();
  delegate_.AsyncCall(&Delegate::OnServerEndpointConnected)
      .WithArgs(std::move(remote_endpoint), sender_pid);
}

mach_port_t NamedMojoServerEndpointConnectorMac::port() {
  DCHECK(pending_server_endpoint_.is_valid());
  return pending_server_endpoint_.platform_handle().GetMachReceiveRight().get();
}

// static
base::SequenceBound<NamedMojoServerEndpointConnector>
NamedMojoServerEndpointConnector::Create(
    base::SequenceBound<Delegate> delegate,
    scoped_refptr<base::SequencedTaskRunner> io_sequence,
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  return base::SequenceBound<NamedMojoServerEndpointConnectorMac>(
      io_sequence, std::move(delegate), server_name);
}

}  // namespace named_mojo_ipc_server
