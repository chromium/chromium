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

// When true, do not show any privacySandbox dialog when the browser isn't a
// normal browser.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxSuppressDialogOnNonNormalBrowsers);

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
extern const char kPrivacySandboxSettings4ForceRestrictedUserForTestingName[];
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
    kPrivacySandboxSettings4ForceRestrictedUserForTesting;
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings4ShowSampleDataForTesting;

// When true, suppress any Privacy Sandbox dialog if Chrome is launched
// from an external app.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings4SuppressDialogForExternalAppLaunches;

// When true, attempts to close all open dialogs when one dialog's flow has
// been completed. Included as a kill switch.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool> kPrivacySandboxSettings4CloseAllPrompts;

COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kOverridePrivacySandboxSettingsLocalTesting);

// Disables any Privacy Sandbox related prompts. Should only be used for testing
// purposes. This feature is used to support external automated testing using
// Chrome, where additional prompts break behavior expectations.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kDisablePrivacySandboxPrompts);

// Enables the First Party Sets UI.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxFirstPartySetsUI);

// Populates First Party Sets information with sample membership information,
// for testing purposes only.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool> kPrivacySandboxFirstPartySetsUISampleSets;

// Enables enforcement of Privacy Sandbox Enrollment/Attestations.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kEnforcePrivacySandboxAttestations);

// Enable the Privacy Sandbox Attestations to default allow when the
// attestations map is absent.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kDefaultAllowPrivacySandboxAttestations);

// Enables the sentinel file guard for Privacy Sandbox Attestations.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxAttestationSentinel);

// Gives a list of sites permission to use Privacy Sandbox features without
// being officially enrolled.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const char kPrivacySandboxEnrollmentOverrides[];

// Allow the Privacy Sandbox Attestations component registration to use higher
// task priority.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(
    kPrivacySandboxAttestationsHigherComponentRegistrationPriority);

// Enables Privacy Sandbox Proactive Topics Blocking.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxProactiveTopicsBlocking);

// Enables showing the rollback notice for Tracking Protection in settings.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kTrackingProtectionSettingsPageRollbackNotice);

#if BUILDFLAG(IS_ANDROID)
// Forces Tracking Protection Onboarding notice to be shown on all kind of pages
// (not only the ones with secure connection).
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kTrackingProtectionOnboardingSkipSecurePageCheck);
#endif  // BUILDFLAG(IS_ANDROID)

// Enables the Tracking protection Rollback flow.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kTrackingProtectionOnboardingRollback);

// Enables attribution reporting transitional debug reporting for the cookie
// deprecation experiment.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kAttributionDebugReportingCookieDeprecationTesting);

// Enables Private Aggregation debug reporting to be enabled during the
// third-party cookie deprecation experiment.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivateAggregationDebugReportingCookieDeprecationTesting);

#if BUILDFLAG(IS_ANDROID)
// Enables the trackingProtectionNoticeController to notify the
// TrackingProtectionOnboardingService when a notice was requested (Message
// enqueued).
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kTrackingProtectionNoticeRequestTracking);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_FEATURES_H_
