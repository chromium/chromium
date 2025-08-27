// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_features.h"

#include "base/feature_list.h"

namespace privacy_sandbox {

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(PrivacySandboxAdsNoticeCCT, base::FEATURE_ENABLED_BY_DEFAULT);

const char kPrivacySandboxAdsNoticeCCTAppIdName[] = "app-id";
const char kAndroidGoogleSearchAppIdName[] =
    "com.google.android.googlequicksearchbox";

const base::FeatureParam<std::string> kPrivacySandboxAdsNoticeCCTAppId{
    &kPrivacySandboxAdsNoticeCCT, kPrivacySandboxAdsNoticeCCTAppIdName,
    kAndroidGoogleSearchAppIdName};
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(PrivacySandboxSettings4, base::FEATURE_ENABLED_BY_DEFAULT);

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
const base::FeatureParam<bool> kPrivacySandboxSettings4ShowSampleDataForTesting{
    &kPrivacySandboxSettings4,
    kPrivacySandboxSettings4ShowSampleDataForTestingName, false};

const base::FeatureParam<bool>
    kPrivacySandboxSettings4SuppressDialogForExternalAppLaunches{
        &kPrivacySandboxSettings4, "suppress-dialog-for-external-app-launches",
        true};

BASE_FEATURE(OverridePrivacySandboxSettingsLocalTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(DisablePrivacySandboxPrompts, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(EnforcePrivacySandboxAttestations,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(DefaultAllowPrivacySandboxAttestations,
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(DefaultAllowPrivacySandboxAttestations,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

const char kPrivacySandboxEnrollmentOverrides[] =
    "privacy-sandbox-enrollment-overrides";

BASE_FEATURE(AttributionDebugReportingCookieDeprecationTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(PrivacySandboxAttestationsLoadFromAPKAsset,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(PrivateAggregationDebugReportingCookieDeprecationTesting,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(PrivateAggregationDebugReportingIgnoreSiteExceptions,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(PrivacySandboxInternalsDevUI, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(RelatedWebsiteSetsDevUI, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(AlwaysBlock3pcsIncognito, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(FingerprintingProtectionUx, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(IpProtectionUx, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ActUserBypassUx, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(TrackingProtectionContentSettingIn3pcUx,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(TrackingProtectionContentSettingFor3pcb,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(RelatedWebsiteSetsUi, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(TrackingProtectionUserBypassPwa,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(TrackingProtectionUserBypassPwaTrigger,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(DisplayWildcardInContentSettings,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(PsDualWritePrefsToNoticeStorage, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(PrivateStateTokensDevUI, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(PrivacySandboxActivityTypeStorage,
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

BASE_FEATURE(PrivacySandboxAdTopicsContentParity,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(PrivacySandboxNoticeQueue, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(PrivacySandboxSentimentSurvey, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kPrivacySandboxSentimentSurveyTriggerId{
    &kPrivacySandboxSentimentSurvey, "sentiment-survey-trigger-id", ""};

BASE_FEATURE(PrivacySandboxAdsApiUxEnhancements,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(PrivacySandboxAllowPromptForBlocked3PCookies,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(PrivacySandboxMigratePrefsToSchemaV2,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(PrivacySandboxNoticeFramework, base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace privacy_sandbox
