// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/external_mojo/external_service_support/chromium_service.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/token.h"
#include "chromecast/external_mojo/external_service_support/external_connector.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service_control.mojom.h"

namespace chromecast {
namespace external_service_support {

namespace {

void OnStartCallback(
    ExternalConnector* connector,
    mojo::PendingReceiver<service_manager::mojom::Connector> connector_receiver,
    mojo::PendingAssociatedReceiver<service_manager::mojom::ServiceControl>
        control_receiver) {
  DCHECK(connector);
  if (connector_receiver.is_valid()) {
    connector->SendChromiumConnectorRequest(connector_receiver.PassPipe());
  }
}

}  // namespace

ChromiumServiceWrapper::ChromiumServiceWrapper(
    ExternalConnector* connector,
    mojo::Remote<service_manager::mojom::Service> service_remote,
    std::unique_ptr<service_manager::Service> chromium_service,
    const std::string& service_name)
    : service_remote_(std::move(service_remote)),
      chromium_service_(std::move(chromium_service)) {
  DCHECK(connector);
  DCHECK(chromium_service_);

  connector->RegisterService(service_name,
                             service_receiver_.BindNewPipeAndPassRemote());
}

ChromiumServiceWrapper::~ChromiumServiceWrapper() = default;

void ChromiumServiceWrapper::OnBindInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  chromium_service_->OnBindInterface(
      service_manager::BindSourceInfo(
          service_manager::Identity("unique", base::Token::CreateRandom(),
                                    base::Token::CreateRandom(),
                                    base::Token::CreateRandom()),
          service_manager::CapabilitySet()),
      interface_name, std::move(interface_pipe));
}

mojo::PendingReceiver<service_manager::mojom::Service>
CreateChromiumServiceReceiver(
    ExternalConnector* connector,
    mojo::Remote<service_manager::mojom::Service>* service_remote,
    service_manager::Identity identity) {
  DCHECK(connector);

  if (identity.name().empty()) {
    identity = service_manager::Identity(
        "unspecified", base::Token::CreateRandom(), base::Token::CreateRandom(),
        base::Token::CreateRandom());
  }

  auto receiver = service_remote->BindNewPipeAndPassReceiver();
  (*service_remote)
      ->OnStart(identity, base::BindOnce(&OnStartCallback, connector));
  return receiver;
}

std::unique_ptr<service_manager::Connector> CreateChromiumConnector(
    ExternalConnector* connector) {
  mojo::MessagePipe pipe;
  connector->SendChromiumConnectorRequest(std::move(pipe.handle1));
  return std::make_unique<service_manager::Connector>(
      mojo::Remote<service_manager::mojom::Connector>(
          mojo::PendingRemote<service_manager::mojom::Connector>(
              std::move(pipe.handle0), 0)));
}

}  // namespace external_service_support
}  // namespace chromecast
