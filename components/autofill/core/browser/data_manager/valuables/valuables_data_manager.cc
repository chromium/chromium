// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"

#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/webdata/common/web_data_results.h"

namespace autofill {

ValuablesDataManager::ValuablesDataManager(
    scoped_refptr<AutofillWebDataService> webdata_service)
    : webdata_service_(std::move(webdata_service)) {
  CHECK(webdata_service_);
  webdata_service_observer_.Observe(webdata_service_.get());
  LoadLoyaltyCards();
}

ValuablesDataManager::~ValuablesDataManager() = default;

base::span<const LoyaltyCard> ValuablesDataManager::GetLoyaltyCards() const {
  return loyalty_cards_;
}

void ValuablesDataManager::LoadLoyaltyCards() {
  if (pending_query_) {
    webdata_service_->CancelRequest(pending_query_);
  }
  pending_query_ = webdata_service_->GetLoyaltyCards(base::BindOnce(
      [](base::WeakPtr<ValuablesDataManager> self,
         WebDataServiceBase::Handle handle,
         std::unique_ptr<WDTypedResult> result) {
        CHECK_EQ(handle, self->pending_query_);
        self->pending_query_ = {};
        if (result) {
          CHECK_EQ(result->GetType(), AUTOFILL_LOYALTY_CARD_RESULT);
          self->loyalty_cards_ =
              static_cast<WDResult<std::vector<LoyaltyCard>>*>(result.get())
                  ->GetValue();
        }
      },
      weak_ptr_factory_.GetWeakPtr()));
}

void ValuablesDataManager::OnAutofillChangedBySync(syncer::DataType data_type) {
  if (data_type == syncer::DataType::AUTOFILL_VALUABLE) {
    LoadLoyaltyCards();
  }
}

}  // namespace autofill
