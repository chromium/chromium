// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/core/features.h"

namespace security_state {
namespace features {

const base::Feature kSafetyTipUI{"SafetyTip", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSafetyTipUIForSimplifiedDomainDisplay{
    "SafetyTipForSimplifiedDomainDisplay", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSafetyTipUIOnDelayedWarning{
    "SafetyTipUIOnDelayedWarning", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace security_state
