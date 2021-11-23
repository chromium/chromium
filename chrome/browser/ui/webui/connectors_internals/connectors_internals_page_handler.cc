// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/connectors_internals/connectors_internals_page_handler.h"

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/connectors_internals/connectors_internals.mojom.h"
#include "chrome/browser/ui/webui/connectors_internals/zero_trust_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)

namespace enterprise_connectors {

namespace {

connectors_internals::mojom::KeyManagerInitializedValue
IsKeyManagerInitialized() {
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
  auto* key_manager = g_browser_process->browser_policy_connector()
                          ->chrome_browser_cloud_management_controller()
                          ->GetDeviceTrustKeyManager();
  if (key_manager) {
    return key_manager->IsFullyInitialized()
               ? connectors_internals::mojom::KeyManagerInitializedValue::
                     KEY_LOADED
               : connectors_internals::mojom::KeyManagerInitializedValue::
                     NO_KEY;
  }
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
  return connectors_internals::mojom::KeyManagerInitializedValue::UNSUPPORTED;
}

}  // namespace

ConnectorsInternalsPageHandler::ConnectorsInternalsPageHandler(
    mojo::PendingReceiver<connectors_internals::mojom::PageHandler> receiver,
    Profile* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {
  DCHECK(profile_);
}

ConnectorsInternalsPageHandler::~ConnectorsInternalsPageHandler() = default;

void ConnectorsInternalsPageHandler::GetZeroTrustState(
    GetZeroTrustStateCallback callback) {
  auto* device_trust_service =
      DeviceTrustServiceFactory::GetForProfile(profile_);

  // The factory will not return a service if the profile is off-the-record.
  if (!device_trust_service) {
    auto state = connectors_internals::mojom::ZeroTrustState::New(
        false,
        connectors_internals::mojom::KeyManagerInitializedValue::UNSUPPORTED,
        base::flat_map<std::string, std::string>());
    std::move(callback).Run(std::move(state));
    return;
  }

  // Since this page is used for debugging purposes, show the signals regardless
  // of the policy value (i.e. even if service->IsEnabled is false).
  device_trust_service->GetSignals(
      base::BindOnce(&ConnectorsInternalsPageHandler::OnSignalsCollected,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     device_trust_service->IsEnabled()));
}

void ConnectorsInternalsPageHandler::OnSignalsCollected(
    GetZeroTrustStateCallback callback,
    bool is_device_trust_enabled,
    std::unique_ptr<SignalsType> signals) {
  auto state = connectors_internals::mojom::ZeroTrustState::New(
      is_device_trust_enabled, IsKeyManagerInitialized(),
      utils::SignalsToMap(std::move(signals)));
  std::move(callback).Run(std::move(state));
}

}  // namespace enterprise_connectors
