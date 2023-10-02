// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

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

// Enables the third release of the Privacy Sandbox settings.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxSettings3);
// When true, the user will be shown a consent to enable the Privacy Sandbox
// release 3, if they accept the APIs will become active. Only one of this and
// the below notice feature should be enabled at any one time.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool> kPrivacySandboxSettings3ConsentRequired;
// When true, the user will be shown a notice, after which the Privacy Sandbox
// 3 APIs will become active. Only one of this and the above consent feature
// should be enabled at any one time.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool> kPrivacySandboxSettings3NoticeRequired;

// Feature parameters which should exclusively be used for testing purposes.
// Enabling any of these parameters may result in the Privacy Sandbox prefs
// (unsynced) entering an unexpected state, requiring profile deletion to
// resolve.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings3ForceShowConsentForTesting;
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings3ForceShowNoticeForTesting;
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings3ShowSampleDataForTesting;
// This parameter will suppress all Privacy Sandbox prompts, but is supersceeded
// by the kDisablePrivacySandboxPrompts feature below, and will be removed when
// the PrivacySandboxSettings3 feature is fully launched & solidified.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings3DisablePromptForTesting;

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

// Forces profile eligibility for Tracking Protection Onboarding.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kTrackingProtectionOnboardingForceEligibility);

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_FEATURES_H_
