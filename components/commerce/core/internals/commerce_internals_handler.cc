// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/internals/commerce_internals_handler.h"

#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/shopping_service.h"
#include "components/prefs/pref_service.h"

namespace commerce {

CommerceInternalsHandler::CommerceInternalsHandler(
    mojo::PendingRemote<mojom::CommerceInternalsPage> page,
    mojo::PendingReceiver<mojom::CommerceInternalsHandler> receiver,
    ShoppingService* shopping_service)
    : page_(std::move(page)),
      receiver_(this, std::move(receiver)),
      shopping_service_(shopping_service) {
  page_->OnShoppingListEligibilityChanged(
      shopping_service_ ? shopping_service_->IsShoppingListEligible() : false);
}

CommerceInternalsHandler::~CommerceInternalsHandler() = default;

void CommerceInternalsHandler::GetIsShoppingListEligible(
    GetIsShoppingListEligibleCallback callback) {
  std::move(callback).Run(
      shopping_service_ ? shopping_service_->IsShoppingListEligible() : false);
}

void CommerceInternalsHandler::GetShoppingListEligibleDetails(
    GetShoppingListEligibleDetailsCallback callback) {
  mojom::ShoppingListEligibleDetailPtr detail =
      mojom::ShoppingListEligibleDetail::New();

  if (!shopping_service_) {
    std::move(callback).Run(std::move(detail));
    return;
  }

  detail->is_region_locked_feature_enabled = mojom::EligibleEntry::New(
      IsRegionLockedFeatureEnabled(kShoppingList, kShoppingListRegionLaunched,
                                   shopping_service_->country_on_startup_,
                                   shopping_service_->locale_on_startup_),
      /*expected_value=*/true);
  detail->is_shopping_list_allowed_for_enterprise = mojom::EligibleEntry::New(
      shopping_service_->pref_service_ &&
          IsShoppingListAllowedForEnterprise(shopping_service_->pref_service_),
      /*expected_value=*/true);

  auto* account_checker = shopping_service_->account_checker_.get();
  if (!account_checker) {
    detail->is_account_checker_valid =
        mojom::EligibleEntry::New(false, /*expected_value=*/true);
    std::move(callback).Run(std::move(detail));
    return;
  }
  detail->is_account_checker_valid =
      mojom::EligibleEntry::New(true, /*expected_value=*/true);
  detail->is_signed_in =
      mojom::EligibleEntry::New(account_checker->IsOptedIntoSync(),
                                /*expected_value=*/true);
  detail->is_syncing_bookmarks = mojom::EligibleEntry::New(
      account_checker->IsSyncingBookmarks(), /*expected_value=*/true);
  detail->is_anonymized_url_data_collection_enabled = mojom::EligibleEntry::New(
      account_checker->IsAnonymizedUrlDataCollectionEnabled(),
      /*expected_value=*/true);
  detail->is_web_and_app_activity_enabled = mojom::EligibleEntry::New(
      account_checker->IsWebAndAppActivityEnabled(), /*expected_value=*/true);
  detail->is_subject_to_parental_controls = mojom::EligibleEntry::New(
      account_checker->IsSubjectToParentalControls(), /*expected_value=*/false);

  std::move(callback).Run(std::move(detail));
}

void CommerceInternalsHandler::ResetPriceTrackingEmailPref() {
  if (!shopping_service_) {
    return;
  }
  shopping_service_->pref_service_->ClearPref(kPriceEmailNotificationsEnabled);
}

}  // namespace commerce
