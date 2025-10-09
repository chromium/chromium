// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/interventions/browser/canvas_interventions_web_contents_helper.h"

#include "base/feature_list.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/fingerprinting_protection_filter/interventions/common/interventions_features.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/shared_worker_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
class StoragePartition;
class ServiceWorkerContext;
}  // namespace content

namespace fingerprinting_protection_interventions {

void CanvasInterventionsWebContentsHelper::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  auto& mutable_runtime_feature_state =
      navigation_handle->GetMutableRuntimeFeatureStateContext();
  const bool canvas_base_feature_enabled =
      features::ShouldBlockCanvasReadbackForIncognitoState(is_incognito_);

  if (mutable_runtime_feature_state.IsBlockCanvasReadbackEnabled() !=
      canvas_base_feature_enabled) {
    mutable_runtime_feature_state.SetBlockCanvasReadbackEnabled(
        canvas_base_feature_enabled);
  }
}

// private
CanvasInterventionsWebContentsHelper::CanvasInterventionsWebContentsHelper(
    content::WebContents* web_contents,
    privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
    bool is_incognito)
    : content::WebContentsUserData<CanvasInterventionsWebContentsHelper>(
          *web_contents),
      content::WebContentsObserver(web_contents),
      is_incognito_(is_incognito) {
  CHECK(tracking_protection_settings);
  tracking_protection_settings_observation_.Observe(
      tracking_protection_settings);
}

CanvasInterventionsWebContentsHelper::~CanvasInterventionsWebContentsHelper() =
    default;

void CanvasInterventionsWebContentsHelper::
    OnTrackingProtectionExceptionsChanged(const GURL& first_party_url) {
  // Update canvas noise tokens for service workers matching with their
  // respective top level site.
  GetWebContents()
      .GetBrowserContext()
      ->GetStoragePartition(GetWebContents().GetSiteInstance())
      ->GetServiceWorkerContext()
      ->UpdateAllCanvasNoiseTokensFromTopLevelSite(first_party_url);
  GetWebContents()
      .GetBrowserContext()
      ->GetStoragePartition(GetWebContents().GetSiteInstance())
      ->GetSharedWorkerService()
      ->UpdateAllCanvasNoiseTokensFromTopLevelSite(first_party_url);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CanvasInterventionsWebContentsHelper);

}  // namespace fingerprinting_protection_interventions
