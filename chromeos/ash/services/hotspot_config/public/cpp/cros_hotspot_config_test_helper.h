// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_CROS_HOTSPOT_CONFIG_TEST_HELPER_H_
#define CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_CROS_HOTSPOT_CONFIG_TEST_HELPER_H_

#include <memory>

#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"

namespace ash {

namespace hotspot_config {

// Helper for tests which need a CrosHotspotConfig service interface.
class CrosHotspotConfigTestHelper {
 public:
  // Default constructor for unit tests.
  explicit CrosHotspotConfigTestHelper(bool use_fake_implementation);
  CrosHotspotConfigTestHelper(const CrosHotspotConfigTestHelper&) = delete;
  CrosHotspotConfigTestHelper& operator=(const CrosHotspotConfigTestHelper&) =
      delete;
  ~CrosHotspotConfigTestHelper();

  void EnableHotspot();
  void DisableHotspot();
  void SetHotspotConfig(hotspot_config::mojom::HotspotConfigPtr hotspot_config);
  // Only call this function when using FakeCrosHotspotConfig.
  void SetFakeHotspotInfo(mojom::HotspotInfoPtr hotspot_info);

 protected:
  // Called in |~CrosHotspotConfigTestHelper()| to destroy
  // cros_hotspot_config_impl_.
  void Shutdown();

  bool use_fake_implementation_ = false;
  std::unique_ptr<mojom::CrosHotspotConfig> cros_hotspot_config_impl_;
};

}  // namespace hotspot_config

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_CROS_HOTSPOT_CONFIG_TEST_HELPER_H_
