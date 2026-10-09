// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_GMAIL_OTP_OPT_IN_RESULT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_GMAIL_OTP_OPT_IN_RESULT_H_

namespace autofill {

// Outcome of the Gmail OTP opt-in prompt.
enum class GmailOtpOptInResult {
  // The bubble closed for an unspecified reason.
  kUnknown = 0,
  // The user accepted the prompt by clicking the "Turn on" button.
  kAccepted = 1,
  // The user declined the prompt by clicking the "No thanks" button.
  kDeclined = 2,
  // The user dismissed the bubble via the close ("X") button or Escape key.
  kClosed = 3,
  // The bubble closed because it lost focus (e.g., clicking outside).
  kLostFocus = 4,
  // The pending request in `BubbleManager` was dropped from the queue without
  // finishing (e.g., timed out, preempted/replaced, or torn down on tab
  // closure).
  kDiscarded = 5,
  kMaxValue = kDiscarded,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_GMAIL_OTP_OPT_IN_RESULT_H_
