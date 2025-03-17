// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_PASSES_PASSES_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_PASSES_PASSES_DATA_MANAGER_H_

#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/data_model/passes/loyalty_card.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/base/data_type.h"
#include "components/webdata/common/web_data_service_base.h"

namespace autofill {

// Loads non-payments data types coming from the Google Wallet like loyalty
// cards.
//
// A shared instance of this service is created for regular and off-the-record
// profiles. Future modifications to this service must make sure that no data is
// persisted for the off-the-record profile.
class PassesDataManager : public KeyedService,
                          public AutofillWebDataServiceObserverOnUISequence {
 public:
  explicit PassesDataManager(
      scoped_refptr<AutofillWebDataService> webdata_service);
  PassesDataManager(const PassesDataManager&) = delete;
  PassesDataManager& operator=(const PassesDataManager&) = delete;
  ~PassesDataManager() override;

  // Returns the cached loyalty cards from the database.
  //
  // The cache is populated asynchronously after the construction of this
  // `PassesDataManager`. Returns an empty span until the population is
  // finished.
  base::span<const LoyaltyCard> GetLoyaltyCards() const;

  // AutofillWebDataServiceObserverOnUISequence:
  void OnAutofillChangedBySync(syncer::DataType data_type) override;

 private:
  void LoadLoyaltyCards();

  const scoped_refptr<AutofillWebDataService> webdata_service_;

  base::ScopedObservation<AutofillWebDataService,
                          AutofillWebDataServiceObserverOnUISequence>
      webdata_service_observer_{this};

  // The ongoing `LoadLoyaltyCards()` query.
  WebDataServiceBase::Handle pending_query_{};

  // The result of the last successful `LoadLoyaltyCards()` query.
  std::vector<LoyaltyCard> loyalty_cards_;

  base::WeakPtrFactory<PassesDataManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_PASSES_PASSES_DATA_MANAGER_H_
