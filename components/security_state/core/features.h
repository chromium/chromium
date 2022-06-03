// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_STATE_CORE_FEATURES_H_
#define COMPONENTS_SECURITY_STATE_CORE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace security_state {
namespace features {

// This feature enables Safety Tip warnings on possibly-risky sites.
COMPONENT_EXPORT(SECURITY_STATE_FEATURES)
extern const base::Feature kSafetyTipUI;

// This feature enables Safety Tip warnings on some types of lookalike sites,
// for the purposes of measuring Simplified Domain Display
// (https://crbug.com/1090393). It has similar behavior to kSafetyTipUI, but can
// be enabled independently in a separate experiment.
COMPONENT_EXPORT(SECURITY_STATE_FEATURES)
extern const base::Feature kSafetyTipUIForSimplifiedDomainDisplay;

// This feature enables Safety Tip warnings on pages where there is a delayed
// Safe Browsing warning. Has no effect unless safe_browsing::kDelayedWarnings
// is also enabled. Can be enabled independently of kSafetyTipUI.
COMPONENT_EXPORT(SECURITY_STATE_FEATURES)
extern const base::Feature kSafetyTipUIOnDelayedWarning;

}  // namespace features
}  // namespace security_state

#endif  // COMPONENTS_SECURITY_STATE_CORE_FEATURES_H_
