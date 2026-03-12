// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/memory/scoped_refptr.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/subresource_filter/content/browser/ads_intervention_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"

namespace subresource_filter {

// Store the member variables for SubresourceFilterProfileContext ensuring
// they are destroyed in reverse order of construction (and thus correctly
// handle dependencies between the member variables).
class SubresourceFilterProfileContext::Storage {
 public:
  using EmbedderData = SubresourceFilterProfileContext::EmbedderData;

  Storage(HostContentSettingsMap* settings_map,
          scoped_refptr<content_settings::CookieSettings> cookie_settings)
      : settings_manager_(settings_map),
        ads_intervention_manager_(&settings_manager_),
        cookie_settings_(std::move(cookie_settings)) {}

  Storage(const Storage&) = delete;
  Storage& operator=(const Storage&) = delete;

  ~Storage() = default;

  SubresourceFilterContentSettingsManager* settings_manager() {
    return &settings_manager_;
  }

  AdsInterventionManager* ads_intervention_manager() {
    return &ads_intervention_manager_;
  }

  content_settings::CookieSettings* cookie_settings() {
    return cookie_settings_.get();
  }

  // Attaches an embedder-level object.
  void SetEmbedderData(std::unique_ptr<EmbedderData> embedder_data) {
    CHECK(!embedder_data_);
    embedder_data_ = std::move(embedder_data);
  }

 private:
  // The declaration order of those objects ensure the raw_ptr<T>
  // are destroyed before the objects they point to (e.g. in particular
  // `ads_intervention_manager_` has a pointer to `settings_manager_`).

  SubresourceFilterContentSettingsManager settings_manager_;
  // Manages ads interventions that have been triggered on previous
  // navigations.
  AdsInterventionManager ads_intervention_manager_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  std::unique_ptr<EmbedderData> embedder_data_;
};

SubresourceFilterProfileContext::SubresourceFilterProfileContext(
    HostContentSettingsMap* settings_map,
    scoped_refptr<content_settings::CookieSettings> cookie_settings)
    : storage_(std::make_unique<Storage>(settings_map,
                                         std::move(cookie_settings))) {}

SubresourceFilterProfileContext::~SubresourceFilterProfileContext() = default;

SubresourceFilterContentSettingsManager*
SubresourceFilterProfileContext::settings_manager() {
  return CHECK_DEREF(storage_).settings_manager();
}

AdsInterventionManager*
SubresourceFilterProfileContext::ads_intervention_manager() {
  return CHECK_DEREF(storage_).ads_intervention_manager();
}

content_settings::CookieSettings*
SubresourceFilterProfileContext::cookie_settings() {
  return CHECK_DEREF(storage_).cookie_settings();
}

void SubresourceFilterProfileContext::SetEmbedderData(
    std::unique_ptr<EmbedderData> embedder_data) {
  CHECK_DEREF(storage_).SetEmbedderData(std::move(embedder_data));
}

void SubresourceFilterProfileContext::Shutdown() {
  storage_.reset();
}

}  // namespace subresource_filter
