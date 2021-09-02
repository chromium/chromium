// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/input_overlay/resources/input_overlay_resources_util.h"

#include <map>

#include "components/arc/grit/input_overlay_resources.h"

namespace arc {

absl::optional<int> GetInputOverlayResourceId(const std::string& package_name) {
  static std::map<std::string, int> resource_id_map = {
      {"org.chromium.arc.testapp.inputoverlay",
       IDR_IO_ORG_CHROMIUM_ARC_TESTAPP_INPUTOVERLAY},
  };

  auto it = resource_id_map.find(package_name);
  if (it != resource_id_map.end())
    return absl::optional<int>(it->second);
  return absl::optional<int>();
}

}  // namespace arc
