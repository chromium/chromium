// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_MANAGER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_MANAGER_H_

class GURL;

namespace payments::facilitated {

// A cross-platform interface that manages the eWallet push payment flow. It is
// owned by `FacilitatedPaymentsDriver`.
class EwalletManager {
 public:
  EwalletManager();
  EwalletManager(const EwalletManager&) = delete;
  EwalletManager& operator=(const EwalletManager&) = delete;
  virtual ~EwalletManager();

  // Initiates the eWallet push payment flow for a given payment link in a
  // certain page. The `payment_link_url` contains all the information to
  // initialize a payment. And the `page_url` is the url of a page where the
  // payment link is detected. More details on payment links can be found at
  // https://github.com/aneeshali/paymentlink/blob/main/docs/explainer.md.
  virtual void TriggerEwalletPushPayment(const GURL& payment_link_url,
                                         const GURL& page_url);
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_MANAGER_H_
