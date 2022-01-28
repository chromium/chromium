// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_features.h"

namespace privacy_sandbox {

// Enables the third release of the Privacy Sandbox settings
const base::Feature kPrivacySandboxSettings3{"PrivacySandboxSettings3",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<bool> kPrivacySandboxSettings3ForceShowConsent{
    &kPrivacySandboxSettings3, "force-show-consent", false};
const base::FeatureParam<bool> kPrivacySandboxSettings3ForceShowNotice{
    &kPrivacySandboxSettings3, "force-show-notice", false};

}  // namespace privacy_sandbox
