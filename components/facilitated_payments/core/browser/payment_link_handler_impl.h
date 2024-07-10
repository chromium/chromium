// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PAYMENT_LINK_HANDLER_IMPL_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PAYMENT_LINK_HANDLER_IMPL_H_

#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "url/gurl.h"

namespace payments::facilitated {

// The core implementation for handling payment links in Chromium. This class
// orchestrates the process of triggering eWallet push payments, including
// checking against allowlists and initiating the necessary prompts.
class PaymentLinkHandlerImpl {
 public:
  PaymentLinkHandlerImpl();
  ~PaymentLinkHandlerImpl();

  PaymentLinkHandlerImpl(const PaymentLinkHandlerImpl&) = delete;
  PaymentLinkHandlerImpl& operator=(const PaymentLinkHandlerImpl&) = delete;

  // Initiates the eWallet push payment flow for a given payment link.
  void TriggerEwalletPushPayment(const GURL& payment_link_url,
                                 const GURL& page_url);

 private:
  // Checks if a payment link URL is on the allowlist and triggers the eWallet
  // prompt if allowed.
  void CheckAllowlistAndTriggerEwalletPrompt(const GURL& payment_link_url,
                                             const GURL& page_url);

  optimization_guide::OptimizationGuideDecision GetAllowlistCheckResult(
      const GURL& url) const;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PAYMENT_LINK_HANDLER_IMPL_H_
