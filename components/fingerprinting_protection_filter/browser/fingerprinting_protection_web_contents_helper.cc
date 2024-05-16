// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_filter_features.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_observer.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace subresource_filter {
enum class ActivationDecision;
enum class LoadPolicy;
}  // namespace subresource_filter

namespace fingerprinting_protection_filter {

// static
void FingerprintingProtectionWebContentsHelper::CreateForWebContents(
    content::WebContents* web_contents,
    PrefService* pref_service,
    privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings) {
  if (!base::FeatureList::IsEnabled(
          features::kEnableFingerprintingProtectionFilter)) {
    return;
  }

  if (FromWebContents(web_contents)) {
    return;
  }

  content::WebContentsUserData<FingerprintingProtectionWebContentsHelper>::
      CreateForWebContents(web_contents, pref_service,
                           tracking_protection_settings);
}

//  private
FingerprintingProtectionWebContentsHelper::
    FingerprintingProtectionWebContentsHelper(
        content::WebContents* web_contents,
        PrefService* pref_service,
        privacy_sandbox::TrackingProtectionSettings*
            tracking_protection_settings)
    : content::WebContentsUserData<FingerprintingProtectionWebContentsHelper>(
          *web_contents),
      tracking_protection_settings_(tracking_protection_settings),
      pref_service_(pref_service) {}

FingerprintingProtectionWebContentsHelper::
    ~FingerprintingProtectionWebContentsHelper() = default;

void FingerprintingProtectionWebContentsHelper::NotifyPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    const subresource_filter::ActivationDecision& activation_decision) {
  // TODO(crbug.com/327005578): Notify ThrottleManager
}

void FingerprintingProtectionWebContentsHelper::
    NotifyChildFrameNavigationEvaluated(
        content::NavigationHandle* navigation_handle,
        subresource_filter::LoadPolicy load_policy) {
  // TODO(crbug.com/327005578): Notify ThrottleManager
}

void FingerprintingProtectionWebContentsHelper::NotifyOnBlockedResources() {
  is_subresource_blocked_ = true;
  for (auto& observer : observer_list_) {
    observer.OnSubresourceBlocked();
  }
}

void FingerprintingProtectionWebContentsHelper::AddObserver(
    FingerprintingProtectionObserver* observer) {
  observer_list_.AddObserver(observer);
}

void FingerprintingProtectionWebContentsHelper::RemoveObserver(
    FingerprintingProtectionObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FingerprintingProtectionWebContentsHelper);

}  // namespace fingerprinting_protection_filter
