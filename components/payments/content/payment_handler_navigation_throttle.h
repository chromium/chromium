// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_HANDLER_NAVIGATION_THROTTLE_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_HANDLER_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace payments {
// The navigation throttle for the payment handler WebContents, used to
// prevent the WebContents from openning pages of certain categories, e.g. pdf.
class PaymentHandlerNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit PaymentHandlerNavigationThrottle(
      content::NavigationHandle* navigation_handle);
  ~PaymentHandlerNavigationThrottle() override;

  PaymentHandlerNavigationThrottle(const PaymentHandlerNavigationThrottle&) =
      delete;
  PaymentHandlerNavigationThrottle& operator=(
      const PaymentHandlerNavigationThrottle&) = delete;

  // Marks the given WebContents as a PaymentHandler WebContents. Ignores null
  // web_contents.
  static void MarkPaymentHandlerWebContents(content::WebContents* web_contents);

  static std::unique_ptr<PaymentHandlerNavigationThrottle>
  MaybeCreateThrottleFor(content::NavigationHandle* handle);

  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;
};
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_HANDLER_NAVIGATION_THROTTLE_H_
