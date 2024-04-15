// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_

#include <deque>
#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/address_data_cleaner.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_shared_storage_handler.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/strike_databases/strike_database_base.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/signatures.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_member.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"

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
// Since `AutofillTable` lives on a separate sequence, changes posted to the PDM
// are asynchronous. They only become effective in the PDM after/if the
// corresponding database operation successfully finished.
//
// Sync writes to `AutofillTable` directly, since sync bridges live on the same
// sequence. In this case, the PDM is notified via
// `AutofillWebDataServiceObserverOnUISequence::OnAutofillChangedBySync()` and
// it reloads all its data from `AutofillTable`. This is done via an operation
// called `Refresh()`.
//
// PDM getters such as `GetProfiles()` expose pointers to the PDM's internal
// copy of `AutofillTable`'s data. As a result, whenever the PDM reloads any
// data, these pointer are invalidated. Do not store them as member variables,
// since a refresh through Sync can happen anytime.
//
// The PDM is a `KeyedService`. However, no separate instance exists for
// incognito mode. In incognito mode the original profile's PDM is used. It is
// the responsibility of the consumers of the PDM to ensure that no data from an
// incognito session is persisted unintentionally.
//
// Technical details on how changes are implemented:
// The mechanism works differently for `AutofillProfile` and `CreditCard`.

