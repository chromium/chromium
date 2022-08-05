// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_features.h"

namespace privacy_sandbox {

const base::Feature kPrivacySandboxSettings3{"PrivacySandboxSettings3",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<bool> kPrivacySandboxSettings3ConsentRequired{
    &kPrivacySandboxSettings3, "consent-required", false};
const base::FeatureParam<bool> kPrivacySandboxSettings3NoticeRequired{
    &kPrivacySandboxSettings3, "notice-required", false};
const base::FeatureParam<bool> kPrivacySandboxSettings3NewNotice{
    &kPrivacySandboxSettings3, "new-notice", false};

const base::FeatureParam<bool>
    kPrivacySandboxSettings3ForceShowConsentForTesting{
        &kPrivacySandboxSettings3, "force-show-consent-for-testing", false};
const base::FeatureParam<bool>
    kPrivacySandboxSettings3ForceShowNoticeForTesting{
        &kPrivacySandboxSettings3, "force-show-notice-for-testing", false};
const base::FeatureParam<bool> kPrivacySandboxSettings3ShowSampleDataForTesting{
    &kPrivacySandboxSettings3, "show-sample-data", false};
const base::FeatureParam<bool> kPrivacySandboxSettings3DisableDialogForTesting{
    &kPrivacySandboxSettings3, "disable-dialog-for-testing", false};

const base::Feature kOverridePrivacySandboxSettingsLocalTesting{
    "OverridePrivacySandboxSettingsLocalTesting",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDisablePrivacySandboxPrompts{
    "DisablePrivacySandboxPrompts", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPrivacySandboxFirstPartySetsUI{
    "PrivacySandboxFirstPartySetsUI", base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<bool> kPrivacySandboxFirstPartySetsUISampleSets{
    &kPrivacySandboxFirstPartySetsUI, "use-sample-sets", false};

}  // namespace privacy_sandbox
