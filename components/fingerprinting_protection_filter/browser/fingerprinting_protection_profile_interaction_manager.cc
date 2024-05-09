// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_profile_interaction_manager.h"

#include "base/check_op.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/subresource_filter/content/shared/common/subresource_filter_utils.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"

namespace fingerprinting_protection_filter {

using ::subresource_filter::ActivationDecision;
using ::subresource_filter::mojom::ActivationLevel;

ProfileInteractionManager::ProfileInteractionManager(
    privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings)
    : tracking_protection_settings_(tracking_protection_settings) {}

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
  // We enable fingerprinting protection if the user has turned the feature on
  // in settings.
  // TODO(crbug.com/327005578): Add a FeatureParam-guarded check for users who
  // have third-party cookies blocked, meaning they have toggled this in the
  // settings.
  bool enable_fp =
      tracking_protection_settings_->IsFingerprintingProtectionEnabled();

  // Disable the feature if the user a) does not meet the conditions for
  // enabling or b) if they have a Tracking Protection exception for the current
  // URL
  if (!enable_fp) {
    *decision = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET;
    return ActivationLevel::kDisabled;
  } else if (tracking_protection_settings_->HasTrackingProtectionException(
                 navigation_handle->GetURL())) {
    *decision = ActivationDecision::URL_ALLOWLISTED;
    return ActivationLevel::kDisabled;
  }

  *decision = ActivationDecision::ACTIVATED;
  DCHECK_NE(initial_activation_level, ActivationLevel::kDisabled);
  return initial_activation_level;
}

}  // namespace fingerprinting_protection_filter
