// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_app.h"

#include <algorithm>

#include "base/callback.h"
#include "base/containers/contains.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/payments/content/autofill_payment_app.h"
#include "components/payments/core/features.h"
#include "components/payments/core/payments_experimental_features.h"

namespace payments {
namespace {

// Returns the sorting group of a payment app. This is used to order payment
// apps in the order of:
// 1. Built-in 1st-party payment handlers.
// 2. Installed 3rd-party payment handlers
// 3. Complete autofill instruments
// 4. Just-in-time installable payment handlers that is not yet installed.
// 5. Incomplete autofill instruments
int GetSortingGroup(const PaymentApp& app) {
  switch (app.type()) {
    case PaymentApp::Type::INTERNAL:
      return 1;
    case PaymentApp::Type::SERVICE_WORKER_APP:
    case PaymentApp::Type::NATIVE_MOBILE_APP:
      // If the experimental feature is enabled, sort 3rd-party payment handlers
      // that needs installation below autofill instruments.
      if (app.NeedsInstallation() &&
          PaymentsExperimentalFeatures::IsEnabled(
              features::kDownRankJustInTimePaymentApp)) {
        return 4;
      }
      return 2;
    case PaymentApp::Type::AUTOFILL:
      if (app.IsCompleteForPayment()) {
        return 3;
      }
      return 5;
    case PaymentApp::Type::UNDEFINED:
      NOTREACHED();
      return 99;
  }
}
}  // namespace

PaymentApp::PaymentApp(int icon_resource_id, Type type)
    : icon_resource_id_(icon_resource_id), type_(type) {}

PaymentApp::~PaymentApp() {}

const SkBitmap* PaymentApp::icon_bitmap() const {
  return nullptr;
}

std::string PaymentApp::GetApplicationIdentifierToHide() const {
  return std::string();
}

std::set<std::string> PaymentApp::GetApplicationIdentifiersThatHideThisApp()
    const {
  return std::set<std::string>();
}

void PaymentApp::IsValidForPaymentMethodIdentifier(
    const std::string& payment_method_identifier,
    bool* is_valid) const {
  *is_valid = base::Contains(app_method_names_, payment_method_identifier);
}

const std::set<std::string>& PaymentApp::GetAppMethodNames() const {
  return app_method_names_;
}

ukm::SourceId PaymentApp::UkmSourceId() {
  return ukm::kInvalidSourceId;
}

bool PaymentApp::IsWaitingForPaymentDetailsUpdate() const {
  return false;
}

void PaymentApp::AbortPaymentApp(
    base::OnceCallback<void(bool)> abort_callback) {
  std::move(abort_callback).Run(/*aborted=*/false);
}

bool PaymentApp::IsPreferred() const {
  return false;
}

mojom::PaymentResponsePtr PaymentApp::SetAppSpecificResponseFields(
    mojom::PaymentResponsePtr response) const {
  return response;
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
  int sorting_group = GetSortingGroup(*this);
  int other_sorting_group = GetSortingGroup(other);

  // First sort payment apps by their sorting group.
  if (sorting_group != other_sorting_group) {
    return sorting_group < other_sorting_group;
  }

  // Within a group, compare by completeness.
  // Non-autofill apps have max completeness score. Autofill cards are sorted
  // based on completeness. (Each autofill card is considered an app.)
  int completeness = GetCompletenessScore() - other.GetCompletenessScore();
  if (completeness != 0)
    return completeness > 0;

  // Sort autofill cards using their ranking scores as tie breaker.
  if (type_ == Type::AUTOFILL) {
    DCHECK_EQ(other.type(), Type::AUTOFILL);
    return static_cast<const AutofillPaymentApp*>(this)
        ->credit_card()
        ->HasGreaterRankingThan(
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
