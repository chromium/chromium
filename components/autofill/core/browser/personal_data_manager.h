// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/address_data_cleaner.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_shared_storage_handler.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;
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
// TODO(b/322170538): Currently, only the PDM can be observed. Split the PDM
// observer into an ADM and a PayDM observer.
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
                            public history::HistoryServiceObserver {
 public:
  explicit PersonalDataManager(const std::string& app_locale);
  PersonalDataManager(const std::string& app_locale,
                      const std::string& country_code);
  PersonalDataManager(const PersonalDataManager&) = delete;
  PersonalDataManager& operator=(const PersonalDataManager&) = delete;
  ~PersonalDataManager() override;

  // Kicks off asynchronous loading of profiles and credit cards.
  // |profile_database| is a profile-scoped database that will be used to save
  // local cards. |account_database| is scoped to the currently signed-in
  // account, and is wiped on signout and browser exit. This can be a nullptr
  // if personal_data_manager should use |profile_database| for all data.
  // If passed in, the |account_database| is used by default for server cards.
  // |pref_service| must outlive this instance. |sync_service| is either null
  // (sync disabled by CLI) or outlives this object, it may not have started yet
  // but its preferences can already be queried. |image_fetcher| is to fetch the
  // customized images for autofill data.
  // TODO(b/40100455): Merge with the constructor?
  void Init(
      scoped_refptr<AutofillWebDataService> profile_database,
      scoped_refptr<AutofillWebDataService> account_database,
      PrefService* pref_service,
      PrefService* local_state,
      signin::IdentityManager* identity_manager,
      history::HistoryService* history_service,
      syncer::SyncService* sync_service,
      StrikeDatabaseBase* strike_database,
      AutofillImageFetcherBase* image_fetcher,
      std::unique_ptr<AutofillSharedStorageHandler> shared_storage_handler);

  // The (Address|Payments)DataManager classes are responsible for handling
  // address/payments specific functionality. All new address or payments
  // specific code should go through them.
  // TODO(b/322170538): Migrate existing callers.
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

  // history::HistoryServiceObserver
  // TODO(b/322170538): This is used to clear the crowdsourcing manager's
  // history. Consider moving the observer there.
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  // Returns the account info of currently signed-in user, or std::nullopt if
  // the user is not signed-in or the identity manager is not available.
  std::optional<CoreAccountInfo> GetPrimaryAccountInfo() const;

  // Adds a listener to be notified of PersonalDataManager events.
  virtual void AddObserver(PersonalDataManagerObserver* observer);

  // Adds a callback which will be triggered on the next personal data change,
  // at the same time `PersonalDataManagerObserver::OnPersonalDataChanged()` of
  // `observers_` is called.
  void AddChangeCallback(base::OnceClosure callback);

  // Removes |observer| as an observer of this PersonalDataManager.
  virtual void RemoveObserver(PersonalDataManagerObserver* observer);

  // Depending on what the `guid` identifies, removes either an AutofillProfile,
  // a credit card or an IBAN.
  // TODO(b/322170538): Remove. Callers should use one of the following
  // functions instead:
  // - `address_data_manager().RemoveProfile()`.
  // - `payments_data_manager().RemoveByGUID()`.
  virtual void RemoveByGUID(const std::string& guid);

  // Returns whether the personal data has been loaded from the web database.
  virtual bool IsDataLoaded() const;

  // All of the following functions simply forward the call to a function of the
  // same name in the `address_data_manager()` or the `payments_data_manager().
  // They should not be used anymore. Instead, callers should use the function
  // in the address/payments data manager instead.
  // TODO(b/322170538): Migrate existing callers.
  void AddProfile(const AutofillProfile& profile);
  void UpdateProfile(const AutofillProfile& profile);
  void AddCreditCard(const CreditCard& credit_card);
  void UpdateCreditCard(const CreditCard& credit_card);
  void ClearAllServerDataForTesting();
  void AddServerCreditCardForTest(std::unique_ptr<CreditCard> credit_card);
  bool IsUsingAccountStorageForServerDataForTest() const;
  CreditCard* GetCreditCardByGUID(const std::string& guid);
  CreditCard* GetCreditCardByNumber(const std::string& number);
  CreditCard* GetCreditCardByInstrumentId(int64_t instrument_id);
  CreditCard* GetCreditCardByServerId(const std::string& server_id);
  void AddCreditCardBenefitForTest(CreditCardBenefit benefit);
  std::vector<AutofillProfile*> GetProfiles(
      AddressDataManager::ProfileOrder order =
          AddressDataManager::ProfileOrder::kNone) const;
  std::vector<CreditCard*> GetLocalCreditCards() const;
  std::vector<CreditCard*> GetServerCreditCards() const;
  std::vector<CreditCard*> GetCreditCards() const;
  PaymentsCustomerData* GetPaymentsCustomerData() const;
  std::vector<CreditCardCloudTokenData*> GetCreditCardCloudTokenData() const;
  std::vector<AutofillOfferData*> GetAutofillOffers() const;
  std::vector<const AutofillOfferData*>
  GetActiveAutofillPromoCodeOffersForOrigin(GURL origin) const;
  GURL GetCardArtURL(const CreditCard& credit_card) const;
  gfx::Image* GetCreditCardArtImageForUrl(const GURL& card_art_url) const;
  std::vector<CreditCard*> GetCreditCardsToSuggest() const;
  bool HasPendingPaymentQueriesForTesting() const;
  void SetSyncingForTest(bool is_syncing_for_test);
  void SetCreditCards(std::vector<CreditCard>* credit_cards);
  void SetPaymentMethodsMandatoryReauthEnabled(bool enabled);
  bool IsPaymentMethodsMandatoryReauthEnabled();

  // Re-loads profiles, credit cards, and IBANs from the WebDatabase
  // asynchronously. In the general case, this is a no-op and will re-create
  // the same in-memory model as existed prior to the call.  If any change
  // occurred to profiles in the WebDatabase directly, as is the case if the
  // browser sync engine processed a change from the cloud, we will learn of
  // these as a result of this call.
  void Refresh();

  // Returns the |app_locale_| that was provided during construction.
  const std::string& app_locale() const { return app_locale_; }

  // Triggers `OnPersonalDataChanged()` for all `observers_`.
  // Additionally, if all of the PDM's pending operations have finished, meaning
  // that the data exposed through the PDM matches the database,
  // `OnPersonalDataFinishedProfileTasks()` is triggered.
  void NotifyPersonalDataObserver();

  // Returns true if either Profile or CreditCard Autofill is enabled.
  bool IsAutofillEnabled() const;

  AlternativeStateNameMapUpdater*
  get_alternative_state_name_map_updater_for_testing() {
    return alternative_state_name_map_updater_.get();
  }

  // TODO(b/40100455): Consider moving this to the TestPDM or a TestAPI.
  void SetSyncServiceForTest(syncer::SyncService* sync_service);

 protected:
  // Responsible for all address-related logic of the PDM.
  // Non-null after `Init()`.
  std::unique_ptr<AddressDataManager> address_data_manager_;

  // Responsible for all payments-related logic of the PDM.
  // Non-null after `Init()`.
  std::unique_ptr<PaymentsDataManager> payments_data_manager_;

  // The observers.
  base::ObserverList<PersonalDataManagerObserver>::Unchecked observers_;

  // The list of change callbacks. All of them are being triggered in
  // `NotifyPersonalDataObserver()` and then the list is cleared.
  std::vector<base::OnceClosure> change_callbacks_;

  // Used to populate AlternativeStateNameMap with the geographical state data
  // (including their abbreviations and localized names).
  // TODO(b/322170538): Move to `address_data_manager()`. Since it depends on
  // the observer, this requires splitting the observer first.
  std::unique_ptr<AlternativeStateNameMapUpdater>
      alternative_state_name_map_updater_;

  // The PrefService that this instance uses. Must outlive this instance.
  raw_ptr<PrefService> pref_service_ = nullptr;

 private:
  // Stores the |app_locale| supplied on construction.
  const std::string app_locale_;

  // Stores the country code that was provided from the variations service
  // during construction.
  const GeoIpCountryCode variations_country_code_;

  // The HistoryService to be observed by the personal data manager. Must
  // outlive this instance. This unowned pointer is retained so the PDM can
  // remove itself from the history service's observer list on shutdown.
  raw_ptr<history::HistoryService> history_service_ = nullptr;

  // The AddressDataCleaner is used to apply various cleanups (e.g.
  // deduplication, disused address removal) at browser startup or when the sync
  // starts.
  // TODO(b/322170538): Move to `address_data_manager()`. Since it depends on
  // the observer, this requires splitting the observer first.
  std::unique_ptr<AddressDataCleaner> address_data_cleaner_;

  // The identity manager that this instance uses. Must outlive this instance.
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  base::ScopedObservation<history::HistoryService, HistoryServiceObserver>
      history_service_observation_{this};

  base::WeakPtrFactory<PersonalDataManager> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_
