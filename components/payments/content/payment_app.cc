// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_app.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/functional/callback.h"

namespace payments {
namespace {

// Returns the sorting group of a payment app. This is used to order payment
// apps in the order of:
// 1. Built-in available 1st-party payment handlers.
// 2. Installed or just-in-time installable payment handlers.
int GetSortingGroup(const PaymentApp& app) {
  switch (app.type()) {
    case PaymentApp::Type::INTERNAL:
      return 1;
    case PaymentApp::Type::SERVICE_WORKER_APP:
    case PaymentApp::Type::NATIVE_MOBILE_APP:
      return 2;
    case PaymentApp::Type::UNDEFINED:
      NOTREACHED_IN_MIGRATION();
      return 99;
  }
}
}  // namespace

PaymentApp::PaymentApp(int icon_resource_id, Type type)
    : icon_resource_id_(icon_resource_id), type_(type) {}

PaymentApp::~PaymentApp() = default;

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
  // or not.
  if (CanPreselect() != other.CanPreselect())
    return CanPreselect();
  return false;
}

}  // namespace payments
