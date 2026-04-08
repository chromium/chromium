// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_DEBUG_FEATURES_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_DEBUG_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

// The features in this namespace contains are not meant to be rolled out. They
// are only intended for manual debugging and testing purposes.
namespace accessibility_annotator::features::debug {

BASE_DECLARE_FEATURE(kAccessibilityAnnotatorForceEnablementState);
BASE_DECLARE_FEATURE_PARAM(int,
                           kAccessibilityAnnotatorForceEnablementStateParam);

}  // namespace accessibility_annotator::features::debug

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_DEBUG_FEATURES_H_
