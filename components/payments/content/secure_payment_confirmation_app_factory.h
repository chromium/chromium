// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_APP_FACTORY_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_APP_FACTORY_H_

#include <map>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_app_factory.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace payments {

struct SecurePaymentConfirmationInstrument;

class SecurePaymentConfirmationAppFactory : public PaymentAppFactory,
                                            public WebDataServiceConsumer {
 public:
  SecurePaymentConfirmationAppFactory();
  ~SecurePaymentConfirmationAppFactory() override;

  SecurePaymentConfirmationAppFactory(
      const SecurePaymentConfirmationAppFactory& other) = delete;
  SecurePaymentConfirmationAppFactory& operator=(
      const SecurePaymentConfirmationAppFactory& other) = delete;

  // PaymentAppFactory:
  void Create(base::WeakPtr<Delegate> delegate) override;

 private:
  struct Request;

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override;

  void OnIsUserVerifyingPlatformAuthenticatorAvailable(
      base::WeakPtr<PaymentAppFactory::Delegate> delegate,
      mojom::SecurePaymentConfirmationRequestPtr request,
      std::unique_ptr<autofill::InternalAuthenticator> authenticator,
      bool is_available);

  void OnAppIconDecoded(
      std::unique_ptr<SecurePaymentConfirmationInstrument> instrument,
      std::unique_ptr<Request> request,
      const SkBitmap& decoded_image);

  std::map<WebDataServiceBase::Handle, std::unique_ptr<Request>> requests_;
  base::WeakPtrFactory<SecurePaymentConfirmationAppFactory> weak_ptr_factory_{
      this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_APP_FACTORY_H_
