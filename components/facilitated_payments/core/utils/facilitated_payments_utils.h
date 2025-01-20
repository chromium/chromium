// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_UTILS_FACILITATED_PAYMENTS_UTILS_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_UTILS_FACILITATED_PAYMENTS_UTILS_H_

namespace payments::facilitated {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.facilitated_payments
// The result of invoking the purchase manager with an action token.
enum class PurchaseActionResult : int {
  // Could not invoke the purchase manager.
  kCouldNotInvoke,

  // The purchase manager was invoked successfully.
  kResultOk,

  // The user cancelled out of the purchase manager flow.
  kResultCanceled,
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_UTILS_FACILITATED_PAYMENTS_UTILS_H_
