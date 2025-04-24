// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/features.h"

namespace policy::features {

BASE_FEATURE(kPolicyBlocklistProceedUntilResponse,
             "PolicyBlocklistProceedUntilResponse",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kProfileSeparationDomainExceptionListRetroactive,
             "ProfileSeparationDomainExceptionListRetroactive",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnhancedSecurityEventFields,
             "EnhancedSecurityEventFields",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Kiosk session for the Helium android app.
BASE_FEATURE(kHeliumArcvmKiosk,
             "HeliumArcvmKiosk",
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsHeliumArcvmKioskEnabled() {
  return base::FeatureList::IsEnabled(kHeliumArcvmKiosk);
}

}  // namespace policy::features
