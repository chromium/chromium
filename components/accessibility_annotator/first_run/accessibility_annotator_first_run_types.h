// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_TYPES_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_TYPES_H_

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

// First run trigger result.
enum class FirstRunTriggerResult {
  kSuccess = 0,
  kIgnoredNotEligible = 1,
  kIgnoredAlreadyEnabled = 2,
};

// Notice action result. Used internally, not exposed to consuming features.
enum class InfoResult {
  kAcknowledged = 0,     // User clicked "Got it"
  kNotAcknowledged = 1,  // User hit 'ESC' or clicked elsewhere, closing the
                         // Info without acknowledging.
};

// Source of the first run invocation.
enum class FirstRunInvocationSource {
  kAutofill = 0,
  kAutoTriggerPromo = 1,
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_TYPES_H_
