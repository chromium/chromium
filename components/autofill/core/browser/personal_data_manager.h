// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_shared_storage_handler.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class PrefService;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace autofill {

class AutofillImageFetcherBase;
class PersonalDataManagerObserver;

// The PersonalDataManager (PDM) has two main responsibilities:
// - Caching the data stored in `AutofillTable` for synchronous retrieval.
// - Posting changes to `AutofillTable` via the `AutofillWebDataService`
//   and updating its state accordingly.
//   Some payment-related changes (e.g. adding a new server card) don't pass
//   through the PDM. Instead, they are upstreamed to payments directly, before
//   Sync downstreams them to Chrome, making them available in `AutofillTable`.
//
// These responsibilities are split between address and payments data and
// managed separately in the `AddressDataManager` (ADM) and
// `PaymentsDataManager` (PayDM), respectively. The comment here explains the
// general principles that apply to both. For data type specific information,
// see those classes.
// The ADM/PayDM are owned by the PDM.
// TODO(crbug.com/322170538): Currently, only the PDM can be observed. Split the
// PDM observer into an ADM and a PayDM observer.
//
// Since `AutofillTable` lives on a separate sequence, changes posted to the
// ADM/PayDM are asynchronous. They only become effective in the ADM/PayDM
// after/if the corresponding database operation successfully finished.
//
// Sync writes to `AutofillTable` directly, since sync bridges live on the same
// sequence. In this case, the ADM/PayDM is notified via
// `AutofillWebDataServiceObserverOnUISequence::OnAutofillChangedBySync()` and
// it reloads all its data from `AutofillTable`. This is done via an operation
// called `Refresh()`.
//
// ADM/PayDM getters such as `GetProfiles()` expose pointers to their internal
// copy of `AutofillTable`'s data. As a result, whenever a `Refresh()` happens,
// these pointer are invalidated. Do not store them as member variables, since a
// refresh through Sync can happen anytime.
//
// The PDM is a `KeyedService`. However, no separate instance exists for
// incognito mode. In incognito mode the original profile's PDM is used. It is
// the responsibility of the consumers of the PDM to ensure that no data from an
// incognito session is persisted unintentionally.
class PersonalDataManager : public KeyedService,
                            public history::HistoryServiceObserver,
                            public AddressDataManager::Observer,
                            public PaymentsDataManager::Observer {
 public:
  // Initializes the `address_data_manager_` and `payments_data_manager_` with
  // the provided services and triggers asynchronous loading of address and
  // payments data.
  PersonalDataManager(
      scoped_refptr<AutofillWebDataService> profile_database,
      scoped_refptr<AutofillWebDataService> account_database,
      PrefService* pref_service,
      PrefService* local_state,
      signin::IdentityManager* identity_manager,
      history::HistoryService* history_service,
      syncer::SyncService* sync_service,
      StrikeDatabaseBase* strike_database,
      AutofillImageFetcherBase* image_fetcher,
      std::unique_ptr<AutofillSharedStorageHandler> shared_storage_handler,
      std::string app_locale,
      std::string country_code);
  PersonalDataManager(const PersonalDataManager&) = delete;
  PersonalDataManager& operator=(const PersonalDataManager&) = delete;
  ~PersonalDataManager() override;

  // The (Address|Payments)DataManager classes are responsible for handling
  // address/payments specific functionality. All address or payments specific
  // code should go through them.
  AddressDataManager& address_data_manager() { return *address_data_manager_; }
  const AddressDataManager& address_data_manager() const {
    return *address_data_manager_;
  }
  PaymentsDataManager& payments_data_manager() {
    return *payments_data_manager_;
  }
  const PaymentsDataManager& payments_data_manager() const {
    return *payments_data_manager_;
  }

  // KeyedService:
  void Shutdown() override;

  // AddressDataManager::Observer:
  void OnAddressDataChanged() override;

  // PaymentsDataManager::Observer:
  void OnPaymentsDataChanged() override;

  // history::HistoryServiceObserver:
  // TODO(crbug.com/322170538): This is used to clear the crowdsourcing
  // manager's history. Consider moving the observer there.
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  void AddObserver(PersonalDataManagerObserver* observer);
  void RemoveObserver(PersonalDataManagerObserver* observer);

  // Depending on what the `guid` identifies, removes either an AutofillProfile,
  // a credit card or an IBAN.
  // TODO(crbug.com/322170538): Remove. Callers should use one of the following
  // functions instead:
  // - `address_data_manager().RemoveProfile()`.
  // - `payments_data_manager().RemoveByGUID()`.
  virtual void RemoveByGUID(const std::string& guid);

  // Returns whether the personal data has been loaded from the web database.
  virtual bool IsDataLoaded() const;

  // Re-loads profiles, credit cards, and IBANs from the WebDatabase
  // asynchronously. In the general case, this is a no-op and will re-create
  // the same in-memory model as existed prior to the call.  If any change
  // occurred to profiles in the WebDatabase directly, as is the case if the
  // browser sync engine processed a change from the cloud, we will learn of
  // these as a result of this call.
  void Refresh();

  // Returns the |app_locale_| that was provided during construction.
  const std::string& app_locale() const { return app_locale_; }

  // Triggers `OnPersonalDataChanged()` for all `observers_` if no address or
  // payment changes are pending.
  void NotifyPersonalDataObserver();

  // TODO(crbug.com/40100455): Consider moving this to the TestPDM or a TestAPI.
  void SetSyncServiceForTest(syncer::SyncService* sync_service);

 protected:
  // Responsible for all address-related logic of the PDM. Non-null.
  std::unique_ptr<AddressDataManager> address_data_manager_;

  // Responsible for all payments-related logic of the PDM. Non-null.
  std::unique_ptr<PaymentsDataManager> payments_data_manager_;

  // TODO(crbug.com/322170538): These observers are used to emulate the PDM
  // observer while it is being split into separate address and payments
  // observers. Remove once the PDMObserver is gone.
  base::ScopedObservation<AddressDataManager, AddressDataManager::Observer>
      address_data_manager_observation_{this};
  base::ScopedObservation<PaymentsDataManager, PaymentsDataManager::Observer>
      payments_data_manager_observation_{this};

  // The PrefService that this instance uses. Must outlive this instance.
  raw_ptr<PrefService> pref_service_ = nullptr;

 private:
  base::ObserverList<PersonalDataManagerObserver>::Unchecked observers_;

  // Stores the |app_locale| supplied on construction.
  const std::string app_locale_;

  // The HistoryService to be observed by the personal data manager. Must
  // outlive this instance. This unowned pointer is retained so the PDM can
  // remove itself from the history service's observer list on shutdown.
  raw_ptr<history::HistoryService> history_service_ = nullptr;

  base::ScopedObservation<history::HistoryService, HistoryServiceObserver>
      history_service_observation_{this};

  base::WeakPtrFactory<PersonalDataManager> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_
