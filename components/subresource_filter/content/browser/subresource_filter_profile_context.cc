// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"

#include "components/subresource_filter/content/browser/ads_intervention_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"

namespace subresource_filter {

SubresourceFilterProfileContext::SubresourceFilterProfileContext(
    HostContentSettingsMap* settings_map)
    : settings_manager_(
          std::make_unique<SubresourceFilterContentSettingsManager>(
              settings_map)),
      ads_intervention_manager_(
          std::make_unique<AdsInterventionManager>(settings_manager_.get())) {}

SubresourceFilterProfileContext::~SubresourceFilterProfileContext() {}

void SubresourceFilterProfileContext::SetEmbedderData(
    std::unique_ptr<SubresourceFilterProfileContext::EmbedderData>
        embedder_data) {
  DCHECK(!embedder_data_);
  embedder_data_ = std::move(embedder_data);
}

void SubresourceFilterProfileContext::Shutdown() {
  settings_manager_.reset();
  ads_intervention_manager_.reset();
}

}  // namespace subresource_filter
