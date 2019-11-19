// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/core/features.h"

namespace security_state {
namespace features {

const base::Feature kMarkHttpAsFeature{"MarkHttpAs",
                                       base::FEATURE_ENABLED_BY_DEFAULT};
const char kMarkHttpAsFeatureParameterName[] = "treatment";
const char kMarkHttpAsParameterDangerous[] = "dangerous";
const char kMarkHttpAsParameterWarningAndDangerousOnFormEdits[] =
    "warning-and-dangerous-on-form-edits";
const char kMarkHttpAsParameterDangerWarning[] = "danger-warning";

const base::Feature kLegacyTLSWarnings{"LegacyTLSWarnings",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSafetyTipUI{"SafetyTip",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace security_state
