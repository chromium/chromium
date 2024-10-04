// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_features.h"

#include "base/feature_list.h"

namespace privacy_sandbox {

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPrivacySandboxAdsNoticeCCT,
             "PrivacySandboxAdsNoticeCCT",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kPrivacySandboxAdsNoticeCCTAppIdName[] = "app-id";

const base::FeatureParam<std::string> kPrivacySandboxAdsNoticeCCTAppId{
    &kPrivacySandboxAdsNoticeCCT, kPrivacySandboxAdsNoticeCCTAppIdName, ""};

const base::FeatureParam<bool> kPrivacySandboxAdsNoticeCCTIncludeModeB{
    &kPrivacySandboxAdsNoticeCCT, "include-mode-b", false};
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kPrivacySandboxSettings4,
             "PrivacySandboxSettings4",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kPrivacySandboxSettings4ConsentRequiredName[] = "consent-required";
const char kPrivacySandboxSettings4NoticeRequiredName[] = "notice-required";
const char kPrivacySandboxSettings4RestrictedNoticeName[] = "restricted-notice";
const char kPrivacySandboxSettings4ForceShowConsentForTestingName[] =
    "force-show-consent-for-testing";
const char kPrivacySandboxSettings4ForceShowNoticeRowForTestingName[] =
    "force-show-notice-row-for-testing";
const char kPrivacySandboxSettings4ForceShowNoticeEeaForTestingName[] =
    "force-show-notice-eea-for-testing";
const char kPrivacySandboxSettings4ForceShowNoticeRestrictedForTestingName[] =
    "force-show-notice-restricted-for-testing";
const char kPrivacySandboxSettings4ForceRestrictedUserForTestingName[] =
    "force-restricted-user";
const char kPrivacySandboxSettings4ShowSampleDataForTestingName[] =
    "show-sample-data";

const base::FeatureParam<bool> kPrivacySandboxSettings4ConsentRequired{
    &kPrivacySandboxSettings4, kPrivacySandboxSettings4ConsentRequiredName,
    false};
const base::FeatureParam<bool> kPrivacySandboxSettings4NoticeRequired{
    &kPrivacySandboxSettings4, kPrivacySandboxSettings4NoticeRequiredName,
    false};
const base::FeatureParam<bool> kPrivacySandboxSettings4RestrictedNotice{
    &kPrivacySandboxSettings4, kPrivacySandboxSettings4RestrictedNoticeName,
    false};

const base::FeatureParam<bool>
    kPrivacySandboxSettings4ForceShowConsentForTesting{
        &kPrivacySandboxSettings4,
        kPrivacySandboxSettings4ForceShowConsentForTestingName, false};
const base::FeatureParam<bool>
    kPrivacySandboxSettings4ForceShowNoticeRowForTesting{
        &kPrivacySandboxSettings4,
        kPrivacySandboxSettings4ForceShowNoticeRowForTestingName, false};
const base::FeatureParam<bool>
    kPrivacySandboxSettings4ForceShowNoticeEeaForTesting{
        &kPrivacySandboxSettings4,
        kPrivacySandboxSettings4ForceShowNoticeEeaForTestingName, false};
const base::FeatureParam<bool>
    kPrivacySandboxSettings4ForceShowNoticeRestrictedForTesting{
        &kPrivacySandboxSettings4,
        kPrivacySandboxSettings4ForceShowNoticeRestrictedForTestingName, false};
const base::FeatureParam<bool>
    kPrivacySandboxSettings4ForceRestrictedUserForTesting{
        &kPrivacySandboxSettings4,
        kPrivacySandboxSettings4ForceRestrictedUserForTestingName, false};
const base::FeatureParam<bool> kPrivacySandboxSettings4ShowSampleDataForTesting{
    &kPrivacySandboxSettings4,
    kPrivacySandboxSettings4ShowSampleDataForTestingName, false};

const base::FeatureParam<bool>
    kPrivacySandboxSettings4SuppressDialogForExternalAppLaunches{
        &kPrivacySandboxSettings4, "suppress-dialog-for-external-app-launches",
        true};

BASE_FEATURE(kOverridePrivacySandboxSettingsLocalTesting,
             "OverridePrivacySandboxSettingsLocalTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDisablePrivacySandboxPrompts,
             "DisablePrivacySandboxPrompts",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivacySandboxFirstPartySetsUI,
             "PrivacySandboxFirstPartySetsUI",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<bool> kPrivacySandboxFirstPartySetsUISampleSets{
    &kPrivacySandboxFirstPartySetsUI, "use-sample-sets", false};

BASE_FEATURE(kEnforcePrivacySandboxAttestations,
             "EnforcePrivacySandboxAttestations",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDefaultAllowPrivacySandboxAttestations,
             "DefaultAllowPrivacySandboxAttestations",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kPrivacySandboxEnrollmentOverrides[] =
    "privacy-sandbox-enrollment-overrides";

BASE_FEATURE(kPrivacySandboxAttestationsLoadPreInstalledComponent,
             "PrivacySandboxAttestationsLoadPreInstalledComponent",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrivacySandboxProactiveTopicsBlocking,
             "PrivacySandboxProactiveTopicsBlocking",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAttributionDebugReportingCookieDeprecationTesting,
             "AttributionDebugReportingCookieDeprecationTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivateAggregationDebugReportingCookieDeprecationTesting,
             "PrivateAggregationDebugReportingCookieDeprecationTesting",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrivateAggregationDebugReportingIgnoreSiteExceptions,
             "PrivateAggregationDebugReportingIgnoreSiteExceptions",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivacySandboxInternalsDevUI,
             "PrivacySandboxInternalsDevUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRelatedWebsiteSetsDevUI,
             "RelatedWebsiteSetsDevUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFingerprintingProtectionUx,
             "FingerprintingProtectionUx",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAddLimit3pcsSetting,
             "AddLimit3pcsSetting",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAlwaysBlock3pcsIncognito,
             "AlwaysBlock3pcsIncognito",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTrackingProtection3pcdUx,
             "TrackingProtection3pcdUx",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFingerprintingProtectionUserBypass,
             "FingerprintingProtectionUserBypass",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIpProtectionV1,
             "IpProtectionV1",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIpProtectionDogfoodDefaultOn,
             "IpProtectionDogfoodDefaultOn",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIpProtectionUx,
             "IpProtectionUx",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIpProtectionUserBypass,
             "IpProtectionUserBypass",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivacySandboxRelatedWebsiteSetsUi,
             "PrivacySandboxRelatedWebsiteSetsUi",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTrackingProtectionContentSettingInSettings,
             "TrackingProtectionContentSettingInSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTrackingProtectionContentSettingUbControl,
             "TrackingProtectionContentSettingUbControl",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTrackingProtectionContentSettingFor3pcb,
             "TrackingProtectionContentSettingFor3pcb",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kTrackingProtectionUserBypassPwa,
             "TrackingProtectionUserBypassPwa",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTrackingProtectionUserBypassPwaTrigger,
             "TrackingProtectionUserBypassPwaTrigger",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kPsDualWritePrefsToNoticeStorage,
             "PsDualWritePrefsToNoticeStorage",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivateStateTokensDevUI,
             "PrivateStateTokensDevUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivacySandboxActivityTypeStorage,
             "PrivacySandboxActivityTypeStorage",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kPrivacySandboxActivityTypeStorageLastNLaunchesName[] =
    "last-n-launches";

const base::FeatureParam<int> kPrivacySandboxActivityTypeStorageLastNLaunches{
    &kPrivacySandboxActivityTypeStorage,
    kPrivacySandboxActivityTypeStorageLastNLaunchesName, 100};

const char kPrivacySandboxActivityTypeStorageWithinXDaysName[] =
    "within-x-days";

const base::FeatureParam<int> kPrivacySandboxActivityTypeStorageWithinXDays{
    &kPrivacySandboxActivityTypeStorage,
    kPrivacySandboxActivityTypeStorageWithinXDaysName, 60};

const char kPrivacySandboxActivityTypeStorageSkipPreFirstTabName[] =
    "skip-pre-first-tab";

const base::FeatureParam<bool>
    kPrivacySandboxActivityTypeStorageSkipPreFirstTab{
        &kPrivacySandboxActivityTypeStorage,
        kPrivacySandboxActivityTypeStorageSkipPreFirstTabName, false};

BASE_FEATURE(kPrivacySandboxPrivacyGuideAdTopics,
             "PrivacySandboxPrivacyGuideAdTopics",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivacySandboxMigratePrefsToNoticeConsentDataModel,
             "PrivacySandboxMigratePrefsToNoticeConsentDataModel",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrivacySandboxPrivacyPolicy,
             "PrivacySandboxPrivacyPolicy",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivacySandboxNoticeQueue,
             "PrivacySandboxNoticeQueue",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivacySandboxSentimentSurvey,
             "PrivacySandboxSentimentSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kPrivacySandboxSentimentSurveyTriggerId{
    &kPrivacySandboxSentimentSurvey, "sentiment-survey-trigger-id", ""};

}  // namespace privacy_sandbox
