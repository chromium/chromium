// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_child_navigation_throttle.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/subresource_filter/content/shared/browser/child_frame_navigation_filtering_throttle.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"

class GURL;

namespace fingerprinting_protection_filter {

FingerprintingProtectionChildNavigationThrottle::
    FingerprintingProtectionChildNavigationThrottle(
        content::NavigationHandle* handle,
        subresource_filter::AsyncDocumentSubresourceFilter* parent_frame_filter,
        base::RepeatingCallback<std::string(const GURL& url)>
            disallow_message_callback)
    : subresource_filter::ChildFrameNavigationFilteringThrottle(
          handle,
          parent_frame_filter,
          /*alias_check_enabled=*/
          base::FeatureList::IsEnabled(
              features::kUseCnameAliasesForFingerprintingProtectionFilter),
          std::move(disallow_message_callback)) {}

FingerprintingProtectionChildNavigationThrottle::
    ~FingerprintingProtectionChildNavigationThrottle() {
  // Note: Changing the UMA_HISTOGRAM_CUSTOM_MICRO_TIMES params requires
  // renaming the emitted metrics.
#define SUBFRAME_FILTERING_HISTOGRAM(name) \
  UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(        \
      name, total_defer_time_, base::Microseconds(1), base::Seconds(10), 50)
  if (did_alias_check_) {
    SUBFRAME_FILTERING_HISTOGRAM(
        "FingerprintingProtection.DocumentLoad.SubframeFilteringDelay."
        "NameAlias.Checked");
  }
  switch (load_policy_) {
    case subresource_filter::LoadPolicy::EXPLICITLY_ALLOW:
      [[fallthrough]];
    case subresource_filter::LoadPolicy::ALLOW:
      SUBFRAME_FILTERING_HISTOGRAM(
          "FingerprintingProtection.DocumentLoad.SubframeFilteringDelay."
          "Allowed");
      break;
    case subresource_filter::LoadPolicy::WOULD_DISALLOW:
      if (did_alias_check_determine_load_policy_) {
        SUBFRAME_FILTERING_HISTOGRAM(
            "FingerprintingProtection.DocumentLoad.SubframeFilteringDelay."
            "NameAlias.WouldDisallow");
      }
      SUBFRAME_FILTERING_HISTOGRAM(
          "FingerprintingProtection.DocumentLoad.SubframeFilteringDelay."
          "WouldDisallow");
      break;
    case subresource_filter::LoadPolicy::DISALLOW:
      if (did_alias_check_determine_load_policy_) {
        SUBFRAME_FILTERING_HISTOGRAM(
            "FingerprintingProtection.DocumentLoad.SubframeFilteringDelay."
            "NameAlias.Disallowed");
      }
      SUBFRAME_FILTERING_HISTOGRAM(
          "FingerprintingProtection.DocumentLoad.SubframeFilteringDelay."
          "Disallowed");
      break;
  }
#undef SUBFRAME_FILTERING_HISTOGRAM
}

const char*
FingerprintingProtectionChildNavigationThrottle::GetNameForLogging() {
  return "FingerprintingProtectionChildNavigationThrottle";
}

bool FingerprintingProtectionChildNavigationThrottle::ShouldDeferNavigation()
    const {
  // If the embedder document has activation enabled, we calculate frame load
  // policy before proceeding with navigation as filtered navigations are not
  // allowed to get a response. As a result, we must defer while
  // we wait for the ruleset check to complete and pass handling the navigation
  // decision to the callback.
  return parent_frame_filter_->activation_state().activation_level ==
         subresource_filter::mojom::ActivationLevel::kEnabled;
}

void FingerprintingProtectionChildNavigationThrottle::
    OnReadyToResumeNavigationWithLoadPolicy() {
  // Nothing to do here.
  return;
}

void FingerprintingProtectionChildNavigationThrottle::NotifyLoadPolicy() const {
  auto* web_contents_helper =
      FingerprintingProtectionWebContentsHelper::FromWebContents(
          navigation_handle()->GetWebContents());
  // TODO(https://crbug.com/40280666): Figure out how to initialize the
  // WebContentsHelper in unittests to make it testable.
  if (web_contents_helper == nullptr) {
    return;
  };
  web_contents_helper->NotifyChildFrameNavigationEvaluated(navigation_handle(),
                                                           load_policy_);
}

}  // namespace fingerprinting_protection_filter
