// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/profile_interaction_manager.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "build/build_config.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/subresource_filter/content/browser/ads_intervention_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "components/subresource_filter/content/shared/browser/utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/infobars/content/content_infobar_manager.h"  // nogncheck
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/messages_feature.h"
#include "components/subresource_filter/content/browser/ads_blocked_infobar_delegate.h"
#endif

namespace subresource_filter {

ProfileInteractionManager::ProfileInteractionManager(
    SubresourceFilterProfileContext* profile_context)
    : profile_context_(profile_context) {}

ProfileInteractionManager::~ProfileInteractionManager() = default;

void ProfileInteractionManager::DidCreatePage(content::Page& page) {
  // A new ProfileInteractionManager is created for each page so we should only
  // call this, at most, once.
  CHECK(!page_, base::NotFatalUntil::M129);
  page_ = &page;
}

void ProfileInteractionManager::OnReloadRequested() {
  // A reload request comes from browser so it will always be associated with
  // the primary page.
  CHECK(page_, base::NotFatalUntil::M129);
  CHECK(page_->IsPrimary(), base::NotFatalUntil::M129);

  ContentSubresourceFilterThrottleManager::LogAction(
      SubresourceFilterAction::kAllowlistedSite);
  profile_context_->settings_manager()->AllowlistSite(
      page_->GetMainDocument().GetLastCommittedURL());

  // Since the reload comes from the primary page, the use of WebContents here
  // is correct.
  GetWebContents()->GetController().Reload(content::ReloadType::NORMAL, true);
}

// TODO(crbug.com/40721689): Consider adding reporting when
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
  // TODO(crbug.com/40721691): Add support for enabling ads interventions
  // separately for different ads violations.
  const GURL& url = rfh->GetLastCommittedURL();
  std::optional<AdsInterventionManager::LastAdsIntervention> last_intervention =
      profile_context_->ads_intervention_manager()->GetLastAdsIntervention(url);
  // TODO(crbug.com/40721691): If a host triggers multiple times on a single
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
  CHECK(IsInSubresourceFilterRoot(navigation_handle),
        base::NotFatalUntil::M129);

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

void ProfileInteractionManager::MaybeShowNotification() {
  // The caller should make sure this is only called from pages that are
  // currently primary.
  CHECK(page_, base::NotFatalUntil::M129);
  CHECK(page_->IsPrimary(), base::NotFatalUntil::M129);

  const GURL& top_level_url = page_->GetMainDocument().GetLastCommittedURL();
  if (profile_context_->settings_manager()->ShouldShowUIForSite(
          top_level_url)) {
#if BUILDFLAG(IS_ANDROID)
    if (messages::IsAdsBlockedMessagesUiEnabled() &&
        messages::MessageDispatcherBridge::Get()
            ->IsMessagesEnabledForEmbedder()) {
      subresource_filter::AdsBlockedMessageDelegate::CreateForWebContents(
          GetWebContents());
      ads_blocked_message_delegate_ =
          subresource_filter::AdsBlockedMessageDelegate::FromWebContents(
              GetWebContents());
      ads_blocked_message_delegate_->ShowMessage();
    } else {
      // NOTE: It is acceptable for the embedder to not have installed an
      // infobar manager.
      if (auto* infobar_manager =
              infobars::ContentInfoBarManager::FromWebContents(
                  GetWebContents())) {
        subresource_filter::AdsBlockedInfobarDelegate::Create(infobar_manager);
      }
    }
#endif

    // TODO(crbug.com/40139135): Plumb the actual frame reference here
    // (it comes from
    // ContentSubresourceFilterThrottleManager::DidDisallowFirstSubresource,
    // which comes from a specific frame).
    content_settings::PageSpecificContentSettings* content_settings =
        content_settings::PageSpecificContentSettings::GetForFrame(
            &page_->GetMainDocument());
    content_settings->OnContentBlocked(ContentSettingsType::ADS);

    ContentSubresourceFilterThrottleManager::LogAction(
        SubresourceFilterAction::kUIShown);
    profile_context_->settings_manager()->OnDidShowUI(top_level_url);
  } else {
    ContentSubresourceFilterThrottleManager::LogAction(
        SubresourceFilterAction::kUISuppressed);
  }
}

content_settings::CookieSettings*
ProfileInteractionManager::GetCookieSettings() {
  return profile_context_->cookie_settings();
}

content::WebContents* ProfileInteractionManager::GetWebContents() {
  CHECK(page_, base::NotFatalUntil::M129);
  CHECK(page_->IsPrimary(), base::NotFatalUntil::M129);
  return content::WebContents::FromRenderFrameHost(&page_->GetMainDocument());
}

}  // namespace subresource_filter
