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

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxAdsNoticeCCT);

COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const char kPrivacySandboxAdsNoticeCCTAppIdName[];

COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<std::string> kPrivacySandboxAdsNoticeCCTAppId;
#endif  // BUILDFLAG(IS_ANDROID)

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

// When true, suppress any Privacy Sandbox dialog if Chrome is launched
// from an external app.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings4SuppressDialogForExternalAppLaunches;

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

// Enables attribution reporting transitional debug reporting for the cookie
// deprecation experiment.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kAttributionDebugReportingCookieDeprecationTesting);

// Enables Private Aggregation debug reporting to be enabled during the
// third-party cookie deprecation experiment.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivateAggregationDebugReportingCookieDeprecationTesting);

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

// Enables fingerprinting protection setting UX.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kFingerprintingProtectionUx);

// Enables showing IP Protection toggle on the settings page.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kIpProtectionUx);

// Enables showing RWS UI.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kRelatedWebsiteSetsUi);

// Feature for rolling back Mode B.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kRollBackModeB);

// Forces Mode B rollback without checking the 3pcd onboarding pref.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool> kRollBackModeBForced;

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

// If true, adds the privacy sandbox notice to product messaging controller
// queue.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxNoticeQueue);

// Enables the `Always on` sentiment survey
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxSentimentSurvey);

COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<std::string>
    kPrivacySandboxSentimentSurveyTriggerId;

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
