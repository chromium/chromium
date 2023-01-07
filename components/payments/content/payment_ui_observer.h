// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_UI_OBSERVER_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_UI_OBSERVER_H_

namespace payments {

// Interface for cross-platform tests to observe UI events.
class PaymentUIObserver {
 public:
  virtual void OnUIDisplayed() const = 0;

 protected:
  virtual ~PaymentUIObserver() = default;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_UI_OBSERVER_H_
