// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_filter_features.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace fingerprinting_protection_filter {

// static
void FingerprintingProtectionWebContentsHelper::CreateForWebContents(
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(
          features::kEnableFingerprintingProtectionFilter)) {
    return;
  }

  if (FromWebContents(web_contents)) {
    return;
  }

  content::WebContentsUserData<FingerprintingProtectionWebContentsHelper>::
      CreateForWebContents(web_contents);
}

FingerprintingProtectionWebContentsHelper::
    FingerprintingProtectionWebContentsHelper(
        content::WebContents* web_contents)
    : content::WebContentsUserData<FingerprintingProtectionWebContentsHelper>(
          *web_contents) {}

FingerprintingProtectionWebContentsHelper::
    ~FingerprintingProtectionWebContentsHelper() = default;

void FingerprintingProtectionWebContentsHelper::NotifyPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    const subresource_filter::mojom::ActivationState& activation_state) {
  // TODO(crbug.com/327005578): Notify ThrottleManager
}

void FingerprintingProtectionWebContentsHelper::
    NotifyChildFrameNavigationEvaluated(
        content::NavigationHandle* navigation_handle,
        subresource_filter::LoadPolicy load_policy) {
  // TODO(crbug.com/327005578): Notify ThrottleManager
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FingerprintingProtectionWebContentsHelper);

}  // namespace fingerprinting_protection_filter
