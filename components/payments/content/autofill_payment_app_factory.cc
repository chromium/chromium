// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/autofill_payment_app_factory.h"

#include <vector>

#include "base/feature_list.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/content/autofill_payment_app.h"
#include "components/payments/content/content_payment_request_delegate.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/core/features.h"

namespace payments {

// static
std::unique_ptr<PaymentApp>
AutofillPaymentAppFactory::ConvertCardToPaymentAppIfSupportedNetwork(
    const autofill::CreditCard& card,
    base::WeakPtr<Delegate> delegate) {
  DCHECK(delegate);
  DCHECK(delegate->GetSpec());

  std::string basic_card_network =
      autofill::data_util::GetPaymentRequestData(card.network())
          .basic_card_issuer_network;
  if (!delegate->GetSpec()->supported_card_networks_set().count(
          basic_card_network)) {
    return nullptr;
  }

  auto app = std::make_unique<AutofillPaymentApp>(
      basic_card_network, card, delegate->GetBillingProfiles(),
      delegate->GetPaymentRequestDelegate()->GetApplicationLocale(),
      delegate->GetPaymentRequestDelegate());

  app->set_is_requested_autofill_data_available(
      delegate->IsRequestedAutofillDataAvailable());

  return app;
}

AutofillPaymentAppFactory::AutofillPaymentAppFactory()
    : PaymentAppFactory(PaymentApp::Type::AUTOFILL) {}

AutofillPaymentAppFactory::~AutofillPaymentAppFactory() = default;

void AutofillPaymentAppFactory::Create(base::WeakPtr<Delegate> delegate) {
  DCHECK(delegate);

  if (!delegate->GetSpec())
    return;

  // No need to create autofill payment apps if native app creation is skipped
  // because autofill payment apps are created completely by the Java factory.
  if (delegate->SkipCreatingNativePaymentApps()) {
    delegate->OnDoneCreatingPaymentApps();
    return;
  }

  const std::vector<autofill::CreditCard*>& cards =
      delegate->GetPaymentRequestDelegate()
          ->GetPersonalDataManager()
          ->GetCreditCardsToSuggest(
              /*include_server_cards=*/base::FeatureList::IsEnabled(
                  features::kReturnGooglePayInBasicCard));

  for (autofill::CreditCard* card : cards) {
    auto app = ConvertCardToPaymentAppIfSupportedNetwork(*card, delegate);
    if (app)
      delegate->OnPaymentAppCreated(std::move(app));
  }

  delegate->OnDoneCreatingPaymentApps();
}

}  // namespace payments
