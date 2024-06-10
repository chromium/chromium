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

COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool> kPrivacySandboxAdsNoticeCCTIncludeModeB;
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

// Allow the Privacy Sandbox Attestations component registration to use
// `USER_VISIBLE` task priority.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(
    kPrivacySandboxAttestationsHigherComponentRegistrationPriority);

// Allow the Privacy Sandbox Attestations component registration to use
// `USER_BLOCKING` task priority.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxAttestationsUserBlockingPriority);

// Enables Privacy Sandbox Proactive Topics Blocking.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxProactiveTopicsBlocking);

COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const char kPrivacySandboxProactiveTopicsBlockingIncludeModeBName[];

COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool>
    kPrivacySandboxProactiveTopicsBlockingIncludeModeB;

// Enables showing the rollback notice for Tracking Protection in settings.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kTrackingProtectionSettingsPageRollbackNotice);

#if BUILDFLAG(IS_ANDROID)
// Triggers Tracking Protection Onboarding notice for 100% launch.
// TODO(b/341975190): This flag is for testing only and will be replaced by
// proper onboarding flag once onboarding service is done.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kTrackingProtectionFullOnboardingMobileTrigger);
#endif  // BUILDFLAG(IS_ANDROID)

// Enables attribution reporting transitional debug reporting for the cookie
// deprecation experiment.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kAttributionDebugReportingCookieDeprecationTesting);

// Enables Private Aggregation debug reporting to be enabled during the
// third-party cookie deprecation experiment.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivateAggregationDebugReportingCookieDeprecationTesting);

// Enables chrome://privacy-sandbox-internals DevUI page.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxInternalsDevUI);

// Enables chrome://privacy-sandbox-internals/related-website-internals DevUI
// page. Relies on PrivacySandboxInternalsDevUI also being enabled.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kRelatedWebsiteSetsDevUI);

// Enables fingerprinting protection setting behavior.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kFingerprintingProtectionSetting);

// Enables fingerprinting protection setting UX.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kFingerprintingProtectionUx);

// Enables IP Protection setting behavior.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kIpProtectionV1);

// Enables showing IP Protection toggle on the settings page.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kIpProtectionUx);

// Enables IP Protection by default. For use in dogfood.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kIpProtectionDogfoodDefaultOn);

// Enables showing new RWS UI.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxRelatedWebsiteSetsUi);

// Enables settings UX + behavior for the full Tracking Protection launch.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kTrackingProtectionSettingsLaunch);

// Enables TP settings page to display TRACKING_PROTECTION content settings.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kTrackingProtectionContentSettingInSettings);

// Enables UserBypass to set/reset TRACKING_PROTECTION content settings.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kTrackingProtectionContentSettingUbControl);

// Enables TRACKING_PROTECTION content settings to control 3pcb.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kTrackingProtectionContentSettingFor3pcb);

#if BUILDFLAG(IS_ANDROID)
// Enables UserBypass logic for Progressive Web Apps on Android
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kTrackingProtectionUserBypassPwa);

// Triggers UserBypass logic for Progressive Web Apps on Android
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kTrackingProtectionUserBypassPwaTrigger);
#endif  // BUILDFLAG(IS_ANDROID)

// Enables visibility for toggles on the top-level Ad Privacy page.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPsRedesignAdPrivacyPage);

// Enables setting the toggles on the top-level Ad Privacy page.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool> kPsRedesignAdPrivacyPageEnableToggles;

// Enables IPH reminders for tracking protection features.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kTrackingProtectionReminder);

// Defines if the reminder should be silent.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool> kTrackingProtectionIsSilentReminder;

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

// Disables the Privacy Sandbox Ads Dialog when all 3pc are blocked.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxAdsDialogDisabledOnAll3PCBlock);

COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxPrivacyGuideAdTopics);

// Enables the local version of the notice confirmation logic to run.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
BASE_DECLARE_FEATURE(kPrivacySandboxLocalNoticeConfirmation);

// If true, fallback to the OS country when the variation country isn't
// available.
COMPONENT_EXPORT(PRIVACY_SANDBOX_FEATURES)
extern const base::FeatureParam<bool>
    kPrivacySandboxLocalNoticeConfirmationDefaultToOSCountry;

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_FEATURES_H_
