// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_FEATURES_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_FEATURES_H_

namespace privacy_sandbox {

// Enables enforcement of Privacy Sandbox Enrollment/Attestations.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kEnforcePrivacySandboxAttestations);

// Enable the Privacy Sandbox Attestations to default allow when the
// attestations map is absent.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kDefaultAllowPrivacySandboxAttestations);

#if BUILDFLAG(IS_ANDROID)
// Allow the Privacy Sandbox Attestations component to load the pre-installed
// attestation list from Android APK assets.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxAttestationsLoadFromAPKAsset);
#endif

// Enables showing RWS UI.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kRelatedWebsiteSetsUi);

// If true, the Ad Topics, Site Suggested Ads, and Ad Measurement API prefs are
// set to false for users on startup. The UX to access these Ad Privacy settings
// will be removed from all surfaces.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxAdPrivacyUxDeprecation);

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_FEATURES_H_
