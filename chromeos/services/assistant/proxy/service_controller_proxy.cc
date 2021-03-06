// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/proxy/service_controller_proxy.h"

#include "ash/constants/ash_features.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "chromeos/services/assistant/public/cpp/migration/assistant_manager_service_delegate.h"
#include "chromeos/services/assistant/public/cpp/migration/libassistant_v1_api.h"
#include "chromeos/services/libassistant/libassistant_service.h"
#include "chromeos/services/libassistant/public/mojom/service_controller.mojom-forward.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {
namespace assistant {

// TODO(b/171748795): Most of the work that is done here right now (especially
// the work related to starting Libassistant) should be moved to the mojom
// service.

namespace {

using chromeos::libassistant::mojom::AuthenticationTokenPtr;
using chromeos::libassistant::mojom::ServiceState;

}  // namespace

ServiceControllerProxy::ServiceControllerProxy(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    mojo::PendingRemote<chromeos::libassistant::mojom::ServiceController>
        client)
    : url_loader_factory_(network::SharedURLLoaderFactory::Create(
          std::move(pending_url_loader_factory))),
      service_controller_remote_(std::move(client)) {}

ServiceControllerProxy::~ServiceControllerProxy() = default;

void ServiceControllerProxy::Start(
    chromeos::libassistant::mojom::BootupConfigPtr bootup_config) {
  // The mojom service will create the |AssistantManager|.
  service_controller_remote_->Initialize(std::move(bootup_config),
                                         BindURLLoaderFactory());
  service_controller_remote_->Start();
}

void ServiceControllerProxy::Stop() {
  service_controller_remote_->Stop();
}

void ServiceControllerProxy::ResetAllDataAndStop() {
  service_controller_remote_->ResetAllDataAndStop();
}

void ServiceControllerProxy::AddAndFireStateObserver(
    mojo::PendingRemote<chromeos::libassistant::mojom::StateObserver>
        observer) {
  service_controller_remote_->AddAndFireStateObserver(std::move(observer));
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
ServiceControllerProxy::BindURLLoaderFactory() {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;
  url_loader_factory_->Clone(pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

}  // namespace assistant
}  // namespace chromeos