// CreditCards simply post a task to the DB sequence and trigger a `Refresh()`.
// Since `Refresh()` itself simply posts several read requests on the DB
// sequence, and because the DB sequence is a sequence, the `Refresh()` is
// guaranteed to read the latest data. This is unnecessarily inefficient, since
// any change causes the PDM to reload all of its data.
//
// AutofillProfile queues pending changes in `ongoing_profile_changes_`. For
// each profile, they are executed in order and the next change is only posted
// to the DB sequence once the previous change has finished.
// After each change that finishes, the `AutofillWebDataService` notifies the
// PDM via `PersonalDataManager::OnAutofillProfileChanged(change)` - and the PDM
// updates its state accordingly. No `Refresh()` is performed.
// Queuing the pending modifications is necessary, so the PDM can do consistency
// checks against the latest state. For example, a remove should only be
// performed if the profile exists. Without the queuing, if a remove operation
// was posted before the add operation has finished, the remove would
// incorrectly get rejected by the PDM.
class PersonalDataManager : public KeyedService,
                            public history::HistoryServiceObserver {
 public:
  using ProfileOrder = AddressDataManager::ProfileOrder;

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
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  // Returns the account info of currently signed-in user, or std::nullopt if
  // the user is not signed-in or the identity manager is not available.
  std::optional<CoreAccountInfo> GetPrimaryAccountInfo() const;

  // TODO(b/322170538): Update the remaining callers to use the PayDM.
  bool IsPaymentsDownloadActive() const {
    return payments_data_manager_->IsPaymentsDownloadActive();
  }

  // Adds a listener to be notified of PersonalDataManager events.
  virtual void AddObserver(PersonalDataManagerObserver* observer);

  // Adds a callback which will be triggered on the next personal data change,
  // at the same time `PersonalDataManagerObserver::OnPersonalDataChanged()` of
  // `observers_` is called.
  void AddChangeCallback(base::OnceClosure callback);

  // Removes |observer| as an observer of this PersonalDataManager.
  virtual void RemoveObserver(PersonalDataManagerObserver* observer);

  // Called to indicate |profile_or_credit_card| was used (to fill in a form).
  // Updates the database accordingly.
  void RecordUseOf(absl::variant<const AutofillProfile*, const CreditCard*>
                       profile_or_credit_card);

  // Adds |profile| to the web database.
  void AddProfile(const AutofillProfile& profile);

  // Updates |profile| which already exists in the web database.
  void UpdateProfile(const AutofillProfile& profile);

  // Removes the profile, credit card or IBAN identified by `guid`.
  virtual void RemoveByGUID(const std::string& guid);

  // Returns the profile with the specified |guid|, or nullptr if there is no
  // profile with the specified |guid|.
  // TODO(crbug.com/1487119): Change return type to const AutofillProfile*
  AutofillProfile* GetProfileByGUID(const std::string& guid) const;

  // TODO(b/322170538): Deprecated. Use the functions in
  // `address_data_manager()` instead. Migrate remaining callers.
  bool IsCountryEligibleForAccountStorage(std::string_view country_code) const;

  // Migrates a given kLocalOrSyncable `profile` to source kAccount. This has
  // multiple side-effects for the profile:
  // - It is stored in a different backend.
  // - It receives a new GUID.
  // Like all database operations, the migration happens asynchronously.
  // `profile` (the kLocalOrSyncable one) will not be available in the
  // PersonalDataManager anymore once the migrating has finished.
  void MigrateProfileToAccount(const AutofillProfile& profile);

  // Adds `iban` to the web database as a local IBAN. Returns the guid of
  // `iban` if the add is successful, or an empty string otherwise.
  // Below conditions should be met before adding `iban` to the database:
  // 1) IBAN saving must be enabled.
  // 2) No IBAN exists in `local_ibans_` which has the same guid as`iban`.
  // 3) Local database is available.
  std::string AddAsLocalIban(Iban iban);

  // Adds |credit_card| to the web database as a local card.
  void AddCreditCard(const CreditCard& credit_card);

  // Updates |credit_card| which already exists in the web database. This
  // can only be used on local credit cards.
  void UpdateCreditCard(const CreditCard& credit_card);

  // Deletes all server cards (both masked and unmasked).
  void ClearAllServerDataForTesting();

  // Sets a server credit card for test.
  //
  // TODO(crbug.com/330865438): This method currently sets `server_cards_`
  // directly which is not correct for the real PersonalDataManager. It should
  // be moved to TestPersonalDataManager, and unittests should switch to that.
  void AddServerCreditCardForTest(std::unique_ptr<CreditCard> credit_card);

  // Returns whether server credit cards are stored in account (i.e. ephemeral)
  // storage.
  bool IsUsingAccountStorageForServerDataForTest() const;

  // TODO(b/40100455): Consider moving this to the TestPDM or a TestAPI.
  void SetSyncServiceForTest(syncer::SyncService* sync_service);

  // Returns the credit card with the specified |guid|, or nullptr if there is
  // no credit card with the specified |guid|.
  CreditCard* GetCreditCardByGUID(const std::string& guid);

  // Returns the credit card with the specified `number`, or nullptr if there is
  // no credit card with the specified `number`.
  CreditCard* GetCreditCardByNumber(const std::string& number);

  // Returns the credit card with the specified |instrument_id|, or nullptr if
  // there is no credit card with the specified |instrument_id|.
  CreditCard* GetCreditCardByInstrumentId(int64_t instrument_id);

  // Returns the credit card with the given server id, or nullptr if there is no
  // match.
  CreditCard* GetCreditCardByServerId(const std::string& server_id);

  // Add the credit-card-linked benefit to local cache for tests. This does
  // not affect data in the real database.
  void AddCreditCardBenefitForTest(CreditCardBenefit benefit) {
    payments_data_manager_->credit_card_benefits_.push_back(std::move(benefit));
  }

  // Returns whether the personal data has been loaded from the web database.
  virtual bool IsDataLoaded() const;

  // This PersonalDataManager owns these profiles and credit cards. Their
  // lifetime is until the web database is updated with new profile and credit
  // card information, respectively.
  // `GetProfiles()` returns all `kAccount` and `kLocalOrSyncable` profiles. By
  // using `GetProfilesFromSource()`, profiles from a single source are be
  // retrieved.
  // The profiles are returned in the specified `order`.
  // TODO(crbug.com/1487119): Change return type to
  // std::vector<const AutofillProfile*>
  std::vector<AutofillProfile*> GetProfiles(
      ProfileOrder order = ProfileOrder::kNone) const;
  // TODO(crbug.com/1487119): Change return type to
  // std::vector<const AutofillProfile*>
  std::vector<AutofillProfile*> GetProfilesFromSource(
      AutofillProfile::Source profile_source,
      ProfileOrder order = ProfileOrder::kNone) const;
  // Returns just LOCAL_CARD cards.
  std::vector<CreditCard*> GetLocalCreditCards() const;
  // Returns just server cards.
  std::vector<CreditCard*> GetServerCreditCards() const;
  // Returns all credit cards, server and local.
  std::vector<CreditCard*> GetCreditCards() const;

  // Returns the Payments customer data. Returns nullptr if no data is present.
  PaymentsCustomerData* GetPaymentsCustomerData() const;

  // Returns the credit card cloud token data.
  std::vector<CreditCardCloudTokenData*> GetCreditCardCloudTokenData() const;

  // Returns autofill offer data, including card-linked and promo code offers.
  std::vector<AutofillOfferData*> GetAutofillOffers() const;

  // Returns autofill offer data, but only promo code offers that are not
  // expired and that are for the given |origin|.
  std::vector<const AutofillOfferData*>
  GetActiveAutofillPromoCodeOffersForOrigin(GURL origin) const;

  // Return the URL for the card art image, if available.
  GURL GetCardArtURL(const CreditCard& credit_card) const;

  // Returns the customized credit card art image for the |card_art_url|. If no
  // image has been cached, an asynchronous request will be sent to fetch the
  // image and this function will return nullptr.
  gfx::Image* GetCreditCardArtImageForUrl(const GURL& card_art_url) const;

  // TODO(b/322170538): Deprecated. Use the functions in
  // `address_data_manager()` instead. Migrate remaining callers.
  std::vector<AutofillProfile*> GetProfilesToSuggest() const;

  // TODO(b/322170538): Deprecated. Use the functions in
  // `address_data_manager()` instead. Migrate remaining callers.
  std::vector<AutofillProfile*> GetProfilesForSettings() const;

  // Returns the credit cards to suggest to the user. Those have been deduped
  // and ordered by frecency with the expired cards put at the end of the
  // vector.
  std::vector<CreditCard*> GetCreditCardsToSuggest() const;

  // Re-loads profiles, credit cards, and IBANs from the WebDatabase
  // asynchronously. In the general case, this is a no-op and will re-create
  // the same in-memory model as existed prior to the call.  If any change
  // occurred to profiles in the WebDatabase directly, as is the case if the
  // browser sync engine processed a change from the cloud, we will learn of
  // these as a result of this call.
  //
  // Also see SetProfile for more details.
  void Refresh();

  // Returns the |app_locale_| that was provided during construction.
  const std::string& app_locale() const { return app_locale_; }

  // Returns all virtual card usage data linked to the credit card.
  std::vector<VirtualCardUsageData*> GetVirtualCardUsageData() const;

  bool HasPendingPaymentQueriesForTesting() const {
    return payments_data_manager_->HasPendingPaymentQueries();
  }

  void SetSyncingForTest(bool is_syncing_for_test) {
    payments_data_manager_->SetSyncingForTest(is_syncing_for_test);
  }

  // Triggers `OnPersonalDataChanged()` for all `observers_`.
  // Additionally, if all of the PDM's pending operations have finished, meaning
  // that the data exposed through the PDM matches the database,
  // `OnPersonalDataFinishedProfileTasks()` is triggered.
  void NotifyPersonalDataObserver();

  // Returns true if either Profile or CreditCard Autofill is enabled.
  bool IsAutofillEnabled() const;

  // Sets |credit_cards_| to the contents of |credit_cards| and updates the web
  // database by adding, updating and removing credit cards.
  void SetCreditCards(std::vector<CreditCard>* credit_cards);

  // TODO(b/322170538): Deprecated. Use the functions in
  // `payments_data_manager()` instead. Some callers on iOS still rely on this.
  void SetPaymentMethodsMandatoryReauthEnabled(bool enabled);
  bool IsPaymentMethodsMandatoryReauthEnabled();

  // Used to automatically import addresses without a prompt. Should only be
  // set to true in tests.
  void set_auto_accept_address_imports_for_testing(bool auto_accept) {
    auto_accept_address_imports_for_testing_ = auto_accept;
  }
  bool auto_accept_address_imports_for_testing() {
    return auto_accept_address_imports_for_testing_;
  }

  AlternativeStateNameMapUpdater*
  get_alternative_state_name_map_updater_for_testing() {
    return alternative_state_name_map_updater_.get();
  }

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

  // If true, new addresses imports are automatically accepted without a prompt.
  // Only to be used for testing.
  bool auto_accept_address_imports_for_testing_ = false;

  // The HistoryService to be observed by the personal data manager. Must
  // outlive this instance. This unowned pointer is retained so the PDM can
  // remove itself from the history service's observer list on shutdown.
  raw_ptr<history::HistoryService> history_service_ = nullptr;

  // The AddressDataCleaner is used to apply various cleanups (e.g.
  // deduplication, disused address removal) at browser startup or when the sync
  // starts.
  std::unique_ptr<AddressDataCleaner> address_data_cleaner_;

  // The identity manager that this instance uses. Must outlive this instance.
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  base::ScopedObservation<history::HistoryService, HistoryServiceObserver>
      history_service_observation_{this};

  base::WeakPtrFactory<PersonalDataManager> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_
