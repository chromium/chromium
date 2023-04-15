// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/hotspot_config/public/cpp/cros_hotspot_config_test_helper.h"

#include "chromeos/ash/services/hotspot_config/in_process_instance.h"

namespace ash::hotspot_config {

CrosHotspotConfigTestHelper::CrosHotspotConfigTestHelper() {
  cros_hotspot_config_impl_ = std::make_unique<CrosHotspotConfig>();
  OverrideInProcessInstanceForTesting(cros_hotspot_config_impl_.get());
}

CrosHotspotConfigTestHelper::~CrosHotspotConfigTestHelper() {
  Shutdown();
}

void CrosHotspotConfigTestHelper::Shutdown() {
  OverrideInProcessInstanceForTesting(nullptr);
  cros_hotspot_config_impl_.reset();
}

void CrosHotspotConfigTestHelper::EnableHotspot() {
  cros_hotspot_config_impl_->EnableHotspot(base::DoNothing());
}

void CrosHotspotConfigTestHelper::SetHotspotConfig(
    hotspot_config::mojom::HotspotConfigPtr hotspot_config) {
  cros_hotspot_config_impl_->SetHotspotConfig(std::move(hotspot_config),
                                              base::DoNothing());
}

}  // namespace ash::hotspot_config
