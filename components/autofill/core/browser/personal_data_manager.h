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
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/account_info_getter.h"
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
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service_observer.h"

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
                            public history::HistoryServiceObserver,
                            public syncer::SyncServiceObserver,
                            public signin::IdentityManager::Observer,
                            public AccountInfoGetter {
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
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  // SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

  // AccountInfoGetter:
  CoreAccountInfo GetAccountInfoForPaymentsServer() const override;
  bool IsSyncFeatureEnabledForPaymentsServerMetrics() const override;

  // signin::IdentityManager::Observer:
  void OnAccountsCookieDeletedByUserAction() override;

  // Returns the account info of currently signed-in user, or std::nullopt if
  // the user is not signed-in or the identity manager is not available.
  std::optional<CoreAccountInfo> GetPrimaryAccountInfo() const;

  // Returns whether credit card download is active (meaning that wallet sync is
  // running at least in transport mode).
  bool IsPaymentsDownloadActive() const;

  // Returns true if wallet sync is running in transport mode (meaning that
  // Sync-the-feature is disabled).
  virtual bool IsPaymentsWalletSyncTransportEnabled() const;

  // Returns the current sync status for the purpose of metrics only (do not
  // guard actual logic behind this value).
  AutofillMetrics::PaymentsSigninState GetPaymentsSigninStateForMetrics() const;

  // Adds a listener to be notified of PersonalDataManager events.
  virtual void AddObserver(PersonalDataManagerObserver* observer);

  // Removes |observer| as an observer of this PersonalDataManager.
  virtual void RemoveObserver(PersonalDataManagerObserver* observer);

  // Called to indicate |profile_or_credit_card| was used (to fill in a form).
  // Updates the database accordingly.
  void RecordUseOf(absl::variant<const AutofillProfile*, const CreditCard*>
                       profile_or_credit_card);

  // Try to save a credit card locally. If the card already exists, do nothing
  // and return false. If the card is new, save it locally and return true.
  virtual bool SaveCardLocallyIfNew(const CreditCard& imported_credit_card);

  // Called when the user accepts the prompt to save the credit card locally.
  // Records some metrics and attempts to save the imported card. Returns the
  // guid of the new or updated card, or the empty string if no card was saved.
  std::string OnAcceptedLocalCreditCardSave(
      const CreditCard& imported_credit_card);

  // Returns the GUID of `imported_iban` if it is successfully added or updated,
  // or an empty string otherwise.
  // Called when the user accepts the prompt to save the IBAN locally.
  // The function will sets the GUID of `imported_iban` to the one that matches
  // it in `local_ibans_` so that UpdateIban() will be able to update the
  // specific IBAN.
  std::string OnAcceptedLocalIbanSave(Iban imported_iban);

  // Adds |profile| to the web database.
  virtual void AddProfile(const AutofillProfile& profile);

  // Updates |profile| which already exists in the web database.
  virtual void UpdateProfile(const AutofillProfile& profile);

  // Removes the profile, credit card or IBAN identified by `guid`.
  virtual void RemoveByGUID(const std::string& guid);

  // Returns the profile with the specified |guid|, or nullptr if there is no
  // profile with the specified |guid|.
  // TODO(crbug.com/1487119): Change return type to const AutofillProfile*
  AutofillProfile* GetProfileByGUID(const std::string& guid) const;

  // Determines whether the logged in user (if any) is eligible to store
  // Autofill address profiles to their account.
  virtual bool IsEligibleForAddressAccountStorage() const;

  // Users based in unsupported countries and profiles with a country value set
  // to an unsupported country are not eligible for account storage. This
  // function determines if the `country_code` is eligible.
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
  virtual std::string AddAsLocalIban(Iban iban);

  // Updates `iban` which already exists in the web database. This can only
  // be used on local ibans. Returns the guid of `iban` if the update is
  // successful, or an empty string otherwise.
  // This method assumes an IBAN exists; if not, it will be handled gracefully
  // by webdata backend.
  virtual std::string UpdateIban(const Iban& iban);

  // Adds |credit_card| to the web database as a local card.
  virtual void AddCreditCard(const CreditCard& credit_card);

  // Delete list of provided credit cards.
  virtual void DeleteLocalCreditCards(const std::vector<CreditCard>& cards);

  // Delete all local credit cards.
  virtual void DeleteAllLocalCreditCards();

  // Updates |credit_card| which already exists in the web database. This
  // can only be used on local credit cards.
  virtual void UpdateCreditCard(const CreditCard& credit_card);

  // Updates a local CVC in the web database.
  virtual void UpdateLocalCvc(const std::string& guid,
                              const std::u16string& cvc);

  // Updates the use stats and billing address id for the server |credit_cards|.
  // Looks up the cards by server_id.
  virtual void UpdateServerCardsMetadata(
      const std::vector<CreditCard>& credit_cards);

  // Methods to add, update, remove, or clear server CVC in the web database.
  virtual void AddServerCvc(int64_t instrument_id, const std::u16string& cvc);
  virtual void UpdateServerCvc(int64_t instrument_id,
                               const std::u16string& cvc);
  void RemoveServerCvc(int64_t instrument_id);
  virtual void ClearServerCvcs();

  // Method to clear all local CVCs from the local web database.
  virtual void ClearLocalCvcs();

  // Deletes all server cards (both masked and unmasked).
  void ClearAllServerDataForTesting();

  // Deletes all local profiles and cards.
  virtual void ClearAllLocalData();

  // Sets a server credit card for test.
  void AddServerCreditCardForTest(std::unique_ptr<CreditCard> credit_card);

  void AddIbanForTest(std::unique_ptr<Iban> iban) {
    payments_data_manager_->local_ibans_.push_back(std::move(iban));
  }

  // Returns whether server credit cards are stored in account (i.e. ephemeral)
  // storage.
  bool IsUsingAccountStorageForServerDataForTest() const;

  // Adds the offer data to local cache for tests. This does not affect data in
  // the real database.
  void AddOfferDataForTest(std::unique_ptr<AutofillOfferData> offer_data);

  // TODO(b/40100455): Consider moving this to the TestPDM or a TestAPI.
  void SetSyncServiceForTest(syncer::SyncService* sync_service);

  // Returns the credit card with the specified |guid|, or nullptr if there is
  // no credit card with the specified |guid|.
  virtual CreditCard* GetCreditCardByGUID(const std::string& guid);

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
  virtual std::vector<CreditCard*> GetLocalCreditCards() const;
  // Returns just server cards.
  virtual std::vector<CreditCard*> GetServerCreditCards() const;
  // Returns all credit cards, server and local.
  virtual std::vector<CreditCard*> GetCreditCards() const;

  // Returns local IBANs.
  virtual std::vector<const Iban*> GetLocalIbans() const;
  // Returns server IBANs.
  virtual std::vector<const Iban*> GetServerIbans() const;
  // Returns all IBANs, server and local.
  virtual std::vector<const Iban*> GetIbans() const;
  // Returns all IBANs, server and local. All local IBANs that share the same
  // prefix, suffix, and length as any existing server IBAN will be considered a
  // duplicate IBAN. These duplicate IBANs will not be returned in the list.
  virtual std::vector<const Iban*> GetIbansToSuggest() const;

  // Returns the Payments customer data. Returns nullptr if no data is present.
  virtual PaymentsCustomerData* GetPaymentsCustomerData() const;

  // Returns the credit card cloud token data.
  virtual std::vector<CreditCardCloudTokenData*> GetCreditCardCloudTokenData()
      const;

  // Returns autofill offer data, including card-linked and promo code offers.
  virtual std::vector<AutofillOfferData*> GetAutofillOffers() const;

  // Returns autofill offer data, but only promo code offers that are not
  // expired and that are for the given |origin|.
  std::vector<const AutofillOfferData*>
  GetActiveAutofillPromoCodeOffersForOrigin(GURL origin) const;

  // Return the URL for the card art image, if available.
  GURL GetCardArtURL(const CreditCard& credit_card) const;

  // Returns the customized credit card art image for the |card_art_url|. If no
  // image has been cached, an asynchronous request will be sent to fetch the
  // image and this function will return nullptr.
  virtual gfx::Image* GetCreditCardArtImageForUrl(
      const GURL& card_art_url) const;

  // Returns the cached card art image for the |card_art_url| if it was synced
  // locally to the client. This function is called within
  // GetCreditCardArtImageForUrl(), but can also be called separately as an
  // optimization for situations where a separate fetch request after trying to
  // retrieve local card art images is not needed. If the card art image is not
  // present in the cache, this function will return a nullptr.
  gfx::Image* GetCachedCardArtImageForUrl(const GURL& card_art_url) const;

  // Returns the profiles to suggest to the user for filling, ordered by
  // frecency.
  // TODO(crbug.com/1487119): Change return type to
  // std::vector<const AutofillProfile*>
  std::vector<AutofillProfile*> GetProfilesToSuggest() const;

  // Returns all `GetProfiles()` in the order that the should be shown in the
  // settings.
  // TODO(crbug.com/1487119): Change return type to
  // std::vector<const AutofillProfile*>
  std::vector<AutofillProfile*> GetProfilesForSettings() const;

  // Returns the credit cards to suggest to the user. Those have been deduped
  // and ordered by frecency with the expired cards put at the end of the
  // vector.
  std::vector<CreditCard*> GetCreditCardsToSuggest() const;

  // Returns the masked bank accounts that can be suggested to the user.
  std::vector<BankAccount> GetMaskedBankAccounts() const;

  // Re-loads profiles, credit cards, and IBANs from the WebDatabase
  // asynchronously. In the general case, this is a no-op and will re-create
  // the same in-memory model as existed prior to the call.  If any change
  // occurred to profiles in the WebDatabase directly, as is the case if the
  // browser sync engine processed a change from the cloud, we will learn of
  // these as a result of this call.
  //
  // Also see SetProfile for more details.
  virtual void Refresh();

  // Returns the |app_locale_| that was provided during construction.
  const std::string& app_locale() const { return app_locale_; }

  // Returns the country code that was provided from the variations service
  // during construction.
  const std::string& variations_country_code_for_testing() const {
    return variations_country_code_;
  }

  // Sets the country code from the variations service.
  void set_variations_country_code_for_testing(std::string country_code) {
    variations_country_code_ = country_code;
  }

  // Returns our best guess for the country a user is likely to use when
  // inputting a new address. The value is calculated once and cached, so it
  // will only update when Chrome is restarted.
  virtual const std::string& GetDefaultCountryCodeForNewAddress() const;

  // Returns our best guess for the country a user is in, for experiment group
  // purposes. The value is calculated once and cached, so it will only update
  // when Chrome is restarted.
  const std::string& GetCountryCodeForExperimentGroup() const;

  // Returns all virtual card usage data linked to the credit card.
  virtual std::vector<VirtualCardUsageData*> GetVirtualCardUsageData() const;

  // Check if `credit_card` has a duplicate card present in either Local or
  // Server card lists.
  bool IsCardPresentAsBothLocalAndServerCards(const CreditCard& credit_card);

  // Returns a pointer to the server card that has duplicate information of the
  // `local_card`. It is not guaranteed that a server card is found. If not,
  // nullptr is returned.
  const CreditCard* GetServerCardForLocalCard(
      const CreditCard* local_card) const;

  bool HasPendingPaymentQueriesForTesting() const {
    return payments_data_manager_->HasPendingPaymentQueries();
  }

  // This function assumes |credit_card| contains the full PAN. Returns |true|
  // if the card number of |credit_card| is equal to any local card or any
  // unmasked server card known by the browser, or |TypeAndLastFourDigits| of
  // |credit_card| is equal to any masked server card known by the browser.
  bool IsKnownCard(const CreditCard& credit_card) const;

  // Check whether a card is a server card or has a duplicated server card.
  bool IsServerCard(const CreditCard* credit_card) const;

  // Sets the value that can skip the checks to see if we are syncing in a test.
  void SetSyncingForTest(bool is_syncing_for_test) {
    is_syncing_for_test_ = is_syncing_for_test;
  }

  // Returns whether a row to give the option of showing cards from the user's
  // account should be shown in the dropdown.
  bool ShouldShowCardsFromAccountOption() const;

  // Triggered when a user selects the option to see cards from their account.
  // Records the sync transport consent.
  void OnUserAcceptedCardsFromAccountOption();

  // Logs the fact that the server card link was clicked including information
  // about the current sync state.
  void LogServerCardLinkClicked() const;

  // Logs the fact that the server IBAN link was clicked including information
  // about the current sync state.
  void LogServerIbanLinkClicked() const;

  // Records the sync transport consent if the user is in sync transport mode.
  virtual void OnUserAcceptedUpstreamOffer();

  // Triggers `OnPersonalDataChanged()` for all `observers_`.
  // Additionally, if all of the PDM's pending operations have finished, meaning
  // that the data exposed through the PDM matches the database,
  // `OnPersonalDataFinishedProfileTasks()` is triggered.
  void NotifyPersonalDataObserver();

  // TODO(crbug.com/1337392): Revisit the function when card upload feedback is
  // to be added again. In the new proposal, we may not need to go through PDM.
  // Called when at least one (can be multiple) card was saved. |is_local_card|
  // indicates if the card is saved to local storage.
  void OnCreditCardSaved(bool is_local_card);

  // Returns true if either Profile or CreditCard Autofill is enabled.
  virtual bool IsAutofillEnabled() const;

  // Returns whether sync's integration with payments is on.
  virtual bool IsAutofillWalletImportEnabled() const;

  // Sets |credit_cards_| to the contents of |credit_cards| and updates the web
  // database by adding, updating and removing credit cards.
  void SetCreditCards(std::vector<CreditCard>* credit_cards);

  // Returns true if Sync-the-feature is enabled and
  // UserSelectableType::kAutofill is among the user's selected data types.
  // TODO(crbug.com/40066949): Remove this method once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  bool IsSyncFeatureEnabledForAutofill() const;

  // Returns true if the user's selectable `type` is enabled.
  bool IsUserSelectableTypeEnabled(syncer::UserSelectableType type) const;

  // Sets the Sync UserSelectableType::kAutofill toggle value.
  // TODO(crbug.com/1502843): Used for the toggle on the Autofill Settings page
  // only. It controls syncing of autofill data stored in user accounts for
  // non-syncing users. Remove when toggle becomes available on the Sync page.
  void SetAutofillSelectableTypeEnabled(bool enabled);

  // The functions below are related to the payments mandatory re-auth feature.
  // All of this functionality is done through per-profile per-device prefs.
  // `SetPaymentMethodsMandatoryReauthEnabled()` is used to update the opt-in
  // status of the feature, and is called when a user successfully completes a
  // full re-auth opt-in flow (with a successful authentication).
  // `IsPaymentMethodsMandatoryReauthEnabled()` is checked before triggering the
  // re-auth feature during a payments autofill flow.
  // `ShouldShowPaymentMethodsMandatoryReauthPromo()` is used to check whether
  // we should show the re-auth opt-in promo once a user submits a form, and
  // there was no interactive authentication for the most recent payments
  // autofill flow. `IncrementPaymentMethodsMandatoryReauthPromoShownCounter()`
  // increments the counter that denotes the number of times that the promo has
  // been shown, and this counter is used very similarly to a strike database
  // when it comes time to check whether we should show the promo.
  virtual void SetPaymentMethodsMandatoryReauthEnabled(bool enabled);
  virtual bool IsPaymentMethodsMandatoryReauthEnabled();
  bool ShouldShowPaymentMethodsMandatoryReauthPromo();
  void IncrementPaymentMethodsMandatoryReauthPromoShownCounter();

  // Returns true if the user pref to store CVC is enabled.
  virtual bool IsPaymentCvcStorageEnabled();

  // Get pointer to the image fetcher.
  AutofillImageFetcherBase* GetImageFetcher() const;

  // Defines whether the Sync toggle on the Autofill Settings page is visible.
  // TODO(crbug.com/1502843): Remove when toggle becomes available on the Sync
  // page for non-syncing users.
  bool IsAutofillSyncToggleAvailable() const;

  // Used to automatically import addresses without a prompt. Should only be
  // set to true in tests.
  void set_auto_accept_address_imports_for_testing(bool auto_accept) {
    auto_accept_address_imports_for_testing_ = auto_accept;
  }
  bool auto_accept_address_imports_for_testing() {
    return auto_accept_address_imports_for_testing_;
  }

  void set_test_addresses(std::vector<AutofillProfile> test_addresses) {
    test_addresses_ = test_addresses;
  }

  const std::vector<AutofillProfile>& test_addresses() const {
    return test_addresses_;
  }

  // Adds `credit_card` to the web database as a full server card.
  //
  // It is no longer possible for users to reach this path as full server cards
  // have been deprecated, however tests still use this when testing
  // still-supported paths (filling, editing, and deleting full server cards).
  void AddFullServerCreditCardForTesting(const CreditCard& credit_card);

  AlternativeStateNameMapUpdater*
  get_alternative_state_name_map_updater_for_testing() {
    return alternative_state_name_map_updater_.get();
  }

  std::optional<signin::AccountManagedStatusFinder::Outcome>
  GetAccountStatusForTesting() const;

 protected:
  friend class PaymentsDataCleaner;
  // TODO(b/322170538): The `PaymentsDataManager` shouldn't depend on the PDM
  // at all, let alone befriend it.
  friend class PaymentsDataManager;

  // Whether server cards or IBANs are enabled and should be suggested to the
  // user.
  virtual bool ShouldSuggestServerPaymentMethods() const;

  // Responsible for all address-related logic of the PDM.
  // Non-null after `Init()`.
  std::unique_ptr<AddressDataManager> address_data_manager_;

  // Responsible for all payments-related logic of the PDM.
  // Non-null after `Init()`.
  std::unique_ptr<PaymentsDataManager> payments_data_manager_;

  // The observers.
  base::ObserverList<PersonalDataManagerObserver>::Unchecked observers_;

  // Used to populate AlternativeStateNameMap with the geographical state data
  // (including their abbreviations and localized names).
  std::unique_ptr<AlternativeStateNameMapUpdater>
      alternative_state_name_map_updater_;

  // The PrefService that this instance uses. Must outlive this instance.
  raw_ptr<PrefService> pref_service_ = nullptr;

 private:
  // Sets (or resets) the Sync service, which may not have started yet
  // but its preferences can already be queried. Can also be a nullptr
  // if it is disabled by CLI.
  void SetSyncService(syncer::SyncService* sync_service);

  // Saves |imported_credit_card| to the WebDB if it exists. Returns the guid of
  // the new or updated card, or the empty string if no card was saved.
  virtual std::string SaveImportedCreditCard(
      const CreditCard& imported_credit_card);

  // Returns the database that is used for storing local data.
  scoped_refptr<AutofillWebDataService> GetLocalDatabase();

  // Stores the |app_locale| supplied on construction.
  const std::string app_locale_;

  // Stores the country code that was provided from the variations service
  // during construction.
  std::string variations_country_code_;

  // If true, new addresses imports are automatically accepted without a prompt.
  // Only to be used for testing.
  bool auto_accept_address_imports_for_testing_ = false;

  // The determined country code for experiment group purposes. Uses
  // |variations_country_code_| if it exists but falls back to other methods if
  // necessary to ensure it always has a value.
  mutable std::string experiment_country_code_;

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

  // Used for the Autofill sync toggle visibility calculation only.
  // TODO(crbug.com/1502843): Remove when toggle becomes available on the Sync
  // page for non-syncing users.
  std::unique_ptr<const signin::AccountManagedStatusFinder>
      account_status_finder_;

  // The sync service this instances uses. Must outlive this instance.
  raw_ptr<syncer::SyncService> sync_service_ = nullptr;

  // Whether sync should be considered on in a test.
  bool is_syncing_for_test_ = false;

  // Test addresses used to allow developers to test their forms.
  std::vector<AutofillProfile> test_addresses_;

  base::ScopedObservation<history::HistoryService, HistoryServiceObserver>
      history_service_observation_{this};

  base::WeakPtrFactory<PersonalDataManager> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_
