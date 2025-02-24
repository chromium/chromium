// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/passes/passes_data_manager.h"

#include "components/autofill/core/browser/data_model/passes/loyalty_card.h"
#include "components/webdata/common/web_data_results.h"

namespace autofill {

PassesDataManager::PassesDataManager(
    scoped_refptr<AutofillWebDataService> webdata_service)
    : webdata_service_(std::move(webdata_service)) {}

PassesDataManager::~PassesDataManager() = default;

void PassesDataManager::GetLoyaltyCards(LoadCallback cb) {
  webdata_service_->GetLoyaltyCards(base::BindOnce(
      [](LoadCallback cb, WebDataServiceBase::Handle handle,
         std::unique_ptr<WDTypedResult> result) {
        std::vector<LoyaltyCard> loyalty_cards;
        if (result) {
          CHECK_EQ(result->GetType(), AUTOFILL_LOYALTY_CARD_RESULT);
          loyalty_cards =
              static_cast<WDResult<std::vector<LoyaltyCard>>*>(result.get())
                  ->GetValue();
        }
        std::move(cb).Run(std::move(loyalty_cards));
      },
      std::move(cb)));
}

}  // namespace autofill
