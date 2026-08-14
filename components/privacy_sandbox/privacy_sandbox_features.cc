// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_features.h"

#include "base/feature_list.h"

namespace privacy_sandbox {

BASE_FEATURE(kEnforcePrivacySandboxAttestations,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDefaultAllowPrivacySandboxAttestations,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPrivacySandboxAttestationsLoadFromAPKAsset,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kRelatedWebsiteSetsUi, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivacySandboxAdPrivacyUxDeprecation,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace privacy_sandbox
