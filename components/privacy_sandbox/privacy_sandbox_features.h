// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_FEATURES_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_FEATURES_H_

namespace privacy_sandbox {

// Enables the third release of the Privacy Sandbox settings.
extern const base::Feature kPrivacySandboxSettings3;
// Determines whether the user facing controls for Privacy Sandbox Settings 3
// should be default on.
extern const base::FeatureParam<bool> kPrivacySandboxSettings3DefaultOn;
// When true, the user will be shown a consent to enable the Privacy Sandbox
// release 3, if they accept the APIs will become active. Only one of this and
// the below notice feature should be enabled at any one time.
extern const base::FeatureParam<bool> kPrivacySandboxSettings3ConsentRequired;
// When true, the user will be shown a notice, after which the Privacy Sandbox
// 3 APIs will become active. Only one of this and the above consent feature
// should be enabled at any one time.
extern const base::FeatureParam<bool> kPrivacySandboxSettings3NoticeRequired;
// Determines whether the user will be shown a new version of the notice UI.
// The notice will be shown only if `kPrivacySandboxSettings3NoticeRequired` is
// true. This parameter only determines which UI version will be shown.
extern const base::FeatureParam<bool> kPrivacySandboxSettings3NewNotice;

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
    kPrivacySandboxSettings3DisableDialogForTesting;

extern const base::Feature kOverridePrivacySandboxSettingsLocalTesting;

// Disables any Privacy Sandbox related prompts. Should only be used for testing
// purposes. This feature is used to support external automated testing using
// Chrome, where additional prompts break behavior expectations.
extern const base::Feature kDisablePrivacySandboxPrompts;

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_FEATURES_H_
