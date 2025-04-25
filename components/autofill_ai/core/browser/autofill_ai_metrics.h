// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_METRICS_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_METRICS_H_

namespace autofill_ai {

// Logs metrics related to the user seeing an IPH, accepting it and eventually
// seeing or accepting the FFR dialog.
enum class AutofillAiOptInFunnelEvents {
  kIphShown = 0,
  kFFRDialogShown = 1,
  kFFRLearnMoreButtonClicked = 2,
  kFFRDialogAccepted = 3,
  kMaxValue = kFFRDialogAccepted,
};

void LogOptInFunnelEvent(AutofillAiOptInFunnelEvents event);

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_METRICS_H_
