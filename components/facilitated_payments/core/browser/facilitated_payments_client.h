// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_CLIENT_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_CLIENT_H_

#include <cstdint>

#include "base/functional/callback_forward.h"

namespace payments::facilitated {

// A cross-platform client interface for showing UI for non-form based FOPs.
class FacilitatedPaymentsClient {
 public:
  virtual ~FacilitatedPaymentsClient() = default;

  // Shows the user's PIX accounts from their Google Wallet, and prompts to pay.
  // If the UI was shown, then returns true and later invokes the `callback`
  // with the result of user's selection: a boolean for acceptance or
  // cancellation and the selected instrument ID in case of acceptance. If the
  // UI was not shown, then returns false and does not invoke the callback.
  virtual bool ShowPixPaymentPrompt(
      base::OnceCallback<void(bool, int64_t)> on_user_decision_callback);
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_CLIENT_H_
