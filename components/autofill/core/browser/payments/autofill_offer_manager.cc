// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_offer_manager.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/ranges/ranges.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
AutofillOfferManager::AutofillOfferManager(PersonalDataManager* personal_data)
    : personal_data_(personal_data) {
  payments_data_manager_observation.Observe(
      &personal_data_->payments_data_manager());
  UpdateEligibleMerchantDomains();
}

AutofillOfferManager::~AutofillOfferManager() = default;

void AutofillOfferManager::OnPaymentsDataChanged() {
  UpdateEligibleMerchantDomains();
}

void AutofillOfferManager::OnDidNavigateFrame(AutofillClient& client) {
  notification_handler_.UpdateOfferNotificationVisibility(client);
}

AutofillOfferManager::CardLinkedOffersMap
AutofillOfferManager::GetCardLinkedOffersMap(
    const GURL& last_committed_primary_main_frame_url) const {
  GURL last_committed_primary_main_frame_origin =
      last_committed_primary_main_frame_url.DeprecatedGetOriginAsURL();

  if (!base::Contains(eligible_merchant_domains_,
                      last_committed_primary_main_frame_origin)) {
    return {};
  }

  const std::vector<AutofillOfferData*> offers =
      personal_data_->payments_data_manager().GetAutofillOffers();
  const std::vector<CreditCard*> cards =
      personal_data_->payments_data_manager().GetCreditCards();
  AutofillOfferManager::CardLinkedOffersMap card_linked_offers_map;

  for (AutofillOfferData* offer : offers) {
    // Ensure the offer is valid.
    if (!offer->IsActiveAndEligibleForOrigin(
            last_committed_primary_main_frame_origin)) {
      continue;
    }

    // Ensure the offer is a card-linked offer.
    if (!offer->IsCardLinkedOffer()) {
      continue;
    }

    for (const CreditCard* card : cards) {
      // If card has an offer, add the card's guid id to the map. There is
      // currently a one-to-one mapping between cards and offer data.
      if (base::Contains(offer->GetEligibleInstrumentIds(),
                         card->instrument_id())) {
        card_linked_offers_map[card->guid()] = offer;
      }
    }
  }

  return card_linked_offers_map;
}

bool AutofillOfferManager::IsUrlEligible(
    const GURL& last_committed_primary_main_frame_url) {
  return base::Contains(
      eligible_merchant_domains_,
      last_committed_primary_main_frame_url.DeprecatedGetOriginAsURL());
}

AutofillOfferData* AutofillOfferManager::GetOfferForUrl(
    const GURL& last_committed_primary_main_frame_url) {
  for (AutofillOfferData* offer :
       personal_data_->payments_data_manager().GetAutofillOffers()) {
    if (offer->IsActiveAndEligibleForOrigin(
            last_committed_primary_main_frame_url.DeprecatedGetOriginAsURL())) {
      return offer;
    }
  }

  return nullptr;
}

void AutofillOfferManager::UpdateEligibleMerchantDomains() {
  eligible_merchant_domains_.clear();
  std::vector<AutofillOfferData*> offers =
      personal_data_->payments_data_manager().GetAutofillOffers();

  for (auto* offer : offers) {
    eligible_merchant_domains_.insert(offer->GetMerchantOrigins().begin(),
                                      offer->GetMerchantOrigins().end());
  }
}

}  // namespace autofill
