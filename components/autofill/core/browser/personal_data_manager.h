// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_

#include <deque>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/account_info_getter.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/personal_data_manager_cleaner.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/strike_databases/autofill_profile_migration_strike_database.h"
#include "components/autofill/core/browser/strike_databases/autofill_profile_save_strike_database.h"
#include "components/autofill/core/browser/strike_databases/autofill_profile_update_strike_database.h"
#include "components/autofill/core/browser/strike_databases/strike_database_base.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PaymentsSuggestionBottomSheetMediatorTest;
class Profile;
class PrefService;
class RemoveAutofillTester;

namespace autofill {
class AutofillImageFetcherBase;
class AutofillInteractiveTest;
struct CreditCardArtImage;
class PersonalDataManagerObserver;
class PersonalDataManagerFactory;
class PersonalDatabaseHelper;
}  // namespace autofill

namespace autofill_helper {
void SetCreditCards(int, std::vector<autofill::CreditCard>*);
}  // namespace autofill_helper

namespace syncer {
class SyncService;
}  // namespace syncer

namespace autofill {

// Handles loading and saving Autofill profile information to the web database.
// This class also stores the profiles loaded from the database for use during
// Autofill.
// The `PersonalDataManager` is a `KeyedService`. However, no separate instance
// exists for incognito mode. In incognito mode the original profile's
// `PersonalDataManager` is used. It is the responsibility of the consumers of
// the `PersonalDataManager` to ensure that no data from an incognito session is
// persisted unintentionally.
class PersonalDataManager : public KeyedService,
                            public WebDataServiceConsumer,
                            public AutofillWebDataServiceObserverOnUISequence,
                            public history::HistoryServiceObserver,
                            public syncer::SyncServiceObserver,
                            public signin::IdentityManager::Observer,
                            public AccountInfoGetter {
 public:
  // Profiles can be retrieved from the PersonalDataManager in different orders.
  enum class ProfileOrder {
    // Arbitrary order.
    kNone,
    // In descending order of frecency
    // (`AutofillProfile::HasGreaterRankingThan())`.
    kHighestFrecencyDesc,
    // Most recently modified profiles first.
    kMostRecentlyModifiedDesc,
    // Most recently used profiles first.
    kMostRecentlyUsedFirstDesc,
    kMaxValue = kMostRecentlyUsedFirstDesc
  };

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
  void Init(scoped_refptr<AutofillWebDataService> profile_database,
            scoped_refptr<AutofillWebDataService> account_database,
            PrefService* pref_service,
            PrefService* local_state,
            signin::IdentityManager* identity_manager,
            history::HistoryService* history_service,
            syncer::SyncService* sync_service,
            StrikeDatabaseBase* strike_database,
            AutofillImageFetcherBase* image_fetcher);

  // KeyedService:
  void Shutdown() override;

  // history::HistoryServiceObserver
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

  // AutofillWebDataServiceObserverOnUISequence:
  void AutofillMultipleChangedBySync() override;
  void AutofillAddressConversionCompleted() override;
  void SyncStarted(syncer::ModelType model_type) override;

  // SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncPaymentsIntegrationEnabledChanged(
      syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // AccountInfoGetter:
  CoreAccountInfo GetAccountInfoForPaymentsServer() const override;
  bool IsSyncFeatureEnabledForPaymentsServerMetrics() const override;

  // signin::IdentityManager::Observer:
  void OnAccountsCookieDeletedByUserAction() override;

  // Returns the account info of currently signed-in user, or absl::nullopt if
  // the user is not signed-in or the identity manager is not available.
  absl::optional<CoreAccountInfo> GetPrimaryAccountInfo() const;

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

  // Notifies test observers that an address or credit card could not be
  // imported from a form.
  void MarkObserversInsufficientFormDataForImport();

  // Called to indicate |profile_or_credit_card| was used (to fill in a form).
  // Updates the database accordingly.
  virtual void RecordUseOf(
      absl::variant<const AutofillProfile*, const CreditCard*>
          profile_or_credit_card);

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
  virtual AutofillProfile* GetProfileByGUID(const std::string& guid) const;

  // Determines whether the logged in user (if any) is eligible to store
  // Autofill address profiles to their account.
  virtual bool IsEligibleForAddressAccountStorage() const;

  // Users based in unsupported countries and profiles with a country value set
  // to an unsupported country are not eligible for account storage. This
  // function determines if the `country_code` is eligible.
  bool IsCountryEligibleForAccountStorage(base::StringPiece country_code) const;

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
  virtual std::string AddIban(Iban iban);

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

  // Adds |credit_card| to the web database as a full server card.
  virtual void AddFullServerCreditCard(const CreditCard& credit_card);

  // Update a server card. Only the full number and masked/unmasked
  // status can be changed. Looks up the card by server ID.
  virtual void UpdateServerCreditCard(const CreditCard& credit_card);

  // Updates the use stats and billing address id for the server |credit_cards|.
  // Looks up the cards by server_id.
  virtual void UpdateServerCardsMetadata(
      const std::vector<CreditCard>& credit_cards);

  // Methods to add, update, remove, or clear server CVC in the web database.
  virtual void AddServerCvc(int64_t instrument_id, const std::u16string& cvc);
  virtual void UpdateServerCvc(int64_t instrument_id,
                               const std::u16string& cvc);
  void RemoveServerCvc(int64_t instrument_id);
  void ClearServerCvcs();

  // Resets the card for |guid| to the masked state.
  void ResetFullServerCard(const std::string& guid);

  // Resets all unmasked cards to the masked state.
  void ResetFullServerCards();

  // Deletes all server profiles and cards (both masked and unmasked).
  void ClearAllServerData();

  // Deletes all local profiles and cards.
  virtual void ClearAllLocalData();

  // Sets a server credit card for test.
  void AddServerCreditCardForTest(std::unique_ptr<CreditCard> credit_card);

  void AddIbanForTest(std::unique_ptr<Iban> iban) {
    local_ibans_.push_back(std::move(iban));
  }

  // Returns whether server credit cards are stored in account (i.e. ephemeral)
  // storage.
  bool IsUsingAccountStorageForServerDataForTest() const;

  // Adds the offer data to local cache for tests. This does not affect data in
  // the real database.
  void AddOfferDataForTest(std::unique_ptr<AutofillOfferData> offer_data);

  // TODO(1426498): rewrite tests that rely on this method to use Init instead.
  void SetSyncServiceForTest(syncer::SyncService* sync_service);

  // Returns the IBAN with the specified |guid|, or nullptr if there is no IBAN
  // with the specified |guid|.
  virtual Iban* GetIbanByGUID(const std::string& guid);

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

  // Gets the field types available in the stored address and credit card data.
  void GetNonEmptyTypes(ServerFieldTypeSet* non_empty_types) const;

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
  virtual std::vector<AutofillProfile*> GetProfiles(
      ProfileOrder order = ProfileOrder::kNone) const;
  // TODO(crbug.com/1487119): Change return type to
  // std::vector<const AutofillProfile*>
  virtual std::vector<AutofillProfile*> GetProfilesFromSource(
      AutofillProfile::Source profile_source,
      ProfileOrder order = ProfileOrder::kNone) const;
  // Returns just SERVER_PROFILES.
  // TODO(crbug.com/1348294): Server profiles are only accessed in tests and the
  // concept should be removed.
  virtual std::vector<AutofillProfile*> GetServerProfiles() const;
  // Returns just LOCAL_CARD cards.
  virtual std::vector<CreditCard*> GetLocalCreditCards() const;
  // Returns just server cards.
  virtual std::vector<CreditCard*> GetServerCreditCards() const;
  // Returns all credit cards, server and local.
  virtual std::vector<CreditCard*> GetCreditCards() const;

  // Returns local IBANs.
  virtual std::vector<Iban*> GetLocalIbans() const;
  // Returns server IBANs.
  virtual std::vector<const Iban*> GetServerIbans() const;

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
  const std::vector<CreditCard*> GetCreditCardsToSuggest() const;

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

#ifdef UNIT_TEST
  // Returns the country code that was provided from the variations service
  // during construction.
  const std::string& variations_country_code_for_testing() const {
    return variations_country_code_;
  }

  // Sets the country code from the variations service.
  void set_variations_country_code_for_testing(std::string country_code) {
    variations_country_code_ = country_code;
  }

#if BUILDFLAG(IS_IOS)
  // Returns the raw pointer to PersonalDataManagerCleaner used for testing
  // purposes.
  PersonalDataManagerCleaner* personal_data_manager_cleaner_for_testing()
      const {
    DCHECK(personal_data_manager_cleaner_);
    return personal_data_manager_cleaner_.get();
  }
#endif  // IOS
#endif  // UNIT_TEST

  // Returns our best guess for the country a user is likely to use when
  // inputting a new address. The value is calculated once and cached, so it
  // will only update when Chrome is restarted.
  virtual const std::string& GetDefaultCountryCodeForNewAddress() const;

  // Returns our best guess for the country a user is in, for experiment group
  // purposes. The value is calculated once and cached, so it will only update
  // when Chrome is restarted.
  virtual const std::string& GetCountryCodeForExperimentGroup() const;

  // Returns all virtual card usage data linked to the credit card.
  virtual std::vector<VirtualCardUsageData*> GetVirtualCardUsageData() const;

  // De-dupe credit card to suggest. Full server cards are preferred over their
  // local duplicates, and local cards are preferred over their masked server
  // card duplicate.
  static void DedupeCreditCardToSuggest(
      std::list<CreditCard*>* cards_to_suggest);

  // Check if `credit_card` has a duplicate card present in either Local or
  // Server card lists.
  bool IsCardPresentAsBothLocalAndServerCards(const CreditCard& credit_card);

  // Returns a pointer to the server card that has duplicate information of the
  // `local_card`. It is not guaranteed that a server card is found. If not,
  // nullptr is returned.
  const CreditCard* GetServerCardForLocalCard(
      const CreditCard* local_card) const;

  // Cancels any pending queries to the server web database.
  void CancelPendingServerQueries();

#if defined(UNIT_TEST)
  // Returns if there are any pending queries to the web database.
  bool HasPendingQueriesForTesting() { return HasPendingQueries(); }
#endif

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

  // Records the sync transport consent if the user is in sync transport mode.
  virtual void OnUserAcceptedUpstreamOffer();

  // Notifies observers that the waiting should be stopped.
  void NotifyPersonalDataObserver();

  // TODO(crbug.com/1337392): Revisit the function when card upload feedback is
  // to be added again. In the new proposal, we may not need to go through PDM.
  // Called when at least one (can be multiple) card was saved. |is_local_card|
  // indicates if the card is saved to local storage.
  void OnCreditCardSaved(bool is_local_card);

  // Returns true if either Profile or CreditCard Autofill is enabled.
  virtual bool IsAutofillEnabled() const;

  // Returns the value of the AutofillProfileEnabled pref.
  virtual bool IsAutofillProfileEnabled() const;

  // Returns the value of the AutofillCreditCardEnabled pref.
  virtual bool IsAutofillCreditCardEnabled() const;

  // Returns the value of the kAutofillHasSeenIban pref.
  bool IsAutofillHasSeenIbanPrefEnabled() const;

  // Sets the value of the kAutofillHasSeenIban pref to true.
  void SetAutofillHasSeenIban();

  // Returns whether sync's integration with payments is on.
  virtual bool IsAutofillWalletImportEnabled() const;

  // Sets |credit_cards_| to the contents of |credit_cards| and updates the web
  // database by adding, updating and removing credit cards.
  void SetCreditCards(std::vector<CreditCard>* credit_cards);

  // Returns true if a `kLocalOrSyncable` profile identified by its guid is
  // blocked for migration to a `kAccount` profile.
  bool IsProfileMigrationBlocked(const std::string& guid) const;

  // Adds a strike to block a profile identified by its `guid` for migrations.
  // Does nothing if the strike database is not available.
  void AddStrikeToBlockProfileMigration(const std::string& guid);

  // Adds enough strikes to the profile identified by `guid` to block migrations
  // for it.
  void AddMaxStrikesToBlockProfileMigration(const std::string& guid);

  // Removes potential strikes to block a profile identified by its `guid` for
  // migrations. Does nothing if the strike database is not available.
  void RemoveStrikesToBlockProfileMigration(const std::string& guid);

  // Returns true if the import of new profiles should be blocked on `url`.
  // Returns false if the strike database is not available, the `url` is not
  // valid or has no host.
  bool IsNewProfileImportBlockedForDomain(const GURL& url) const;

  // Add a strike for blocking the import of new profiles on `url`.
  // Does nothing if the strike database is not available, the `url` is not
  // valid or has no host.
  void AddStrikeToBlockNewProfileImportForDomain(const GURL& url);

  // Removes potential strikes for the import of new profiles from `url`.
  // Does nothing if the strike database is not available, the `url` is not
  // valid or has no host.
  void RemoveStrikesToBlockNewProfileImportForDomain(const GURL& url);

  // Returns true if a profile identified by its `guid` is blocked for updates.
  // Returns false if the database is not available.
  bool IsProfileUpdateBlocked(const std::string& guid) const;

  // Adds a strike to block a profile identified by its `guid` for updates.
  // Does nothing if the strike database is not available.
  void AddStrikeToBlockProfileUpdate(const std::string& guid);

  // Removes potential strikes to block a profile identified by its `guid` for
  // updates. Does nothing if the strike database is not available.
  void RemoveStrikesToBlockProfileUpdate(const std::string& guid);

  // Returns true if Sync-the-feature is enabled and
  // UserSelectableType::kAutofill is among the user's selected data types.
  // TODO(crbug.com/1462552): Remove this method once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  bool IsSyncFeatureEnabledForAutofill() const;

  // Returns true if the user's selectable `type` is enabled.
  bool IsUserSelectableTypeEnabled(syncer::UserSelectableType type) const;

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

  // Used to automatically import addresses without a prompt. Should only be
  // set to true in tests.
  void set_auto_accept_address_imports_for_testing(bool auto_accept) {
    auto_accept_address_imports_for_testing_ = auto_accept;
  }
  bool auto_accept_address_imports_for_testing() {
    return auto_accept_address_imports_for_testing_;
  }

 protected:
  // Only PersonalDataManagerFactory and certain tests can create instances of
  // PersonalDataManager.
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest, AddProfile_CrazyCharacters);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest, AddProfile_Invalid);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           AddCreditCard_CrazyCharacters);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest, AddCreditCard_Invalid);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest, GetCreditCardByServerId);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           AddAndGetCreditCardArtImage);
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      ConvertWalletAddressesAndUpdateWalletCards_NewProfile);
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      ConvertWalletAddressesAndUpdateWalletCards_MergedProfile);
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      ConvertWalletAddressesAndUpdateWalletCards_NewCrd_AddressAlreadyConverted);  // NOLINT
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      ConvertWalletAddressesAndUpdateWalletCards_AlreadyConverted);
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      ConvertWalletAddressesAndUpdateWalletCards_MultipleSimilarWalletAddresses);  // NOLINT
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           DoNotConvertWalletAddressesInEphemeralStorage);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           DeleteDisusedCreditCards_DoNothingWhenDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetProfilesToSuggest_ProfileAutofillDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetProfilesToSuggest_NoProfilesLoadedIfDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetProfilesToSuggest_NoProfilesAddedIfDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetCreditCardsToSuggest_CreditCardAutofillDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetCreditCardsToSuggest_NoCardsLoadedIfDisabled);
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      GetCreditCardsToSuggest_NoCreditCardsAddedIfDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest, LogStoredCreditCardMetrics);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerCleanerTest,
                           UpdateCardsBillingAddressReference);

  friend class autofill::AutofillInteractiveTest;
  friend class autofill::PersonalDataManagerCleaner;
  friend class autofill::PersonalDataManagerFactory;
  friend class AutofillMetricsTest;
  friend class FormDataImporterTestBase;
  friend class ::PaymentsSuggestionBottomSheetMediatorTest;
  friend class PersonalDataManagerTest;
  friend class PersonalDataManagerTestBase;
  friend class PersonalDataManagerHelper;
  friend class PersonalDataManagerMockTest;
  friend class VirtualCardEnrollmentManagerTest;
  friend class ::RemoveAutofillTester;
  friend std::default_delete<PersonalDataManager>;
  friend void autofill_helper::SetCreditCards(
      int,
      std::vector<autofill::CreditCard>*);
  friend void SetTestProfiles(Profile* base_profile,
                              std::vector<AutofillProfile>* profiles);

  // Used to get a pointer to the strike database for migrating existing
  // profiles. Note, the result can be a nullptr, for example, on incognito
  // mode.
  AutofillProfileMigrationStrikeDatabase* GetProfileMigrationStrikeDatabase();
  virtual const AutofillProfileMigrationStrikeDatabase*
  GetProfileMigrationStrikeDatabase() const;

  // Used to get a pointer to the strike database for importing new profiles.
  // Note, the result can be a nullptr, for example, on incognito
  // mode.
  AutofillProfileSaveStrikeDatabase* GetProfileSaveStrikeDatabase();
  virtual const AutofillProfileSaveStrikeDatabase*
  GetProfileSaveStrikeDatabase() const;

  // Used to get a pointer to the strike database for updating existing
  // profiles. Note, the result can be a nullptr, for example, on incognito
  // mode.
  AutofillProfileUpdateStrikeDatabase* GetProfileUpdateStrikeDatabase();
  virtual const AutofillProfileUpdateStrikeDatabase*
  GetProfileUpdateStrikeDatabase() const;

  // Loads the saved profiles from the web database.
  virtual void LoadProfiles();

  // Loads the saved credit cards from the web database.
  virtual void LoadCreditCards();

  // Loads the saved credit card cloud token data from the web database.
  virtual void LoadCreditCardCloudTokenData();

  // Loads the saved IBANs from the web database.
  virtual void LoadIbans();

  // Loads the payments customer data from the web database.
  virtual void LoadPaymentsCustomerData();

  // Loads the autofill offer data from the web database.
  virtual void LoadAutofillOffers();

  // Loads the virtual card usage data from the web database
  virtual void LoadVirtualCardUsageData();

  // Cancels a pending query to the local web database.  |handle| is a pointer
  // to the query handle.
  void CancelPendingLocalQuery(WebDataServiceBase::Handle* handle);

  // Cancels a pending query to the server web database.  |handle| is a pointer
  // to the query handle.
  void CancelPendingServerQuery(WebDataServiceBase::Handle* handle);

  // The first time this is called, logs a UMA metrics about the user's autofill
  // addresses, credit card, offer and IBAN. On subsequent calls, does nothing.
  void LogStoredDataMetrics() const;

  // Whether the server cards are enabled and should be suggested to the user.
  virtual bool ShouldSuggestServerCards() const;

  // Overrideable for testing.
  virtual std::string CountryCodeForCurrentTimezone() const;

  // Sets which PrefService to use and observe. |pref_service| is not owned by
  // this class and must outlive |this|.
  void SetPrefService(PrefService* pref_service);

  // Asks `image_fetcher_` to fetch images. Virtual for testing.
  virtual void FetchImagesForURLs(base::span<const GURL> updated_urls) const;

  // The PersonalDataManager supports two types of AutofillProfiles, stored in
  // `synced_local_profiles_` and `account_profiles_` and distinguished by their
  // source.
  // Several function need to read/write from the correct vector, depending
  // on the source of the profile they are dealing with. This helper function
  // returns the vector where profiles of the given `source` are stored.
  const std::vector<std::unique_ptr<AutofillProfile>>& GetProfileStorage(
      AutofillProfile::Source source) const;
  std::vector<std::unique_ptr<AutofillProfile>>& GetProfileStorage(
      AutofillProfile::Source source) {
    return const_cast<std::vector<std::unique_ptr<AutofillProfile>>&>(
        const_cast<const PersonalDataManager*>(this)->GetProfileStorage(
            source));
  }

  // Decides which database type to use for server and local cards.
  std::unique_ptr<PersonalDatabaseHelper> database_helper_;

  // True if personal data has been loaded from the web database.
  bool is_data_loaded_ = false;

  // The loaded profiles from the AutofillTable come from two sources:
  // - kLocalOrSyncable: Stored in `synced_local_profiles_`.
  // - kAccount: Stored in `account_profiles_`.
  std::vector<std::unique_ptr<AutofillProfile>> synced_local_profiles_;
  std::vector<std::unique_ptr<AutofillProfile>> account_profiles_;

  // Address profiles associated to the user's payment profile.
  std::vector<std::unique_ptr<AutofillProfile>> credit_card_billing_addresses_;

  // Stores the PaymentsCustomerData obtained from the database.
  std::unique_ptr<PaymentsCustomerData> payments_customer_data_;

  // Cached versions of the local and server credit cards.
  std::vector<std::unique_ptr<CreditCard>> local_credit_cards_;
  std::vector<std::unique_ptr<CreditCard>> server_credit_cards_;

  // Cached versions of the local and server IBANs.
  std::vector<std::unique_ptr<Iban>> local_ibans_;
  std::vector<std::unique_ptr<Iban>> server_ibans_;

  // Cached version of the CreditCardCloudTokenData obtained from the database.
  std::vector<std::unique_ptr<CreditCardCloudTokenData>>
      server_credit_card_cloud_token_data_;

  // Autofill offer data, including card-linked offers for the user's credit
  // cards as well as promo code offers.
  std::vector<std::unique_ptr<AutofillOfferData>> autofill_offer_data_;

  // The customized card art images for the URL.
  std::map<GURL, std::unique_ptr<gfx::Image>> credit_card_art_images_;

  // Virtual card usage data, which contains information regarding usages of a
  // virtual card related to a specific merchant website.
  std::vector<std::unique_ptr<VirtualCardUsageData>>
      autofill_virtual_card_usage_data_;

  // When the manager makes a request from WebDataServiceBase, the database
  // is queried on another sequence, we record the query handle until we
  // get called back.  We store handles for both profile and credit card queries
  // so they can be loaded at the same time.
  WebDataServiceBase::Handle pending_synced_local_profiles_query_ = 0;
  WebDataServiceBase::Handle pending_account_profiles_query_ = 0;
  WebDataServiceBase::Handle pending_creditcard_billing_addresses_query_ = 0;
  WebDataServiceBase::Handle pending_creditcards_query_ = 0;
  WebDataServiceBase::Handle pending_server_creditcards_query_ = 0;
  WebDataServiceBase::Handle pending_server_creditcard_cloud_token_data_query_ =
      0;
  WebDataServiceBase::Handle pending_ibans_query_ = 0;
  WebDataServiceBase::Handle pending_server_ibans_query_ = 0;
  WebDataServiceBase::Handle pending_customer_data_query_ = 0;
  WebDataServiceBase::Handle pending_offer_data_query_ = 0;
  WebDataServiceBase::Handle pending_virtual_card_usage_data_query_ = 0;

  // The observers.
  base::ObserverList<PersonalDataManagerObserver>::Unchecked observers_;

  // Used to populate AlternativeStateNameMap with the geographical state data
  // (including their abbreviations and localized names).
  std::unique_ptr<AlternativeStateNameMapUpdater>
      alternative_state_name_map_updater_;

 private:
  // Sets (or resets) the Sync service, which may not have started yet
  // but its preferences can already be queried. Can also be a nullptr
  // if it is disabled by CLI.
  void SetSyncService(syncer::SyncService* sync_service);

  // Saves |imported_credit_card| to the WebDB if it exists. Returns the guid of
  // the new or updated card, or the empty string if no card was saved.
  virtual std::string SaveImportedCreditCard(
      const CreditCard& imported_credit_card);

  // Finds the country code that occurs most frequently among all profiles.
  // Prefers verified profiles over unverified ones.
  std::string MostCommonCountryCodeFromProfiles() const;

  // Called when the value of prefs::kAutofillCreditCardEnabled or
  // prefs::kAutofillProfileEnabled changes.
  void EnableAutofillPrefChanged();

  // Converts the Wallet addresses to local autofill profiles. This should be
  // called after all the syncable data has been processed (local cards and
  // profiles, Wallet data and metadata). Also updates Wallet cards' billing
  // address id to point to the local profiles.
  void ConvertWalletAddressesAndUpdateWalletCards();

  // Converts the Wallet addresses into local profiles either by merging with an
  // existing |local_profiles| of by adding a new one. Populates the
  // |server_id_profiles_map| to be used when updating cards where the address
  // was already converted. Also populates the |guids_merge_map| to keep the
  // link between the Wallet address and the equivalent local profile (from
  // merge or creation).
  bool ConvertWalletAddressesToLocalProfiles(
      std::vector<AutofillProfile>* local_profiles,
      std::unordered_map<std::string, AutofillProfile*>* server_id_profiles_map,
      std::unordered_map<std::string, std::string>* guids_merge_map);

  // Goes through the Wallet cards to find cards where the billing address is a
  // Wallet address which was already converted in a previous pass. Looks for a
  // matching local profile and updates the |guids_merge_map| to make the card
  // refer to it.
  bool UpdateWalletCardsAlreadyConvertedBillingAddresses(
      const std::vector<AutofillProfile>& local_profiles,
      const std::unordered_map<std::string, AutofillProfile*>&
          server_id_profiles_map,
      std::unordered_map<std::string, std::string>* guids_merge_map) const;

  // Removes profile from web database according to |guid| and resets credit
  // card's billing address if that address is used by any credit cards.
  // The method does not refresh, this allows multiple removal with one
  // refreshing in the end.
  void RemoveAutofillProfileByGUIDAndBlankCreditCardReference(
      const std::string& guid);

  // Add/Update/Removes a profile in AutofillTable asynchronously. The changes
  // only surface in the PDM after the task on the DB sequence has finished.
  // TODO(crbug.cm/1420547): `enforced` should not be used. Remove it.
  void AddProfileToDB(const AutofillProfile& profile);
  void UpdateProfileInDB(const AutofillProfile& profile, bool enforced = false);
  void RemoveProfileFromDB(const std::string& guid);

  // Triggered when a profile is added/updated/removed on db.
  void OnAutofillProfileChanged(const AutofillProfileDeepChange& change);

  // Triggered when all the card art image fetches have been completed,
  // regardless of whether all of them succeeded.
  void OnCardArtImagesFetched(
      const std::vector<std::unique_ptr<CreditCardArtImage>>& art_images);

  // Look at the next profile change for profile with guid = |guid|, and handle
  // it.
  void HandleNextProfileChange(const std::string& guid);
  // returns true if there is any profile change that's still ongoing.
  bool ProfileChangesAreOngoing();
  // returns true if there is any ongoing change for profile with guid = |guid|
  // that's still ongoing.
  bool ProfileChangesAreOngoing(const std::string& guid);
  // Remove the change from the |ongoing_profile_changes_|, handle next task or
  // Refresh.
  void OnProfileChangeDone(const std::string& guid);

  // Returns if there are any pending queries to the web database.
  bool HasPendingQueries();

  // Returns the database that is used for storing local data.
  scoped_refptr<AutofillWebDataService> GetLocalDatabase();

  // Invoked when server credit card cache is refreshed.
  void OnServerCreditCardsRefreshed();

  // Checks whether any new card art url is synced. If so, attempt to fetch the
  // image based on the url.
  void ProcessCardArtUrlChanges();

  // Returns the number of server credit cards that have a valid credit card art
  // image.
  size_t GetServerCardWithArtImageCount() const;

  // Stores the |app_locale| supplied on construction.
  const std::string app_locale_;

  // Stores the country code that was provided from the variations service
  // during construction.
  std::string variations_country_code_;

  // If true, new addresses imports are automatically accepted without a prompt.
  // Only to be used for testing.
  bool auto_accept_address_imports_for_testing_ = false;

  // The default country code for new addresses.
  mutable std::string default_country_code_;

  // The determined country code for experiment group purposes. Uses
  // |variations_country_code_| if it exists but falls back to other methods if
  // necessary to ensure it always has a value.
  mutable std::string experiment_country_code_;

  // The PrefService that this instance uses. Must outlive this instance.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // The HistoryService to be observed by the personal data manager. Must
  // outlive this instance. This unowned pointer is retained so the PDM can
  // remove itself from the history service's observer list on shutdown.
  raw_ptr<history::HistoryService> history_service_ = nullptr;

  // Pref registrar for managing the change observers.
  PrefChangeRegistrar pref_registrar_;

  // PersonalDataManagerCleaner is used to apply various address and credit
  // card fixes/cleanups one time at browser startup or when the sync starts.
  // PersonalDataManagerCleaner is declared as a friend class.
  std::unique_ptr<PersonalDataManagerCleaner> personal_data_manager_cleaner_;

  // A timely ordered list of ongoing changes for each profile.
  std::unordered_map<std::string, std::deque<AutofillProfileDeepChange>>
      ongoing_profile_changes_;

  // The identity manager that this instance uses. Must outlive this instance.
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  // The sync service this instances uses. Must outlive this instance.
  raw_ptr<syncer::SyncService> sync_service_ = nullptr;

  // The image fetcher to fetch customized images for Autofill data.
  raw_ptr<AutofillImageFetcherBase> image_fetcher_ = nullptr;

  // Whether we have already logged the stored profile, credit card, IBAN, offer
  // and virtual card usage metrics this session.
  mutable bool has_logged_stored_data_metrics_ = false;

  // An observer to listen for changes to prefs::kAutofillCreditCardEnabled.
  std::unique_ptr<BooleanPrefMember> credit_card_enabled_pref_;

  // An observer to listen for changes to prefs::kAutofillProfileEnabled.
  std::unique_ptr<BooleanPrefMember> profile_enabled_pref_;

  // The database that is used to count guid-keyed strikes to suppress the
  // migration-prompt of new profiles.
  std::unique_ptr<AutofillProfileMigrationStrikeDatabase>
      profile_migration_strike_database_;

  // The database that is used to count domain-keyed strikes to suppress the
  // import of new profiles.
  std::unique_ptr<AutofillProfileSaveStrikeDatabase>
      profile_save_strike_database_;

  // The database that is used to count guid-keyed strikes to suppress updates
  // of existing profiles.
  std::unique_ptr<AutofillProfileUpdateStrikeDatabase>
      profile_update_strike_database_;

  // Whether sync should be considered on in a test.
  bool is_syncing_for_test_ = false;

  base::ScopedObservation<history::HistoryService, HistoryServiceObserver>
      history_service_observation_{this};

  base::WeakPtrFactory<PersonalDataManager> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_
