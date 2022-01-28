// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_FEATURES_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_FEATURES_H_

namespace privacy_sandbox {

extern const base::Feature kPrivacySandboxSettings3;
extern const base::FeatureParam<bool> kPrivacySandboxSettings3ForceShowConsent;
extern const base::FeatureParam<bool> kPrivacySandboxSettings3ForceShowNotice;

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_FEATURES_H_
