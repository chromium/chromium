// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scalable_iph/config.h"

#include "ash/constants/ash_features.h"
#include "components/feature_engagement/public/feature_constants.h"

namespace scalable_iph {

Config::Config() = default;
Config::~Config() = default;

std::unique_ptr<Config> GetConfig(const base::Feature& feature) {
  if (!ash::features::IsScalableIphClientConfigEnabled()) {
    return nullptr;
  }

  if (&feature == &feature_engagement::kIPHScalableIphHelpAppBasedOneFeature) {
    std::unique_ptr<Config> config = std::make_unique<Config>();
    config->version_number = kCurrentVersionNumber;
    config->ui_type = UiType::kNone;
    return config;
  }

  // TODO(b/308010596): Move other config.

  return nullptr;
}

}  // namespace scalable_iph
