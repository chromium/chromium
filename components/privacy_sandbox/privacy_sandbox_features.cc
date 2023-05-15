// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_features.h"

namespace privacy_sandbox {

BASE_FEATURE(kPrivacySandboxSettings4,
             "PrivacySandboxSettings4",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kPrivacySandboxSettings3,
             "PrivacySandboxSettings3",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<bool> kPrivacySandboxSettings3ConsentRequired{
    &kPrivacySandboxSettings3, "consent-required", false};
const base::FeatureParam<bool> kPrivacySandboxSettings3NoticeRequired{
    &kPrivacySandboxSettings3, "notice-required", false};

const base::FeatureParam<bool>
    kPrivacySandboxSettings3ForceShowConsentForTesting{
        &kPrivacySandboxSettings3, "force-show-consent-for-testing", false};
const base::FeatureParam<bool>
    kPrivacySandboxSettings3ForceShowNoticeForTesting{
        &kPrivacySandboxSettings3, "force-show-notice-for-testing", false};
const base::FeatureParam<bool> kPrivacySandboxSettings3ShowSampleDataForTesting{
    &kPrivacySandboxSettings3, "show-sample-data", false};
const base::FeatureParam<bool> kPrivacySandboxSettings3DisablePromptForTesting{
    &kPrivacySandboxSettings3, "disable-dialog-for-testing", false};

BASE_FEATURE(kOverridePrivacySandboxSettingsLocalTesting,
             "OverridePrivacySandboxSettingsLocalTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDisablePrivacySandboxPrompts,
             "DisablePrivacySandboxPrompts",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivacySandboxFirstPartySetsUI,
             "PrivacySandboxFirstPartySetsUI",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool> kPrivacySandboxFirstPartySetsUISampleSets{
    &kPrivacySandboxFirstPartySetsUI, "use-sample-sets", false};

BASE_FEATURE(kEnforcePrivacySandboxAttestations,
             "EnforcePrivacySandboxAttestations",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace privacy_sandbox
