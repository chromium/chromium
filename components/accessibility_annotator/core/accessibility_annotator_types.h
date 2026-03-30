// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_TYPES_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_TYPES_H_

namespace accessibility_annotator {

// Tracks the global enablement state of the feature for the current profile.
// Used by consuming features to determine both feature execution and UI
// entrypoint visibility.
enum class RemoteAnnotatorEnablementState {
  kDisabledNotEligible = 0,   // Feature disabled, user not eligible.
  kDisabledPendingInfo = 1,   // Feature disabled pending Info.
  kDisabledPendingSetup = 2,  // Feature disabled pending Setup.
  kEnabled = 3                // Feature enabled, first run completed.
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_TYPES_H_
