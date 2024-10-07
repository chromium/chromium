// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/features.h"

namespace policy::features {

BASE_FEATURE(kPolicyBlocklistProceedUntilResponse,
             "PolicyBlocklistProceedUntilResponse",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnablePolicyBanner,
             "EnablePolicyBanner",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace policy::features
