// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_annotator_debug_features.h"

namespace accessibility_annotator::features::debug {

// When enabled, only feature checks are performed on
// AccessibilityAnnotatorEnablementService. All other checks are skipped.
BASE_FEATURE(kAccessibilityAnnotatorForceEnablementState,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kAccessibilityAnnotatorForceEnablementStateParam,
                   &kAccessibilityAnnotatorForceEnablementState,
                   "remote_annotator_enablement_state",
                   1);
}  // namespace accessibility_annotator::features::debug
