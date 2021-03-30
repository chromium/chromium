// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/profile_interaction_manager.h"

#include "base/logging.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/subresource_filter/content/browser/ads_intervention_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace subresource_filter {

ProfileInteractionManager::ProfileInteractionManager(
    content::WebContents* web_contents,
    SubresourceFilterProfileContext* profile_context)
    : content::WebContentsObserver(web_contents),
      profile_context_(profile_context) {
  DCHECK(web_contents);
}

ProfileInteractionManager::~ProfileInteractionManager() = default;

void ProfileInteractionManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() && navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    ads_violation_triggered_for_last_committed_navigation_ = false;
  }
}

void ProfileInteractionManager::OnReloadRequested() {
  ContentSubresourceFilterThrottleManager::LogAction(
      SubresourceFilterAction::kAllowlistedSite);
  profile_context_->settings_manager()->AllowlistSite(
      web_contents()->GetLastCommittedURL());

  web_contents()->GetController().Reload(content::ReloadType::NORMAL, true);
}

// TODO(https://crbug.com/1131969): Consider adding reporting when
// ads violations are triggered.
void ProfileInteractionManager::OnAdsViolationTriggered(
    content::RenderFrameHost* rfh,
    mojom::AdsViolation triggered_violation) {
  // Only trigger violations once per navigation. The ads intervention
  // manager ignores all interventions after recording an intervention
  // for the intervention duration, however, a page that began a navigation
  // before the intervention duration and was still alive after the duration
  // could re-trigger an ads intervention.
  if (ads_violation_triggered_for_last_committed_navigation_)
    return;

  // If the feature is disabled, simulate ads interventions as if we were
  // enforcing on ads: do not record new interventions if we would be enforcing
  // an intervention on ads already.
  //
  // TODO(https://crbug.com/1131971): Add support for enabling ads interventions
  // separately for different ads violations.
  const GURL& url = rfh->GetLastCommittedURL();
  base::Optional<AdsInterventionManager::LastAdsIntervention>
      last_intervention =
          profile_context_->ads_intervention_manager()->GetLastAdsIntervention(
              url);
  // TODO(crbug.com/1131971): If a host triggers multiple times on a single
  // navigate and the durations don't match, we'll use the last duration rather
  // than the longest. The metadata should probably store the activation with
  // the longest duration.
  if (last_intervention && last_intervention->duration_since <
                               AdsInterventionManager::GetInterventionDuration(
                                   last_intervention->ads_violation)) {
    return;
  }

  profile_context_->ads_intervention_manager()
      ->TriggerAdsInterventionForUrlOnSubsequentLoads(url, triggered_violation);

  ads_violation_triggered_for_last_committed_navigation_ = true;
}

mojom::ActivationLevel ProfileInteractionManager::OnPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    mojom::ActivationLevel initial_activation_level,
    ActivationDecision* decision) {
  DCHECK(navigation_handle->IsInMainFrame());

  mojom::ActivationLevel effective_activation_level = initial_activation_level;

  if (profile_context_->ads_intervention_manager()->ShouldActivate(
          navigation_handle)) {
    effective_activation_level = mojom::ActivationLevel::kEnabled;
    *decision = ActivationDecision::ACTIVATED;
  }

  const GURL& url(navigation_handle->GetURL());
  if (url.SchemeIsHTTPOrHTTPS()) {
    profile_context_->settings_manager()->SetSiteMetadataBasedOnActivation(
        url, effective_activation_level == mojom::ActivationLevel::kEnabled,
        SubresourceFilterContentSettingsManager::ActivationSource::
            kSafeBrowsing);
  }

  if (profile_context_->settings_manager()->GetSitePermission(url) ==
      CONTENT_SETTING_ALLOW) {
    if (effective_activation_level == mojom::ActivationLevel::kEnabled) {
      *decision = ActivationDecision::URL_ALLOWLISTED;
    }
    return mojom::ActivationLevel::kDisabled;
  }

  return effective_activation_level;
}

void ProfileInteractionManager::MaybeShowNotification(
    SubresourceFilterClient* client) {
  const GURL& top_level_url = web_contents()->GetLastCommittedURL();
  if (profile_context_->settings_manager()->ShouldShowUIForSite(
          top_level_url)) {
    client->ShowNotification();

    // TODO(https://crbug.com/1103176): Plumb the actual frame reference here
    // (it comes from
    // ContentSubresourceFilterThrottleManager::DidDisallowFirstSubresource,
    // which comes from a specific frame).
    content_settings::PageSpecificContentSettings* content_settings =
        content_settings::PageSpecificContentSettings::GetForFrame(
            web_contents()->GetMainFrame());
    content_settings->OnContentBlocked(ContentSettingsType::ADS);

    ContentSubresourceFilterThrottleManager::LogAction(
        SubresourceFilterAction::kUIShown);
    profile_context_->settings_manager()->OnDidShowUI(top_level_url);
  } else {
    ContentSubresourceFilterThrottleManager::LogAction(
        SubresourceFilterAction::kUISuppressed);
  }
}

}  // namespace subresource_filter
