// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/feature_manager_on_associated_interface.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/common/feature_constants.h"
#include "chromecast/renderer/cast_content_settings_client.h"
#include "content/public/renderer/render_frame.h"

namespace chromecast {
FeatureManagerOnAssociatedInterface::FeatureManagerOnAssociatedInterface(
    content::RenderFrame* render_frame)
    : FeatureManager(render_frame) {}

FeatureManagerOnAssociatedInterface::~FeatureManagerOnAssociatedInterface() {}

void FeatureManagerOnAssociatedInterface::ConfigureFeaturesInternal() {
  FeatureManager::ConfigureFeaturesInternal();

  if (FeatureEnabled(feature::kEnableTrackControlAppRendererFeatureUse)) {
    std::string app_id("MissingAppId");
    auto& feature =
        GetFeature(feature::kEnableTrackControlAppRendererFeatureUse);
    const std::string* app_id_received =
        feature->config.FindString(feature::kKeyAppId);
    if (app_id_received) {
      app_id = *app_id_received;
    } else {
      LOG(ERROR) << __func__ << " failed to receive valid app_id";
    }
    bool allow_insecure_content = true;
    // Lifetime is tied to |render_frame| via content::RenderFrameObserver.
    new CastContentSettingsClient(render_frame(), app_id,
                                  allow_insecure_content);
  }
}

}  // namespace chromecast
