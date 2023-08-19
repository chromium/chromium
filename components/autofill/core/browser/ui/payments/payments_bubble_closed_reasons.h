// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_BUBBLE_CLOSED_REASONS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_BUBBLE_CLOSED_REASONS_H_

namespace autofill {

// The reason why payments bubbles are closed.
// A java IntDef@ is generated from this.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
enum class PaymentsBubbleClosedReason {
  // Bubble closed reason not specified.
  kUnknown,
  // The user explicitly accepted the bubble.
  kAccepted,
  // The user explicitly cancelled the bubble.
  kCancelled,
  // The user explicitly closed the bubble (via the close button or the ESC).
  kClosed,
  // The bubble was not interacted.
  kNotInteracted,
  // The bubble lost focus and was deactivated.
  kLostFocus,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_BUBBLE_CLOSED_REASONS_H_
