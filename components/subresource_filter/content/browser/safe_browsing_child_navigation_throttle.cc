// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/safe_browsing_child_navigation_throttle.h"

#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "components/subresource_filter/content/browser/ad_tagging_utils.h"
#include "components/subresource_filter/content/browser/profile_interaction_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/content/shared/browser/child_frame_navigation_filtering_throttle.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "content/public/browser/navigation_handle.h"
#include "net/cookies/static_cookie_policy.h"
#include "third_party/blink/public/common/frame/frame_ad_evidence.h"
#include "url/gurl.h"

namespace {

// Returns true iff the request is considered third-party.
bool IsThirdPartyRequest(const GURL& url,
                         const net::SiteForCookies& site_for_cookies) {
  return net::StaticCookiePolicy(
             net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES)
             .CanAccessCookies(url, site_for_cookies) != net::OK;
}

}  // namespace

namespace features {

// Enables or disables performing SubresourceFilter checks from the Browser
// against any aliases for the requested URL found from DNS CNAME records.
BASE_FEATURE(kSendCnameAliasesToSubresourceFilterFromBrowser,
             "SendCnameAliasesToSubresourceFilterFromBrowser",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

namespace subresource_filter {

SafeBrowsingChildNavigationThrottle::SafeBrowsingChildNavigationThrottle(
    content::NavigationHandle* handle,
    AsyncDocumentSubresourceFilter* parent_frame_filter,
    base::WeakPtr<ProfileInteractionManager> profile_interaction_manager,
    base::RepeatingCallback<std::string(const GURL& url)>
        disallow_message_callback,
    std::optional<blink::FrameAdEvidence> ad_evidence)
    : ChildFrameNavigationFilteringThrottle(
          handle,
          parent_frame_filter,
          /*alias_check_enabled=*/
          base::FeatureList::IsEnabled(
              features::kSendCnameAliasesToSubresourceFilterFromBrowser),
          std::move(disallow_message_callback)),
      ad_evidence_(std::move(ad_evidence)),
      profile_interaction_manager_(profile_interaction_manager) {
  if (ad_evidence_.has_value()) {
    // Complete the ad evidence as it will be used to make best-effort tagging
    // decisions by request time for ongoing subframe navs.
    ad_evidence_->set_is_complete();
  }
}

SafeBrowsingChildNavigationThrottle::~SafeBrowsingChildNavigationThrottle() {
  switch (load_policy_) {
    case LoadPolicy::EXPLICITLY_ALLOW:
      [[fallthrough]];
    case LoadPolicy::ALLOW:
      UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
          "SubresourceFilter.DocumentLoad.SubframeFilteringDelay.Allowed",
          total_defer_time_, base::Microseconds(1), base::Seconds(10), 50);
      break;
    case LoadPolicy::WOULD_DISALLOW:
      UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
          "SubresourceFilter.DocumentLoad.SubframeFilteringDelay.WouldDisallow",
          total_defer_time_, base::Microseconds(1), base::Seconds(10), 50);
      break;
    case LoadPolicy::DISALLOW:
      UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
          "SubresourceFilter.DocumentLoad.SubframeFilteringDelay.Disallowed2",
          total_defer_time_, base::Microseconds(1), base::Seconds(10), 50);
      break;
  }
}

const char* SafeBrowsingChildNavigationThrottle::GetNameForLogging() {
  return "SafeBrowsingChildNavigationThrottle";
}

bool SafeBrowsingChildNavigationThrottle::ShouldDeferNavigation() const {
  // If the embedder document has activation enabled, we calculate frame load
  // policy before proceeding with navigation as filtered navigations are not
  // allowed to get a response. As a result, we must defer while
  // we wait for the ruleset check to complete and pass handling the navigation
  // decision to the callback.
  if (parent_frame_filter_->activation_state().activation_level ==
      mojom::ActivationLevel::kEnabled) {
    return true;
  }

  // If `kTPCDAdHeuristicSubframeRequestTagging`, we always need to defer
  // navigation start to ensure we have the load policy calculated in order
  // to properly tag the navigation handle as an ad before it goes to the
  // network.
  if (base::FeatureList::IsEnabled(kTPCDAdHeuristicSubframeRequestTagging)) {
    // If `kCheckFor3pcException`, we only defer the navigation if a
    // third-party cookie exceptions is applicable to it.
    bool defer_for_tagging =
        !kCheckFor3pcException.Get() || NavigationHasCookieException();
    UMA_HISTOGRAM_BOOLEAN(
        "PageLoad.FrameCounts.AdFrames.PerFrame.DeferredForTagging",
        defer_for_tagging);

    return defer_for_tagging;
  }

  return false;
}

void SafeBrowsingChildNavigationThrottle::
    OnReadyToResumeNavigationWithLoadPolicy() {
  if (defer_stage_ == DeferStage::kWillStartOrRedirectRequest &&
      ad_evidence_.has_value()) {
    // Tag the navigation handle based on the current load policy + evidence
    // before the request starts.
    ad_evidence_->UpdateFilterListResult(
        InterpretLoadPolicyAsEvidence(load_policy_));
    if (ad_evidence_->IndicatesAdFrame()) {
      navigation_handle()->SetIsAdTagged();
    }
  }
}

void SafeBrowsingChildNavigationThrottle::NotifyLoadPolicy() const {
  auto* observer_manager = SubresourceFilterObserverManager::FromWebContents(
      navigation_handle()->GetWebContents());
  if (!observer_manager) {
    return;
  }

  observer_manager->NotifyChildFrameNavigationEvaluated(navigation_handle(),
                                                        load_policy_);
}

bool SafeBrowsingChildNavigationThrottle::NavigationHasCookieException() const {
  if (!profile_interaction_manager_) {
    // This method informs whether navigations should be deferred for ad-tagging
    // checks to be completed. Default to `true` so potentially affected
    // navigations aren't missed.
    return true;
  }

  net::IsolationInfo isolation_info = navigation_handle()->GetIsolationInfo();

  if (!IsThirdPartyRequest(navigation_handle()->GetURL(),
                           isolation_info.site_for_cookies())) {
    return false;
  }

  using ThirdPartyCookieAllowMechanism =
      content_settings::CookieSettingsBase::ThirdPartyCookieAllowMechanism;

  ThirdPartyCookieAllowMechanism allow_mechanism =
      profile_interaction_manager_->GetCookieSettings()
          ->GetThirdPartyCookieAllowMechanism(
              navigation_handle()->GetURL(), isolation_info.site_for_cookies(),
              isolation_info.top_frame_origin()
                  .value_or(url::Origin())
                  .GetURL(),
              net::CookieSettingOverrides(), nullptr);

  return allow_mechanism != ThirdPartyCookieAllowMechanism::kNone;
}

}  // namespace subresource_filter
