// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/payment_link_handler_impl.h"

#include "components/facilitated_payments/core/util/payment_link_validator.h"

namespace payments::facilitated {

PaymentLinkHandlerImpl::PaymentLinkHandlerImpl() = default;
PaymentLinkHandlerImpl::~PaymentLinkHandlerImpl() = default;

void PaymentLinkHandlerImpl::TriggerEwalletPushPayment(
    const GURL& payment_link_url,
    const GURL& page_url) {
  PaymentLinkValidator payment_link_validator;
  if (!payment_link_validator.IsValid(payment_link_url.spec())) {
    return;
  }

  // TODO(b/40280186): The page needs to have following properties:
  // Cryptographic scheme (e.g., HTTPS. One exception being localhost).
  // Valid SSL (no expired cert).
  // No mixed content (i.e., all sub-resources must come from HTTPS).
  // Only on top-level context (not inside of an iframe).
  // No Permissions-Policy restrictions on the "payments" feature.

  PaymentLinkHandlerImpl::CheckAllowlistAndTriggerEwalletPrompt(
      payment_link_url, page_url);
}

void PaymentLinkHandlerImpl::CheckAllowlistAndTriggerEwalletPrompt(
    const GURL& payment_link_url,
    const GURL& page_url) {
  switch (GetAllowlistCheckResult(page_url)) {
    case optimization_guide::OptimizationGuideDecision::kTrue: {
      // TODO(b/40280186): Trigger the eWallet FOP selector if eWallet accounts
      // are available and support the payment link.
      break;
    }
    case optimization_guide::OptimizationGuideDecision::kUnknown: {
      // TODO(b/40280186): Delay and retry the check if not reaching maximum
      // retry limitation.
      break;
    }
    case optimization_guide::OptimizationGuideDecision::kFalse: {
      // The eWallet FOP selector won't be shown if the page is not allowlisted.
      break;
    }
  }
}

optimization_guide::OptimizationGuideDecision
PaymentLinkHandlerImpl::GetAllowlistCheckResult(const GURL& url) const {
  return optimization_guide::OptimizationGuideDecision::kUnknown;
}

}  // namespace payments::facilitated
