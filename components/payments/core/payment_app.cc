// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_app.h"

#include <algorithm>

#include "components/autofill/core/common/autofill_clock.h"
#include "components/payments/core/autofill_payment_app.h"

namespace payments {

PaymentApp::PaymentApp(int icon_resource_id, Type type)
    : icon_resource_id_(icon_resource_id), type_(type) {}

PaymentApp::~PaymentApp() {}

gfx::ImageSkia PaymentApp::icon_image_skia() const {
  return gfx::ImageSkia();
}

// static
void PaymentApp::SortApps(std::vector<std::unique_ptr<PaymentApp>>* apps) {
  DCHECK(apps);
  std::sort(apps->begin(), apps->end(),
            [](const std::unique_ptr<PaymentApp>& lhs,
               const std::unique_ptr<PaymentApp>& rhs) { return *lhs < *rhs; });
}

// static
void PaymentApp::SortApps(std::vector<PaymentApp*>* apps) {
  DCHECK(apps);
  std::sort(
      apps->begin(), apps->end(),
      [](const PaymentApp* lhs, const PaymentApp* rhs) { return *lhs < *rhs; });
}

bool PaymentApp::operator<(const PaymentApp& other) const {
  // Non-autofill apps before autofill.
  if (type_ == Type::AUTOFILL && other.type() != Type::AUTOFILL)
    return false;
  if (type_ != Type::AUTOFILL && other.type() == Type::AUTOFILL)
    return true;

  // Non-autofill apps have max completeness score. Autofill cards are sorted
  // based on completeness. (Each autofill card is considered an app.)
  int completeness = GetCompletenessScore() - other.GetCompletenessScore();
  if (completeness != 0)
    return completeness > 0;

  // Among equally complete cards, those with matching type come before unknown
  // type cards.
  if (IsExactlyMatchingMerchantRequest() !=
      other.IsExactlyMatchingMerchantRequest()) {
    return IsExactlyMatchingMerchantRequest();
  }

  // Sort autofill cards using their frecency scores as tie breaker.
  if (type_ == Type::AUTOFILL) {
    DCHECK_EQ(other.type(), Type::AUTOFILL);
    return static_cast<const AutofillPaymentApp*>(this)
        ->credit_card()
        ->HasGreaterFrecencyThan(
            static_cast<const AutofillPaymentApp*>(&other)->credit_card(),
            autofill::AutofillClock::Now());
  }

  // SW based payment apps are sorted based on whether they will handle shipping
  // delegation or not (i.e. shipping address is requested and the app supports
  // the delegation.).
  if (HandlesShippingAddress() != other.HandlesShippingAddress())
    return HandlesShippingAddress();

  // SW based payment apps are sorted based on the number of the contact field
  // delegations that they will handle (i.e. number of contact fields which are
  // requested and the apps support their delegations.)
  int supported_contact_delegations_num = 0;
  if (HandlesPayerEmail())
    supported_contact_delegations_num++;
  if (HandlesPayerName())
    supported_contact_delegations_num++;
  if (HandlesPayerPhone())
    supported_contact_delegations_num++;

  int other_supported_contact_delegations_num = 0;
  if (other.HandlesPayerEmail())
    other_supported_contact_delegations_num++;
  if (other.HandlesPayerName())
    other_supported_contact_delegations_num++;
  if (other.HandlesPayerPhone())
    other_supported_contact_delegations_num++;

  int contact_delegations_diff = supported_contact_delegations_num -
                                 other_supported_contact_delegations_num;
  if (contact_delegations_diff != 0)
    return contact_delegations_diff > 0;

  // SW based payment apps are sorted based on whether they can be pre-selected
  // or not. Note that autofill cards are already sorted by CanPreselect() since
  // they are sorted by completeness and type matching.
  if (CanPreselect() != other.CanPreselect())
    return CanPreselect();
  return false;
}

}  // namespace payments
