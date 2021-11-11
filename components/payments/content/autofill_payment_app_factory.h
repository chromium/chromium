// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_AUTOFILL_PAYMENT_APP_FACTORY_H_
#define COMPONENTS_PAYMENTS_CONTENT_AUTOFILL_PAYMENT_APP_FACTORY_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_app_factory.h"

namespace autofill {
class CreditCard;
}  // namespace autofill

namespace payments {

class PaymentApp;

// Creates instances of Autofill payment apps, one per credit card.
class AutofillPaymentAppFactory : public PaymentAppFactory {
 public:
  // Used for creating an AutofillPaymentApp for a card that user adds in
  // Chrome's own Basic Card user interface.
  static std::unique_ptr<PaymentApp> ConvertCardToPaymentAppIfSupportedNetwork(
      const autofill::CreditCard& card,
      base::WeakPtr<Delegate> delegate);

  AutofillPaymentAppFactory();

  AutofillPaymentAppFactory(const AutofillPaymentAppFactory&) = delete;
  AutofillPaymentAppFactory& operator=(const AutofillPaymentAppFactory&) =
      delete;

  ~AutofillPaymentAppFactory() override;

  // PaymentAppFactory:
  void Create(base::WeakPtr<Delegate> delegate) override;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_AUTOFILL_PAYMENT_APP_FACTORY_H_
