// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_VERIFICATION_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_VERIFICATION_VIEW_H_

namespace autofill {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.autofill.autofill_ai
enum class AutofillAiVerificationViewState {
  kConfirm = 1,
  kReview = 2,
};

// The view, in the MVC sense, of the verification UI: its controller,
// `AutofillAiVerificationSession`, drives the platform-specific UI (e.g. a
// bubble or a bottom sheet) through this interface.
class AutofillAiVerificationView {
 public:
  using ViewState = AutofillAiVerificationViewState;

  virtual ~AutofillAiVerificationView() = default;

  // Shows the prompt in `initial_state`. Returns true if shown successfully.
  virtual bool Show(ViewState initial_state) = 0;

  // Transitions the prompt to `state`. The implementation updates its position
  // for the new state.
  virtual void SetState(ViewState state) = 0;

  // Hides the prompt.
  virtual void Hide() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_VERIFICATION_VIEW_H_
