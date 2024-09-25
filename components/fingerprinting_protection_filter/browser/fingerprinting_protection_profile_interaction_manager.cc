// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_profile_interaction_manager.h"

#include "base/check_op.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/subresource_filter/content/shared/browser/utils.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"

namespace fingerprinting_protection_filter {

using ::subresource_filter::ActivationDecision;
using ::subresource_filter::mojom::ActivationLevel;

ProfileInteractionManager::ProfileInteractionManager(
    privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
    PrefService* prefs)
    : tracking_protection_settings_(tracking_protection_settings),
      prefs_(prefs) {}

ProfileInteractionManager::~ProfileInteractionManager() = default;

ActivationLevel ProfileInteractionManager::OnPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    ActivationLevel initial_activation_level,
    ActivationDecision* decision) {
  DCHECK(subresource_filter::IsInSubresourceFilterRoot(navigation_handle));
  // ActivationLevel comes from FeatureParam values. If disabled, the decision
  // made by the feature should not be `ACTIVATED`.
  if (initial_activation_level == ActivationLevel::kDisabled) {
    CHECK(*decision != ActivationDecision::ACTIVATED);
    return initial_activation_level;
  }
  // Should only be possible when
  // `features::kEnableFingerprintingProtectionFilter` is false from
  // `FingerprintingProtectionPageActivationThrottle`.
  if (*decision == ActivationDecision::UNKNOWN) {
    return ActivationLevel::kDisabled;
  }
  if (initial_activation_level == ActivationLevel::kDryRun) {
    return initial_activation_level;
  }
  // If we don't have access to `TrackingProtectionSettings`, we don't have a
  // basis to modify the initial activation level anyway.
  if (!tracking_protection_settings_) {
    return initial_activation_level;
  }
  bool enable_fp = features::IsFingerprintingProtectionFeatureEnabled();
  if (features::kEnableOn3pcBlocked.Get()) {
    // The value of prefs::kCookieControlsMode reflects the state of third-party
    // cookies being disabled, i.e. 3PCD is on or user blocks 3PC.
    // TrackingProtectionSettings API only covers 3PCD case.
    enable_fp =
        enable_fp && (static_cast<content_settings::CookieControlsMode>(
                          prefs_->GetInteger(prefs::kCookieControlsMode)) ==
                      content_settings::CookieControlsMode::kBlockThirdParty);
  }

  // Disable the feature if the user a) does not meet the conditions for
  // enabling or b) if they have a Tracking Protection exception for the current
  // URL
  if (!enable_fp) {
    *decision = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET;
    return ActivationLevel::kDisabled;
  } else if (tracking_protection_settings_->GetTrackingProtectionSetting(
                 navigation_handle->GetURL()) == CONTENT_SETTING_ALLOW) {
    *decision = ActivationDecision::URL_ALLOWLISTED;
    return ActivationLevel::kDisabled;
  }

  *decision = ActivationDecision::ACTIVATED;
  DCHECK_NE(initial_activation_level, ActivationLevel::kDisabled);
  return initial_activation_level;
}

content_settings::SettingSource
ProfileInteractionManager::GetTrackingProtectionSettingSource(const GURL& url) {
  content_settings::SettingInfo info;
  tracking_protection_settings_->GetTrackingProtectionSetting(url, &info);
  return info.source;
}

}  // namespace fingerprinting_protection_filter
