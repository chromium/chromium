// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_CLIENT_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_CLIENT_H_

namespace payments::facilitated {

// A cross-platform client interface for showing UI for non-form based FOPs.
class FacilitatedPaymentsClient {
 public:
  virtual ~FacilitatedPaymentsClient() = default;

  // Shows the user's PIX accounts from their Google Wallet, and prompts to pay.
  virtual bool ShowPixPaymentPrompt();
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_CLIENT_H_
