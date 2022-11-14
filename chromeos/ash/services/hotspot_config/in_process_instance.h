// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_IN_PROCESS_INSTANCE_H_
#define CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_IN_PROCESS_INSTANCE_H_

#include "base/component_export.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash::hotspot_config {

// Binds to an instance of CrosHotspotConfig from within the browser process.
COMPONENT_EXPORT(IN_PROCESS_HOTSPOT_CONFIG)
void BindToInProcessInstance(
    mojo::PendingReceiver<mojom::CrosHotspotConfig> pending_receiver);

// Overrides the in-process instance for testing purposes.
COMPONENT_EXPORT(IN_PROCESS_HOTSPOT_CONFIG)
void OverrideInProcessInstanceForTesting(
    mojom::CrosHotspotConfig* hotspot_config);

}  // namespace ash::hotspot_config

#endif  // CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_IN_PROCESS_INSTANCE_H_