// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_offer_manager.h"

#include <map>

#include "base/bind.h"
#include "base/ranges/algorithm.h"
#include "base/ranges/ranges.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace autofill {

namespace {
// Ensure the offer is not expired and is valid for the current page.
bool IsOfferEligible(const AutofillOfferData& offer,
                     const GURL& last_committed_url_origin) {
  bool is_eligible = (offer.expiry > AutofillClock::Now());
  is_eligible &=
      base::ranges::count(offer.merchant_domain, last_committed_url_origin);
  return is_eligible;
}
}  // namespace

AutofillOfferManager::AutofillOfferManager(PersonalDataManager* personal_data)
    : personal_data_(personal_data) {
  personal_data_->AddObserver(this);
  UpdateEligibleMerchantDomains();
}

AutofillOfferManager::~AutofillOfferManager() {
  personal_data_->RemoveObserver(this);
}

void AutofillOfferManager::OnPersonalDataChanged() {
  UpdateEligibleMerchantDomains();
}

void AutofillOfferManager::UpdateSuggestionsWithOffers(
    const GURL& last_committed_url,
    std::vector<Suggestion>& suggestions) {
  GURL last_committed_url_origin = last_committed_url.GetOrigin();
  if (eligible_merchant_domains_.count(last_committed_url_origin) == 0) {
    return;
  }

  AutofillOfferManager::OffersMap eligible_offers_map =
      CreateOffersMap(last_committed_url_origin);

  // Update |offer_label| for each suggestion.
  for (auto& suggestion : suggestions) {
    std::string id = suggestion.backend_id;
    if (eligible_offers_map.count(id)) {
      suggestion.offer_label =
          l10n_util::GetStringUTF16(IDS_AUTOFILL_OFFERS_CASHBACK);
    }
  }
}

void AutofillOfferManager::UpdateEligibleMerchantDomains() {
  eligible_merchant_domains_.clear();
  std::vector<AutofillOfferData*> offers =
      personal_data_->GetCreditCardOffers();

  for (auto* offer : offers) {
    for (auto& domain : offer->merchant_domain) {
      eligible_merchant_domains_.emplace(domain);
    }
  }
}

AutofillOfferManager::OffersMap AutofillOfferManager::CreateOffersMap(
    const GURL& last_committed_url_origin) const {
  AutofillOfferManager::OffersMap offers_map;

  std::vector<AutofillOfferData*> offers =
      personal_data_->GetCreditCardOffers();
  std::vector<CreditCard*> cards = personal_data_->GetCreditCards();

  for (auto* offer : offers) {
    // Ensure the offer is valid.
    if (!IsOfferEligible(*offer, last_committed_url_origin)) {
      continue;
    }

    // Find card with corresponding instrument ID and add its guid to the map.
    for (const auto* card : cards) {
      // If card has an offer, add the backend ID to the map. There is currently
      // a one-to-one mapping between cards and offer data, however, this may
      // change in the future.
      if (std::count(offer->eligible_instrument_id.begin(),
                     offer->eligible_instrument_id.end(),
                     card->instrument_id())) {
        offers_map[card->guid()] = offer;
      }
    }
  }

  return offers_map;
}

}  // namespace autofill
