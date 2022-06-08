// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_offer_manager.h"

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/ranges/ranges.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AutofillOfferManager::AutofillOfferManager(
    PersonalDataManager* personal_data,
    CouponServiceDelegate* coupon_service_delegate)
    : personal_data_(personal_data),
      coupon_service_delegate_(coupon_service_delegate) {
  personal_data_->AddObserver(this);
  UpdateEligibleMerchantDomains();
}

AutofillOfferManager::~AutofillOfferManager() {
  personal_data_->RemoveObserver(this);
}

void AutofillOfferManager::OnPersonalDataChanged() {
  UpdateEligibleMerchantDomains();
}

void AutofillOfferManager::OnDidNavigateFrame(AutofillClient* client) {
  notification_handler_.UpdateOfferNotificationVisibility(client);
}

void AutofillOfferManager::UpdateSuggestionsWithOffers(
    const GURL& last_committed_url,
    std::vector<Suggestion>& suggestions) {
  GURL last_committed_url_origin =
      last_committed_url.DeprecatedGetOriginAsURL();
  if (eligible_merchant_domains_.count(last_committed_url_origin) == 0) {
    return;
  }

  AutofillOfferManager::OffersMap eligible_offers_map =
      CreateCardLinkedOffersMap(last_committed_url_origin);

  // Update |offer_label| for each suggestion.
  for (auto& suggestion : suggestions) {
    std::string id = suggestion.GetPayload<std::string>();
    if (eligible_offers_map.count(id)) {
      suggestion.offer_label =
          l10n_util::GetStringUTF16(IDS_AUTOFILL_OFFERS_CASHBACK);
    }
  }
  // Sort the suggestions such that suggestions with offers are shown at the
  // top.
  std::sort(suggestions.begin(), suggestions.end(),
            [](const Suggestion& a, const Suggestion& b) {
              if (!a.offer_label.empty() && b.offer_label.empty()) {
                return true;
              }
              return false;
            });
}

bool AutofillOfferManager::IsUrlEligible(const GURL& last_committed_url) {
  if (coupon_service_delegate_ &&
      coupon_service_delegate_->IsUrlEligible(last_committed_url)) {
    return true;
  }
  return base::Contains(eligible_merchant_domains_,
                        last_committed_url.DeprecatedGetOriginAsURL());
}

AutofillOfferData* AutofillOfferManager::GetOfferForUrl(
    const GURL& last_committed_url) {
  if (coupon_service_delegate_) {
    for (AutofillOfferData* offer :
         coupon_service_delegate_->GetFreeListingCouponsForUrl(
             last_committed_url)) {
      if (offer->IsActiveAndEligibleForOrigin(
              last_committed_url.DeprecatedGetOriginAsURL())) {
        return offer;
      }
    }
  }

  for (AutofillOfferData* offer : personal_data_->GetAutofillOffers()) {
    if (offer->IsActiveAndEligibleForOrigin(
            last_committed_url.DeprecatedGetOriginAsURL())) {
      return offer;
    }
  }
  return nullptr;
}

void AutofillOfferManager::UpdateEligibleMerchantDomains() {
  eligible_merchant_domains_.clear();
  std::vector<AutofillOfferData*> offers = personal_data_->GetAutofillOffers();

  for (auto* offer : offers) {
    eligible_merchant_domains_.insert(offer->GetMerchantOrigins().begin(),
                                      offer->GetMerchantOrigins().end());
  }
}

AutofillOfferManager::OffersMap AutofillOfferManager::CreateCardLinkedOffersMap(
    const GURL& last_committed_url_origin) const {
  AutofillOfferManager::OffersMap offers_map;

  std::vector<AutofillOfferData*> offers = personal_data_->GetAutofillOffers();
  std::vector<CreditCard*> cards = personal_data_->GetCreditCards();

  for (auto* offer : offers) {
    // Ensure the offer is valid.
    if (!offer->IsActiveAndEligibleForOrigin(last_committed_url_origin)) {
      continue;
    }
    // Ensure the offer is a card-linked offer.
    if (!offer->IsCardLinkedOffer()) {
      continue;
    }

    // Find card with corresponding instrument ID and add its guid to the map.
    for (const auto* card : cards) {
      // If card has an offer, add the backend ID to the map. There is currently
      // a one-to-one mapping between cards and offer data, however, this may
      // change in the future.
      if (std::count(offer->GetEligibleInstrumentIds().begin(),
                     offer->GetEligibleInstrumentIds().end(),
                     card->instrument_id())) {
        offers_map[card->guid()] = offer;
      }
    }
  }

  return offers_map;
}

}  // namespace autofill
