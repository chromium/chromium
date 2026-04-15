// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_TYPES_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_TYPES_H_

namespace accessibility_annotator {

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

// The outcome of a request to show the Accessibility Annotator info dialog.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(InfoShowRequestResult)
enum class InfoShowRequestResult {
  kShown = 0,
  kAccepted = 1,
  kDismissed = 2,
  kMaxValue = kDismissed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility_annotator/enums.xml:AccessibilityAnnotatorRemoteAnnotatorInfo)

// Source of the first run invocation.
enum class FirstRunInvocationSource {
  kAutofill = 0,
  kAutoTriggerPromo = 1,
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_TYPES_H_
