// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/passes/passes_data_manager.h"

#include "components/autofill/core/browser/data_model/passes/loyalty_card.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/webdata/common/web_data_results.h"

namespace autofill {

PassesDataManager::PassesDataManager(
    scoped_refptr<AutofillWebDataService> webdata_service)
    : webdata_service_(std::move(webdata_service)) {
  CHECK(webdata_service_);
  webdata_service_observer_.Observe(webdata_service_.get());
  LoadLoyaltyCards();
}

PassesDataManager::~PassesDataManager() = default;

base::span<const LoyaltyCard> PassesDataManager::GetLoyaltyCards() const {
  return loyalty_cards_;
}

void PassesDataManager::LoadLoyaltyCards() {
  if (pending_query_) {
    webdata_service_->CancelRequest(pending_query_);
  }
  pending_query_ = webdata_service_->GetLoyaltyCards(base::BindOnce(
      [](base::WeakPtr<PassesDataManager> self,
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

void PassesDataManager::OnAutofillChangedBySync(syncer::DataType data_type) {
  if (data_type == syncer::DataType::AUTOFILL_LOYALTY_CARD) {
    LoadLoyaltyCards();
  }
}

}  // namespace autofill
