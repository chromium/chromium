// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/hotspot_config/in_process_instance.h"

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/services/hotspot_config/cros_hotspot_config.h"
#include "components/device_event_log/device_event_log.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::hotspot_config {

namespace {

mojom::CrosHotspotConfig* g_hotspot_config_override = nullptr;

mojo::ReceiverSet<mojom::CrosHotspotConfig>& GetOverrideReceivers() {
  static base::NoDestructor<mojo::ReceiverSet<mojom::CrosHotspotConfig>>
      receivers;
  return *receivers;
}

}  // namespace

void BindToInProcessInstance(
    mojo::PendingReceiver<mojom::CrosHotspotConfig> pending_receiver) {
  NET_LOG(DEBUG) << "Binding to CrosHotspotConfig";
  if (g_hotspot_config_override) {
    GetOverrideReceivers().Add(g_hotspot_config_override,
                               std::move(pending_receiver));
    return;
  }

  if (!NetworkHandler::IsInitialized()) {
    NET_LOG(DEBUG)
        << "Ignoring request to bind Hotspot Config service because no "
        << "NetworkHandler has been initialized.";
    return;
  }

  static base::NoDestructor<CrosHotspotConfig> instance;
  instance->BindPendingReceiver(std::move(pending_receiver));
}

void OverrideInProcessInstanceForTesting(
    mojom::CrosHotspotConfig* hotspot_config) {
  g_hotspot_config_override = hotspot_config;

  // Wipe out the set of override receivers any time a new override is set.
  GetOverrideReceivers().Clear();
}

}  // namespace ash::hotspot_config