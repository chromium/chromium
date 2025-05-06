// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_VALUABLES_VALUABLES_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_VALUABLES_VALUABLES_DATA_MANAGER_H_

#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/ui/autofill_image_fetcher_base.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/base/data_type.h"
#include "components/webdata/common/web_data_service_base.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace autofill {

// Loads non-payments data types coming from the Google Wallet like loyalty
// cards.
//
// A shared instance of this service is created for regular and off-the-record
// profiles. Future modifications to this service must make sure that no data is
// persisted for the off-the-record profile.
class ValuablesDataManager : public KeyedService,
                             public AutofillWebDataServiceObserverOnUISequence {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Triggered after all pending read operations have finished.
    virtual void OnValuablesDataChanged() = 0;
  };

  ValuablesDataManager(scoped_refptr<AutofillWebDataService> webdata_service,
                       AutofillImageFetcherBase* image_fetcher);
  ValuablesDataManager(const ValuablesDataManager&) = delete;
  ValuablesDataManager& operator=(const ValuablesDataManager&) = delete;
  ~ValuablesDataManager() override;

  void AddObserver(Observer* obs) { observers_.AddObserver(obs); }
  void RemoveObserver(Observer* obs) { observers_.RemoveObserver(obs); }

  // Returns the cached loyalty cards from the database.
  //
  // The cache is populated asynchronously after the construction of this
  // `ValuablesDataManager`. Returns an empty span until the population is
  // finished.
  //
  // The returned span may be invalidated asynchronously.
  base::span<const LoyaltyCard> GetLoyaltyCards() const;

  // Returns if there are any pending queries to the web database.
  bool HasPendingQueries() const;

  // Returns the cached image for the `image_url` if it was synced locally to
  // the client. The image is extracted from the local cache in
  // `AutofillImageFetcher`. If the card art image is not present in the cache,
  // this function will return a nullptr.
  const gfx::Image* GetCachedValuableImageForUrl(const GURL& image_url) const;

  // AutofillWebDataServiceObserverOnUISequence:
  void OnAutofillChangedBySync(syncer::DataType data_type) override;

 protected:
  // The image fetcher to fetch customized images for Autofill data.
  raw_ptr<AutofillImageFetcherBase> image_fetcher_ = nullptr;

 private:
  friend class ValuablesDataManagerTestApi;

  // Handler method called when `pending_query_` finishes.
  void OnDataRetrieved(WebDataServiceBase::Handle handle,
                       std::unique_ptr<WDTypedResult> result);

  // Starts a query to retrieve all loyalty cards.
  void LoadLoyaltyCards();

  // Handler method called with newly received loyalty cards.
  void OnLoyaltyCardsLoaded(const std::vector<LoyaltyCard>& loyalty_cards);

  // Fetches missing loyalty card icons.
  void ProcessLoyaltyCardIconUrlChanges();

  // Notify all observers that a change has occurred.
  void NotifyObservers();

  const scoped_refptr<AutofillWebDataService> webdata_service_;

  base::ScopedObservation<AutofillWebDataService,
                          AutofillWebDataServiceObserverOnUISequence>
      webdata_service_observer_{this};

  // The ongoing `LoadLoyaltyCards()` query.
  WebDataServiceBase::Handle pending_query_{};

  base::ObserverList<Observer> observers_;

  // The result of the last successful `LoadLoyaltyCards()` query.
  std::vector<LoyaltyCard> loyalty_cards_;

  base::WeakPtrFactory<ValuablesDataManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_VALUABLES_VALUABLES_DATA_MANAGER_H_
