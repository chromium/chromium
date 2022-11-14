// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_FEATURES_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_FEATURES_H_

namespace privacy_sandbox {

// Enables the fourth release of the Privacy Sandbox settings.
BASE_DECLARE_FEATURE(kPrivacySandboxSettings4);
// When true, the user will be shown a consent to enable the Privacy Sandbox
// release 4, if they accept the APIs will become active. Only one of this and
// the below notice feature should be enabled at any one time.
extern const base::FeatureParam<bool> kPrivacySandboxSettings4ConsentRequired;
// When true, the user will be shown a notice, after which the Privacy Sandbox
// 4 APIs will become active. Only one of this and the above consent feature
// should be enabled at any one time.
extern const base::FeatureParam<bool> kPrivacySandboxSettings4NoticeRequired;

// Feature parameters which should exclusively be used for testing purposes.
// Enabling any of these parameters may result in the Privacy Sandbox prefs
// (unsynced) entering an unexpected state, requiring profile deletion to
// resolve.
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings4ForceShowConsentForTesting;
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings4ForceShowNoticeRowForTesting;
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings4ForceShowNoticeEeaForTesting;

// Enables the third release of the Privacy Sandbox settings.
BASE_DECLARE_FEATURE(kPrivacySandboxSettings3);
// When true, the user will be shown a consent to enable the Privacy Sandbox
// release 3, if they accept the APIs will become active. Only one of this and
// the below notice feature should be enabled at any one time.
extern const base::FeatureParam<bool> kPrivacySandboxSettings3ConsentRequired;
// When true, the user will be shown a notice, after which the Privacy Sandbox
// 3 APIs will become active. Only one of this and the above consent feature
// should be enabled at any one time.
extern const base::FeatureParam<bool> kPrivacySandboxSettings3NoticeRequired;

// Feature parameters which should exclusively be used for testing purposes.
// Enabling any of these parameters may result in the Privacy Sandbox prefs
// (unsynced) entering an unexpected state, requiring profile deletion to
// resolve.
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings3ForceShowConsentForTesting;
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings3ForceShowNoticeForTesting;
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings3ShowSampleDataForTesting;
// This parameter will suppress all Privacy Sandbox prompts, but is supersceeded
// by the kDisablePrivacySandboxPrompts feature below, and will be removed when
// the PrivacySandboxSettings3 feature is fully launched & solidified.
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings3DisablePromptForTesting;

BASE_DECLARE_FEATURE(kOverridePrivacySandboxSettingsLocalTesting);

// Disables any Privacy Sandbox related prompts. Should only be used for testing
// purposes. This feature is used to support external automated testing using
// Chrome, where additional prompts break behavior expectations.
BASE_DECLARE_FEATURE(kDisablePrivacySandboxPrompts);

// Enables the First Party Sets UI.
BASE_DECLARE_FEATURE(kPrivacySandboxFirstPartySetsUI);

// Populates First Party Sets information with sample membership information,
// for testing purposes only.
extern const base::FeatureParam<bool> kPrivacySandboxFirstPartySetsUISampleSets;

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_FEATURES_H_
