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

// Enables the fourth release of the Privacy Sandbox settings.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxSettings4);

// Split out name definitions since about_flags otherwise complains about the
// features having static initializers. Not sure if there is a better solution
// that both allows usage of these params in about_flags.cc and usage of the
// feature in code that is compiled into different components.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const char kPrivacySandboxSettings4ConsentRequiredName[];
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const char kPrivacySandboxSettings4NoticeRequiredName[];
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const char kPrivacySandboxSettings4RestrictedNoticeName[];
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const char kPrivacySandboxSettings4ForceShowConsentForTestingName[];
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const char kPrivacySandboxSettings4ForceShowNoticeRowForTestingName[];
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const char kPrivacySandboxSettings4ForceShowNoticeEeaForTestingName[];
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const char
    kPrivacySandboxSettings4ForceShowNoticeRestrictedForTestingName[];
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const char kPrivacySandboxSettings4ShowSampleDataForTestingName[];

// When true, the user will be shown a consent to enable the Privacy Sandbox
// release 4, if they accept the APIs will become active. Only one of this and
// the below notice feature should be enabled at any one time.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool> kPrivacySandboxSettings4ConsentRequired;
// When true, the user will be shown a notice, after which the Privacy Sandbox
// 4 APIs will become active. Only one of this and the above consent feature
// should be enabled at any one time.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool> kPrivacySandboxSettings4NoticeRequired;

// When true, the user could be shown a Privacy Sandbox restricted notice.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool> kPrivacySandboxSettings4RestrictedNotice;

// Feature parameters which should exclusively be used for testing purposes.
// Enabling any of these parameters may result in the Privacy Sandbox prefs
// (unsynced) entering an unexpected state, requiring profile deletion to
// resolve.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings4ForceShowConsentForTesting;
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings4ForceShowNoticeRowForTesting;
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings4ForceShowNoticeEeaForTesting;
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings4ForceShowNoticeRestrictedForTesting;
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings4ShowSampleDataForTesting;

COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kOverridePrivacySandboxSettingsLocalTesting);

// Disables any Privacy Sandbox related prompts. Should only be used for testing
// purposes. This feature is used to support external automated testing using
// Chrome, where additional prompts break behavior expectations.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kDisablePrivacySandboxPrompts);

// Enables enforcement of Privacy Sandbox Enrollment/Attestations.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kEnforcePrivacySandboxAttestations);

// Enable the Privacy Sandbox Attestations to default allow when the
// attestations map is absent.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kDefaultAllowPrivacySandboxAttestations);

// Gives a list of sites permission to use Privacy Sandbox features without
// being officially enrolled.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const char kPrivacySandboxEnrollmentOverrides[];

#if BUILDFLAG(IS_ANDROID)
// Allow the Privacy Sandbox Attestations component to load the pre-installed
// attestation list from Android APK assets.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxAttestationsLoadFromAPKAsset);
#endif

// Prevents site-level exceptions from permitting Private Aggregation debug
// reporting if third-party cookies are generally blocked.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivateAggregationDebugReportingIgnoreSiteExceptions);

// Enables chrome://privacy-sandbox-internals DevUI page.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxInternalsDevUI);

// Enables chrome://privacy-sandbox-internals/related-website-internals DevUI
// page. Relies on PrivacySandboxInternalsDevUI also being enabled.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kRelatedWebsiteSetsDevUI);

// Privacy UX features start

// Enables showing RWS UI.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kRelatedWebsiteSetsUi);

// Privacy UX features end

// Enables the notice storage for pref storage.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPsDualWritePrefsToNoticeStorage);

// Enables Activity Type Storage
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxActivityTypeStorage);

COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const char kPrivacySandboxActivityTypeStorageLastNLaunchesName[];


// Enables chrome://privacy-sandbox-internals/private-state-tokens DevUI
// page. Relies on PrivacySandboxInternalsDevUI also being enabled.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivateStateTokensDevUI);

COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<int>
    kPrivacySandboxActivityTypeStorageLastNLaunches;

COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const char kPrivacySandboxActivityTypeStorageWithinXDaysName[];

COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<int>
    kPrivacySandboxActivityTypeStorageWithinXDays;

COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool>
    kPrivacySandboxActivityTypeStorageSkipPreFirstTab;

COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxAdTopicsContentParity);

#if BUILDFLAG(IS_ANDROID)
// The delay in milliseconds between the first click and the next accepted
// click.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<int> kPrivacySandboxDebouncingDelayMilliseconds;

#endif  // BUILDFLAG(IS_ANDROID)

// If true, displays the Ads APIs UX Enancements.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxAdsApiUxEnhancements);

// If true, enable showing notices through the notice framework.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxNoticeFramework);

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_FEATURES_H_
