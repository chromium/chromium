// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ui/display_settings_manager.h"

namespace chromecast {

DisplaySettingsManager::ColorTemperatureConfig::ColorTemperatureConfig() =
    default;
DisplaySettingsManager::ColorTemperatureConfig::ColorTemperatureConfig(
    const ColorTemperatureConfig& other) = default;
DisplaySettingsManager::ColorTemperatureConfig::~ColorTemperatureConfig() =
    default;

}  // namespace chromecast
