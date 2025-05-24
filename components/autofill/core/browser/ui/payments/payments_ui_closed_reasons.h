// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_UI_CLOSED_REASONS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_UI_CLOSED_REASONS_H_

namespace autofill {

// The reason why payments bubbles, dialogs, or other UI are closed.
// A java IntDef@ is generated from this.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
enum class PaymentsUiClosedReason {
  // UI closed reason not specified.
  kUnknown,
  // The user explicitly accepted the UI.
  kAccepted,
  // The user explicitly cancelled the UI.
  kCancelled,
  // The user explicitly closed the UI (via the close button or the ESC).
  kClosed,
  // The UI was not interacted.
  kNotInteracted,
  // The UI lost focus and was deactivated.
  kLostFocus,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_UI_CLOSED_REASONS_H_
