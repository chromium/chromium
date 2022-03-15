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
extern const base::FeatureParam<bool> kPrivacySandboxSettings3ConsentRequired;

// Feature parameters which should exclusively be used for testing purposes.
// Enabling any of these parameters may result in the Privacy Sandbox prefs
// (unsynced) entering an unexpected state, requiring profile deletion to
// resolve.
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings3DisableDialogForTesting;
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings3ForceShowConsentForTesting;
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings3ForceShowNoticeForTesting;
extern const base::FeatureParam<bool>
    kPrivacySandboxSettings3ShowSampleDataForTesting;

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_FEATURES_H_
