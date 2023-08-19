// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/feature_manager_on_associated_interface.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/common/feature_constants.h"
#include "chromecast/renderer/cast_content_settings_client.h"
#include "content/public/renderer/render_frame.h"

namespace chromecast {
FeatureManagerOnAssociatedInterface::FeatureManagerOnAssociatedInterface(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame), configured_(false) {
  registry_.AddInterface<shell::mojom::FeatureManager>(base::BindRepeating(
      &FeatureManagerOnAssociatedInterface::OnFeatureManagerAssociatedRequest,
      base::Unretained(this)));
}

FeatureManagerOnAssociatedInterface::~FeatureManagerOnAssociatedInterface() {}

bool FeatureManagerOnAssociatedInterface::OnAssociatedInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle* handle) {
  return registry_.TryBindInterface(interface_name, handle);
}

void FeatureManagerOnAssociatedInterface::OnDestruct() {
  delete this;
}

void FeatureManagerOnAssociatedInterface::ConfigureFeatures(
    std::vector<chromecast::shell::mojom::FeaturePtr> features) {
  if (configured_)
    return;
  configured_ = true;
  for (auto& feature : features) {
    // If we want to add enabled/disabled status to FeaturePtr, we can overlap
    // previous setting via [] operator
    features_map_[feature->name] = std::move(feature);
  }

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
    bool allow_insecure_content = false;
    absl::optional<bool> allow_insecure_content_received =
        feature->config.FindBool(feature::kKeyAllowInsecureContent);
    if (allow_insecure_content_received) {
      allow_insecure_content = *allow_insecure_content_received;
    } else {
      LOG(ERROR) << __func__
                 << " failed to receive valid allow_insecure_content";
    }
    // Lifetime is tied to |render_frame| via content::RenderFrameObserver.
    new CastContentSettingsClient(render_frame(), app_id,
                                  allow_insecure_content);
  }
}

void FeatureManagerOnAssociatedInterface::OnFeatureManagerAssociatedRequest(
    mojo::PendingAssociatedReceiver<shell::mojom::FeatureManager>
        pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

bool FeatureManagerOnAssociatedInterface::FeatureEnabled(
    const std::string& feature) const {
  return base::Contains(features_map_, feature);
}

const chromecast::shell::mojom::FeaturePtr&
FeatureManagerOnAssociatedInterface::GetFeature(
    const std::string& feature) const {
  auto itor = features_map_.find(feature);
  DCHECK(itor != features_map_.end());
  return itor->second;
}

}  // namespace chromecast
