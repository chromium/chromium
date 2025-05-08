// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"

#include <vector>

#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/ui/autofill_image_fetcher_base.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/sync/base/features.h"
#include "components/webdata/common/web_data_results.h"
#include "url/gurl.h"

namespace autofill {

ValuablesDataManager::ValuablesDataManager(
    scoped_refptr<AutofillWebDataService> webdata_service,
    AutofillImageFetcherBase* image_fetcher)
    : image_fetcher_(image_fetcher),
      webdata_service_(std::move(webdata_service)) {
  if (!webdata_service_) {
    // In some tests, there are no dbs.
    return;
  }
  webdata_service_observer_.Observe(webdata_service_.get());
  if (base::FeatureList::IsEnabled(syncer::kSyncAutofillLoyaltyCard)) {
    LoadLoyaltyCards();
  }
}

ValuablesDataManager::~ValuablesDataManager() = default;

base::span<const LoyaltyCard> ValuablesDataManager::GetLoyaltyCards() const {
  return loyalty_cards_;
}

const gfx::Image* ValuablesDataManager::GetCachedValuableImageForUrl(
    const GURL& image_url) const {
  if (!image_url.is_valid()) {
    return nullptr;
  }
  if (!image_fetcher_) {
    return nullptr;
  }
  return image_fetcher_->GetCachedImageForUrl(
      image_url, AutofillImageFetcherBase::ImageType::kValuableImage);
}

void ValuablesDataManager::OnDataRetrieved(
    WebDataServiceBase::Handle handle,
    std::unique_ptr<WDTypedResult> result) {
  CHECK_EQ(handle, pending_query_);
  pending_query_ = {};
  if (result) {
    CHECK_EQ(result->GetType(), AUTOFILL_LOYALTY_CARD_RESULT);
    OnLoyaltyCardsLoaded(
        static_cast<WDResult<std::vector<LoyaltyCard>>*>(result.get())
            ->GetValue());
  }
}

void ValuablesDataManager::LoadLoyaltyCards() {
  if (!webdata_service_) {
    return;
  }
  if (pending_query_) {
    webdata_service_->CancelRequest(pending_query_);
  }
  pending_query_ = webdata_service_->GetLoyaltyCards(base::BindOnce(
      &ValuablesDataManager::OnDataRetrieved, weak_ptr_factory_.GetWeakPtr()));
}

void ValuablesDataManager::OnLoyaltyCardsLoaded(
    const std::vector<LoyaltyCard>& loyalty_cards) {
  loyalty_cards_ = loyalty_cards;
  // Loyalty cards are coming from sync. A non-empty list of loyalty cards
  // implies that the user is syncing payment methods, which is a prerequisite
  // for caching loyalty card icons.
  ProcessLoyaltyCardIconUrlChanges();
  NotifyObservers();
}

void ValuablesDataManager::ProcessLoyaltyCardIconUrlChanges() {
  if (!image_fetcher_) {
    return;
  }
  std::vector<GURL> updated_urls;
  for (const auto& loyalty_card : loyalty_cards_) {
    if (!loyalty_card.program_logo().is_valid()) {
      continue;
    }
    updated_urls.emplace_back(loyalty_card.program_logo());
  }
  image_fetcher_->FetchValuableImagesForURLs(updated_urls);
}

bool ValuablesDataManager::HasPendingQueries() const {
  return pending_query_ != 0;
}

void ValuablesDataManager::OnAutofillChangedBySync(syncer::DataType data_type) {
  if (data_type == syncer::DataType::AUTOFILL_VALUABLE) {
    LoadLoyaltyCards();
  }
}

void ValuablesDataManager::NotifyObservers() {
  for (Observer& observer : observers_) {
    observer.OnValuablesDataChanged();
  }
}

}  // namespace autofill
