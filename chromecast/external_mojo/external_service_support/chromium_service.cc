// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/external_mojo/external_service_support/chromium_service.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/token.h"
#include "chromecast/external_mojo/external_service_support/external_connector.h"
#include "mojo/public/cpp/bindings/interface_request.h"
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
    service_manager::mojom::ServicePtr service_ptr,
    std::unique_ptr<service_manager::Service> chromium_service,
    const std::string& service_name)
    : service_ptr_(std::move(service_ptr)),
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

service_manager::mojom::ServiceRequest CreateChromiumServiceRequest(
    ExternalConnector* connector,
    service_manager::mojom::ServicePtr* service_ptr,
    service_manager::Identity identity) {
  DCHECK(connector);

  if (identity.name().empty()) {
    identity = service_manager::Identity(
        "unspecified", base::Token::CreateRandom(), base::Token::CreateRandom(),
        base::Token::CreateRandom());
  }

  auto request = mojo::MakeRequest(service_ptr);
  (*service_ptr)
      ->OnStart(identity, base::BindOnce(&OnStartCallback, connector));
  return request;
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
