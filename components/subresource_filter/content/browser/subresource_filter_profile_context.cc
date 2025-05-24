// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"

#include "base/check.h"
#include "base/not_fatal_until.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/subresource_filter/content/browser/ads_intervention_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"

namespace subresource_filter {

SubresourceFilterProfileContext::SubresourceFilterProfileContext(
    HostContentSettingsMap* settings_map,
    scoped_refptr<content_settings::CookieSettings> cookie_settings)
    : settings_manager_(
          std::make_unique<SubresourceFilterContentSettingsManager>(
              settings_map)),
      ads_intervention_manager_(
          std::make_unique<AdsInterventionManager>(settings_manager_.get())),
      cookie_settings_(std::move(cookie_settings)) {}

SubresourceFilterProfileContext::~SubresourceFilterProfileContext() = default;

void SubresourceFilterProfileContext::SetEmbedderData(
    std::unique_ptr<SubresourceFilterProfileContext::EmbedderData>
        embedder_data) {
  CHECK(!embedder_data_, base::NotFatalUntil::M129);
  embedder_data_ = std::move(embedder_data);
}

void SubresourceFilterProfileContext::Shutdown() {
  // `ads_intervention_manager_` holds a raw_ptr to `settings_manager_`. Make
  // sure they are reset in the right order to avoid holding a dangling pointer.
  ads_intervention_manager_.reset();
  settings_manager_.reset();
  cookie_settings_.reset();
}

}  // namespace subresource_filter
