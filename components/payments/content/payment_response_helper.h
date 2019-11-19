// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_RESPONSE_HELPER_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_RESPONSE_HELPER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/address_normalizer.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/payments/core/payment_app.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace payments {

class PaymentRequestDelegate;
class PaymentRequestSpec;

// A helper class to facilitate the creation of the PaymentResponse.
class PaymentResponseHelper
    : public PaymentApp::Delegate,
      public base::SupportsWeakPtr<PaymentResponseHelper> {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void OnPaymentResponseReady(
        mojom::PaymentResponsePtr payment_response) = 0;

    virtual void OnPaymentResponseError(const std::string& error_message) = 0;
  };

  // The spec, selected_app and delegate cannot be null.
  PaymentResponseHelper(const std::string& app_locale,
                        PaymentRequestSpec* spec,
                        PaymentApp* selected_app,
                        PaymentRequestDelegate* payment_request_delegate,
                        autofill::AutofillProfile* selected_shipping_profile,
                        autofill::AutofillProfile* selected_contact_profile,
                        Delegate* delegate);
  ~PaymentResponseHelper() override;

  // PaymentApp::Delegate
  void OnInstrumentDetailsReady(const std::string& method_name,
                                const std::string& stringified_details,
                                const PayerData& payer_data) override;
  void OnInstrumentDetailsError(const std::string& error_message) override;

  mojom::PayerDetailPtr GeneratePayerDetail(
      const autofill::AutofillProfile* selected_contact_profile) const;

 private:
  // Generates the Payment Response and sends it to the delegate.
  void GeneratePaymentResponse();

  // To be used as AddressNormalizer::NormalizationCallback.
  void OnAddressNormalized(bool success,
                           const autofill::AutofillProfile& normalized_profile);

  const std::string& app_locale_;
  bool is_waiting_for_shipping_address_normalization_;
  bool is_waiting_for_instrument_details_;

  // Not owned, cannot be null.
  PaymentRequestSpec* spec_;
  Delegate* delegate_;
  PaymentApp* selected_app_;
  PaymentRequestDelegate* payment_request_delegate_;

  // Not owned, can be null (dependent on the spec).
  autofill::AutofillProfile* selected_contact_profile_;

  // A normalized copy of the shipping address, which will be included in the
  // PaymentResponse.
  autofill::AutofillProfile shipping_address_;

  // Instrument Details.
  std::string method_name_;
  std::string stringified_details_;

  // Details from payment handler response that will be included in the
  // PaymentResponse when shipping/contact handling is delegated to the payment
  // handler.
  PayerData payer_data_from_app_;

  base::WeakPtrFactory<PaymentResponseHelper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PaymentResponseHelper);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_RESPONSE_HELPER_H_
