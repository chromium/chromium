// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/external_mojo/external_service_support/external_connector_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/external_mojo/external_service_support/external_service.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"

namespace chromecast {
namespace external_service_support {

namespace {
constexpr base::TimeDelta kConnectRetryDelay =
    base::TimeDelta::FromMilliseconds(500);
}  // namespace

// static
void ExternalConnector::Connect(
    const std::string& broker_path,
    base::OnceCallback<void(std::unique_ptr<ExternalConnector>)> callback) {
  DCHECK(callback);

  mojo::PlatformChannelEndpoint endpoint =
      mojo::NamedPlatformChannel::ConnectToServer(broker_path);
  if (!endpoint.is_valid()) {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ExternalConnector::Connect, broker_path,
                       std::move(callback)),
        kConnectRetryDelay);
    return;
  }

  auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
  auto pipe = invitation.ExtractMessagePipe(0);
  if (!pipe) {
    LOG(ERROR) << "Invalid message pipe";
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ExternalConnector::Connect, broker_path,
                       std::move(callback)),
        kConnectRetryDelay);
    return;
  }
  mojo::Remote<external_mojo::mojom::ExternalConnector> connector(
      mojo::PendingRemote<external_mojo::mojom::ExternalConnector>(
          std::move(pipe), 0));
  std::move(callback).Run(
      std::make_unique<ExternalConnectorImpl>(std::move(connector)));
}

ExternalConnectorImpl::ExternalConnectorImpl(
    mojo::Remote<external_mojo::mojom::ExternalConnector> connector)
    : connector_(std::move(connector)) {
  connector_.set_disconnect_handler(base::BindOnce(
      &ExternalConnectorImpl::OnMojoDisconnect, base::Unretained(this)));
}

ExternalConnectorImpl::ExternalConnectorImpl(
    mojo::PendingRemote<external_mojo::mojom::ExternalConnector> unbound_state)
    : unbound_state_(std::move(unbound_state)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ExternalConnectorImpl::~ExternalConnectorImpl() = default;

void ExternalConnectorImpl::SetConnectionErrorCallback(
    base::OnceClosure callback) {
  connection_error_callback_ = std::move(callback);
}

void ExternalConnectorImpl::RegisterService(const std::string& service_name,
                                            ExternalService* service) {
  RegisterService(service_name, service->GetReceiver());
}

void ExternalConnectorImpl::RegisterService(
    const std::string& service_name,
    mojo::PendingRemote<external_mojo::mojom::ExternalService> service_remote) {
  if (BindConnectorIfNecessary()) {
    connector_->RegisterServiceInstance(service_name,
                                        std::move(service_remote));
  }
}

void ExternalConnectorImpl::QueryServiceList(
    base::OnceCallback<void(
        std::vector<chromecast::external_mojo::mojom::ExternalServiceInfoPtr>)>
        callback) {
  if (BindConnectorIfNecessary()) {
    connector_->QueryServiceList(std::move(callback));
  }
}

void ExternalConnectorImpl::BindInterface(
    const std::string& service_name,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (BindConnectorIfNecessary()) {
    connector_->BindInterface(service_name, interface_name,
                              std::move(interface_pipe));
  }
}

std::unique_ptr<ExternalConnector> ExternalConnectorImpl::Clone() {
  mojo::PendingRemote<external_mojo::mojom::ExternalConnector> connector_remote;
  auto receiver = connector_remote.InitWithNewPipeAndPassReceiver();
  if (BindConnectorIfNecessary()) {
    connector_->Clone(std::move(receiver));
  }
  return std::make_unique<ExternalConnectorImpl>(std::move(connector_remote));
}

void ExternalConnectorImpl::SendChromiumConnectorRequest(
    mojo::ScopedMessagePipeHandle request) {
  if (BindConnectorIfNecessary()) {
    connector_->BindChromiumConnector(std::move(request));
  }
}

void ExternalConnectorImpl::OnMojoDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connector_.reset();
  if (connection_error_callback_) {
    std::move(connection_error_callback_).Run();
  }
}

bool ExternalConnectorImpl::BindConnectorIfNecessary() {
  // Bind the message pipe and SequenceChecker to the current thread the first
  // time it is used to connect.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (connector_.is_bound()) {
    return true;
  }

  if (!unbound_state_.is_valid()) {
    // OnMojoDisconnect was already called, but |this| was not destroyed.
    return false;
  }

  connector_.Bind(std::move(unbound_state_));
  connector_.set_disconnect_handler(base::BindOnce(
      &ExternalConnectorImpl::OnMojoDisconnect, base::Unretained(this)));

  return true;
}

}  // namespace external_service_support
}  // namespace chromecast
