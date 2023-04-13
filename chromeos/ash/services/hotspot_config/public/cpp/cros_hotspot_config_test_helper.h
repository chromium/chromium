// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_CROS_HOTSPOT_CONFIG_TEST_HELPER_H_
#define CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_CROS_HOTSPOT_CONFIG_TEST_HELPER_H_

#include <memory>
#include "chromeos/ash/services/hotspot_config/cros_hotspot_config.h"

namespace ash {

namespace hotspot_config {

class CrosHotspotConfig;

// Helper for tests which need a CrosHotspotConfig service interface.
class CrosHotspotConfigTestHelper {
 public:
  // Default constructor for unit tests.
  CrosHotspotConfigTestHelper();

  CrosHotspotConfigTestHelper(const CrosHotspotConfigTestHelper&) = delete;
  CrosHotspotConfigTestHelper& operator=(const CrosHotspotConfigTestHelper&) =
      delete;

  ~CrosHotspotConfigTestHelper();

  void EnableHotspot();

  void SetHotspotConfig(hotspot_config::mojom::HotspotConfigPtr hotspot_config);

 protected:
  // Called in |~CrosHotspotConfigTestHelper()| to destroy
  // cros_hotspot_config_impl_.
  void Shutdown();

  std::unique_ptr<CrosHotspotConfig> cros_hotspot_config_impl_;
};

}  // namespace hotspot_config

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_CROS_HOTSPOT_CONFIG_TEST_HELPER_H_
