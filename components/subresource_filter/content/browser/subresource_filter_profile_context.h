// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_PROFILE_CONTEXT_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_PROFILE_CONTEXT_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"

class HostContentSettingsMap;

namespace content_settings {
class CookieSettings;
}  // namespace content_settings

namespace subresource_filter {

class SubresourceFilterContentSettingsManager;
class AdsInterventionManager;

// This class holds BrowserContext-scoped context for subresource filtering. The
// embedder should use KeyedServiceFactory to associate instances of this class
// with instances of (their subclass of) BrowserContext; see //chrome's
// subresource_filter_profile_context_factory.* for an example.
class SubresourceFilterProfileContext : public KeyedService {
 public:
  // An opaque class that the embedder can use to scope an embedder-level object
  // to SubresourceFilterProfileContext via SetEmbedderData().
  class EmbedderData {
   public:
    virtual ~EmbedderData() = default;
  };

  explicit SubresourceFilterProfileContext(
      HostContentSettingsMap* settings_map,
      scoped_refptr<content_settings::CookieSettings> cookie_settings);

  SubresourceFilterProfileContext(const SubresourceFilterProfileContext&) =
      delete;
  SubresourceFilterProfileContext& operator=(
      const SubresourceFilterProfileContext&) = delete;

  ~SubresourceFilterProfileContext() override;

  SubresourceFilterContentSettingsManager* settings_manager() {
    return settings_manager_.get();
  }

  AdsInterventionManager* ads_intervention_manager() {
    return ads_intervention_manager_.get();
  }

  content_settings::CookieSettings* cookie_settings() {
    return cookie_settings_.get();
  }

  // Can be used to attach an embedder-level object to this object. Can only be
  // invoked once. |embedder_data| will be destroyed before the other objects
  // owned by this object, and thus it can safely depend on those other objects.
  void SetEmbedderData(std::unique_ptr<EmbedderData> embedder_data);

 private:
  // KeyedService:
  void Shutdown() override;

  std::unique_ptr<SubresourceFilterContentSettingsManager> settings_manager_;

  // Manages ads interventions that have been triggered on previous
  // navigations.
  std::unique_ptr<AdsInterventionManager> ads_intervention_manager_;

  scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  // NOTE: Declared after the objects above to ensure that it is destroyed
  // before them.
  std::unique_ptr<EmbedderData> embedder_data_;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_PROFILE_CONTEXT_H_
