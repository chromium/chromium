// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_

#include <list>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/account_info_getter.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_profile_validator.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/suggestion.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/signin/core/browser/account_info.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"

class Browser;
class PrefService;
class RemoveAutofillTester;

namespace autofill {
class AutofillInteractiveTest;
class PersonalDataManagerObserver;
class PersonalDataManagerFactory;
class PersonalDatabaseHelper;
}  // namespace autofill

namespace autofill_helper {
void SetProfiles(int, std::vector<autofill::AutofillProfile>*);
void SetCreditCards(int, std::vector<autofill::CreditCard>*);
}  // namespace autofill_helper

namespace identity {
class IdentityManager;
}

namespace syncer {
class SyncService;
}  // namespace syncer

namespace autofill {

// Handles loading and saving Autofill profile information to the web database.
// This class also stores the profiles loaded from the database for use during
// Autofill.
class PersonalDataManager : public KeyedService,
                            public WebDataServiceConsumer,
                            public AutofillWebDataServiceObserverOnUISequence,
                            public history::HistoryServiceObserver,
                            public syncer::SyncServiceObserver,
                            public AccountInfoGetter {
 public:
  explicit PersonalDataManager(const std::string& app_locale);
  ~PersonalDataManager() override;

  // Kicks off asynchronous loading of profiles and credit cards.
  // |profile_database| is a profile-scoped database that will be used to save
  // local cards. |account_database| is scoped to the currently signed-in
  // account, and is wiped on signout and browser exit. This can be a nullptr
  // if personal_data_manager should use |profile_database| for all data.
  // If passed in, the |account_database| is used by default for server cards.
  // |pref_service| must outlive this instance. |is_off_the_record| informs this
  // instance whether the user is currently operating in an off-the-record
  // context.
  void Init(scoped_refptr<AutofillWebDataService> profile_database,
            scoped_refptr<AutofillWebDataService> account_database,
            PrefService* pref_service,
            identity::IdentityManager* identity_manager,
            AutofillProfileValidator* client_profile_validator,
            history::HistoryService* history_service,
            bool is_off_the_record);

  // KeyedService:
  void Shutdown() override;

  // Called once the sync service is known to be instantiated. Note that it may
  // not be started, but it's preferences can be queried.
  virtual void OnSyncServiceInitialized(syncer::SyncService* sync_service);

  // history::HistoryServiceObserver
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

  // AutofillWebDataServiceObserverOnUISequence:
  void AutofillMultipleChanged() override;
  void SyncStarted(syncer::ModelType model_type) override;

  // SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // AccountInfoGetter:
  AccountInfo GetAccountInfoForPaymentsServer() const override;
  bool IsSyncFeatureEnabled() const override;

  // Adds a listener to be notified of PersonalDataManager events.
  virtual void AddObserver(PersonalDataManagerObserver* observer);

  // Removes |observer| as an observer of this PersonalDataManager.
  virtual void RemoveObserver(PersonalDataManagerObserver* observer);

  // Notifies test observers that an address or credit card could not be
  // imported from a form.
  void MarkObserversInsufficientFormDataForImport();

  // Called to indicate |data_model| was used (to fill in a form). Updates
  // the database accordingly. Can invalidate |data_model|.
  virtual void RecordUseOf(const AutofillDataModel& data_model);

  // Saves |imported_profile| to the WebDB if it exists. Returns the guid of
  // the new or updated profile, or the empty string if no profile was saved.
  virtual std::string SaveImportedProfile(
      const AutofillProfile& imported_profile);

  // Called when the user accepts the prompt to save the credit card locally.
  // Records some metrics and attempts to save the imported card. Returns the
  // guid of the new or updated card, or the empty string if no card was saved.
  std::string OnAcceptedLocalCreditCardSave(
      const CreditCard& imported_credit_card);

  // Adds |profile| to the web database.
  virtual void AddProfile(const AutofillProfile& profile);

  // Updates |profile| which already exists in the web database.
  virtual void UpdateProfile(const AutofillProfile& profile);

  // Removes the profile or credit card represented by |guid|.
  virtual void RemoveByGUID(const std::string& guid);

  // Returns the profile with the specified |guid|, or nullptr if there is no
  // profile with the specified |guid|. Both web and auxiliary profiles may
  // be returned.
  virtual AutofillProfile* GetProfileByGUID(const std::string& guid);

  // Returns the profile with the specified |guid| from the given |profiles|, or
  // nullptr if there is no profile with the specified |guid|.
  static AutofillProfile* GetProfileFromProfilesByGUID(
      const std::string& guid,
      const std::vector<AutofillProfile*>& profiles);

  // Adds |credit_card| to the web database as a local card.
  virtual void AddCreditCard(const CreditCard& credit_card);

  // Delete list of provided credit cards.
  virtual void DeleteLocalCreditCards(const std::vector<CreditCard>& cards);

  // Updates |credit_card| which already exists in the web database. This
  // can only be used on local credit cards.
  virtual void UpdateCreditCard(const CreditCard& credit_card);

  // Adds |credit_card| to the web database as a full server card.
  virtual void AddFullServerCreditCard(const CreditCard& credit_card);

  // Update a server card. Only the full number and masked/unmasked
  // status can be changed. Looks up the card by server ID.
  virtual void UpdateServerCreditCard(const CreditCard& credit_card);

  // Updates the use stats and billing address id for the server |credit_card|.
  // Looks up the card by server_id.
  virtual void UpdateServerCardMetadata(const CreditCard& credit_card);

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

  // Returns whether server credit cards are stored in account (i.e. ephemeral)
  // storage.
  bool IsUsingAccountStorageForServerCardsForTest() const;

  // Sets which SyncService to use and observe in a test. |sync_service| is not
  // owned by this class and must outlive |this|.
  void SetSyncServiceForTest(syncer::SyncService* sync_service);

  // Returns the credit card with the specified |guid|, or nullptr if there is
  // no credit card with the specified |guid|.
  virtual CreditCard* GetCreditCardByGUID(const std::string& guid);

  // Returns the credit card with the specified |number|, or nullptr if there is
  // no credit card with the specified |number|.
  virtual CreditCard* GetCreditCardByNumber(const std::string& number);

  // Gets the field types availabe in the stored address and credit card data.
  void GetNonEmptyTypes(ServerFieldTypeSet* non_empty_types);

  // Returns whether the personal data has been loaded from the web database.
  virtual bool IsDataLoaded() const;

  // This PersonalDataManager owns these profiles and credit cards.  Their
  // lifetime is until the web database is updated with new profile and credit
  // card information, respectively.
  virtual std::vector<AutofillProfile*> GetProfiles() const;
  // Returns just SERVER_PROFILES.
  virtual std::vector<AutofillProfile*> GetServerProfiles() const;
  // Returns just LOCAL_CARD cards.
  virtual std::vector<CreditCard*> GetLocalCreditCards() const;
  // Returns just server cards.
  virtual std::vector<CreditCard*> GetServerCreditCards() const;
  // Returns all credit cards, server and local.
  virtual std::vector<CreditCard*> GetCreditCards() const;

  // Returns the Payments customer data. Returns nullptr if no data is present.
  virtual PaymentsCustomerData* GetPaymentsCustomerData() const;

  // Updates the validity states of |profiles| according to server validity map.
  void UpdateProfilesValidityMapsIfNeeded(
      std::vector<AutofillProfile*>& profiles);

  // Updates the validity states of |profiles| according to client side
  // validation API: |client_profile_validator_|.
  void UpdateClientValidityStates(std::vector<AutofillProfile*>& profiles);

  // Returns the profiles to suggest to the user, ordered by frecency.
  std::vector<AutofillProfile*> GetProfilesToSuggest() const;

  // Remove profiles that haven't been used after |min_last_used| from
  // |profiles|. The relative ordering of |profiles| is maintained.
  static void RemoveProfilesNotUsedSinceTimestamp(
      base::Time min_last_used,
      std::vector<AutofillProfile*>* profiles);

  // Remove profiles that whose |type| field is flagged as invalid, if Chrome
  // is configured to not make suggestions based on invalid data.
  static void MaybeRemoveInvalidSuggestions(
      const AutofillType& type,
      std::vector<AutofillProfile*>* profiles);

  // Loads profiles that can suggest data for |type|. |field_contents| is the
  // part the user has already typed. |field_is_autofilled| is true if the field
  // has already been autofilled. |other_field_types| represents the rest of
  // form.
  std::vector<Suggestion> GetProfileSuggestions(
      const AutofillType& type,
      const base::string16& field_contents,
      bool field_is_autofilled,
      const std::vector<ServerFieldType>& other_field_types);

  // Tries to delete disused addresses once per major version if the
  // feature is enabled.
  bool DeleteDisusedAddresses();

  // Returns the credit cards to suggest to the user. Those have been deduped
  // and ordered by frecency with the expired cards put at the end of the
  // vector. If |include_server_cards| is false, server side cards should not
  // be included.
  const std::vector<CreditCard*> GetCreditCardsToSuggest(
      bool include_server_cards) const;

  // Remove credit cards that are expired at |comparison_time| and not used
  // since |min_last_used| from |cards|. The relative ordering of |cards| is
  // maintained.
  static void RemoveExpiredCreditCardsNotUsedSinceTimestamp(
      base::Time comparison_time,
      base::Time min_last_used,
      std::vector<CreditCard*>* cards);

  // Gets credit cards that can suggest data for |type|. See
  // GetProfileSuggestions for argument descriptions. The variant in each
  // GUID pair should be ignored. If |include_server_cards| is false, server
  // side cards should not be included.
  std::vector<Suggestion> GetCreditCardSuggestions(
      const AutofillType& type,
      const base::string16& field_contents,
      bool include_server_cards);

  // Tries to delete disused credit cards once per major version if the
  // feature is enabled.
  bool DeleteDisusedCreditCards();

  // Re-loads profiles and credit cards from the WebDatabase asynchronously.
  // In the general case, this is a no-op and will re-create the same
  // in-memory model as existed prior to the call.  If any change occurred to
  // profiles in the WebDatabase directly, as is the case if the browser sync
  // engine processed a change from the cloud, we will learn of these as a
  // result of this call.
  //
  // Also see SetProfile for more details.
  virtual void Refresh();

  const std::string& app_locale() const { return app_locale_; }

  // Merges |new_profile| into one of the |existing_profiles| if possible;
  // otherwise appends |new_profile| to the end of that list. Fills
  // |merged_profiles| with the result. Returns the |guid| of the new or updated
  // profile.
  std::string MergeProfile(
      const AutofillProfile& new_profile,
      std::vector<std::unique_ptr<AutofillProfile>>* existing_profiles,
      const std::string& app_locale,
      std::vector<AutofillProfile>* merged_profiles);

  // Returns true if |country_code| is a country that the user is likely to
  // be associated with the user. More concretely, it checks if there are any
  // addresses with this country or if the user's system timezone is in the
  // given country.
  virtual bool IsCountryOfInterest(const std::string& country_code) const;

  // Returns our best guess for the country a user is likely to use when
  // inputting a new address. The value is calculated once and cached, so it
  // will only update when Chrome is restarted.
  virtual const std::string& GetDefaultCountryCodeForNewAddress() const;

  // De-dupe credit card to suggest. Full server cards are preferred over their
  // local duplicates, and local cards are preferred over their masked server
  // card duplicate.
  static void DedupeCreditCardToSuggest(
      std::list<CreditCard*>* cards_to_suggest);

  // Notifies test observers that personal data has changed.
  void NotifyPersonalDataChangedForTest() { NotifyPersonalDataChanged(); }

  // Cancels any pending queries to the server web database.
  void CancelPendingServerQueries();

  // This function assumes |credit_card| contains the full PAN. Returns |true|
  // if the card number of |credit_card| is equal to any local card or any
  // unmasked server card known by the browser, or |TypeAndLastFourDigits| of
  // |credit_card| is equal to any masked server card known by the browser.
  bool IsKnownCard(const CreditCard& credit_card);

  // Check whether a card is a server card or has a duplicated server card.
  bool IsServerCard(const CreditCard* credit_card) const;

  // Sets the value that can skip the checks to see if we are syncing in a test.
  void SetSyncingForTest(bool is_syncing_for_test) {
    is_syncing_for_test_ = is_syncing_for_test;
  }

 protected:
  // Only PersonalDataManagerFactory and certain tests can create instances of
  // PersonalDataManager.
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest, AddProfile_CrazyCharacters);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest, AddProfile_Invalid);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           AddCreditCard_CrazyCharacters);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest, AddCreditCard_Invalid);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, FirstMiddleLast);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AutofillIsEnabledAtStartup);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           DedupeProfiles_ProfilesToDelete);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           DedupeProfiles_GuidsMergeMap);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           UpdateCardsBillingAddressReference);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_CardsBillingAddressIdUpdated);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_MergedProfileValues);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_VerifiedProfileFirst);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_VerifiedProfileLast);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_MultipleVerifiedProfiles);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_FeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_NopIfZeroProfiles);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_NopIfOneProfile);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_OncePerVersion);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_MultipleDedupes);
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
                           DeleteDisusedCreditCards_OncePerVersion);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           DeleteDisusedCreditCards_DoNothingWhenDisabled);
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      DeleteDisusedCreditCards_OnlyDeleteExpiredDisusedLocalCards);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetProfileSuggestions_ProfileAutofillDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetProfileSuggestions_NoProfilesLoadedIfDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetProfileSuggestions_NoProfilesAddedIfDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetCreditCardSuggestions_CreditCardAutofillDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetCreditCardSuggestions_NoCardsLoadedIfDisabled);
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      GetCreditCardSuggestions_NoCreditCardsAddedIfDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ClearProfileNonSettingsOrigins);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ClearCreditCardNonSettingsOrigins);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           MoveJapanCityToStreetAddress);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest, RequestProfileValidity);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetProfileSuggestions_InvalidDataBasedOnServer);

  friend class autofill::AutofillInteractiveTest;
  friend class autofill::PersonalDataManagerFactory;
  friend class AutofillMetricsTest;
  friend class FormDataImporterTest;
  friend class PersonalDataManagerTest;
  friend class PersonalDataManagerTestBase;
  friend class SaveImportedProfileTest;
  friend class ProfileSyncServiceAutofillTest;
  friend class ::RemoveAutofillTester;
  friend std::default_delete<PersonalDataManager>;
  friend void autofill_helper::SetProfiles(
      int,
      std::vector<autofill::AutofillProfile>*);
  friend void autofill_helper::SetCreditCards(
      int,
      std::vector<autofill::CreditCard>*);
  friend void SetTestProfiles(Browser* browser,
                              std::vector<AutofillProfile>* profiles);

  // Sets |web_profiles_| to the contents of |profiles| and updates the web
  // database by adding, updating and removing profiles.
  // The relationship between this and Refresh is subtle.
  // A call to |SetProfiles| could include out-of-date data that may conflict
  // if we didn't refresh-to-latest before an Autofill window was opened for
  // editing. |SetProfiles| is implemented to make a "best effort" to apply the
  // changes, but in extremely rare edge cases it is possible not all of the
  // updates in |profiles| make it to the DB.  This is why SetProfiles will
  // invoke Refresh after finishing, to ensure we get into a
  // consistent state.  See Refresh for details.
  virtual void SetProfiles(std::vector<AutofillProfile>* profiles);

  // Sets |credit_cards_| to the contents of |credit_cards| and updates the web
  // database by adding, updating and removing credit cards.
  void SetCreditCards(std::vector<CreditCard>* credit_cards);

  // Loads the saved profiles from the web database.
  virtual void LoadProfiles();

  // Loads the saved credit cards from the web database.
  virtual void LoadCreditCards();

  // Loads the payments customer data from the web database.
  virtual void LoadPaymentsCustomerData();

  // Cancels a pending query to the local web database.  |handle| is a pointer
  // to the query handle.
  void CancelPendingLocalQuery(WebDataServiceBase::Handle* handle);

  // Cancels a pending query to the server web database.  |handle| is a pointer
  // to the query handle.
  void CancelPendingServerQuery(WebDataServiceBase::Handle* handle);

  // Notifies observers that personal data has changed.
  void NotifyPersonalDataChanged();

  // The first time this is called, logs a UMA metrics about the user's autofill
  // addresses. On subsequent calls, does nothing.
  void LogStoredProfileMetrics() const;

  // The first time this is called, logs an UMA metric about the user's autofill
  // credit cardss. On subsequent calls, does nothing.
  void LogStoredCreditCardMetrics() const;

  // Returns the value of the AutofillEnabled pref.
  virtual bool IsAutofillEnabled() const;

  // Returns the value of the AutofillEnabled pref.
  virtual bool IsAutofillProfileEnabled() const;

  // Returns the value of the AutofillCreditCardEnabled pref.
  virtual bool IsAutofillCreditCardEnabled() const;

  // Returns the value of the AutofillWalletImportEnabled pref.
  virtual bool IsAutofillWalletImportEnabled() const;

  // Whether the server cards are enabled and should be suggested to the user.
  virtual bool ShouldSuggestServerCards() const;

  // Overrideable for testing.
  virtual std::string CountryCodeForCurrentTimezone() const;

  // Sets which PrefService to use and observe. |pref_service| is not owned by
  // this class and must outlive |this|.
  void SetPrefService(PrefService* pref_service);

  // Clears the value of the origin field of the autofill profiles or cards that
  // were not created from the settings page.
  void ClearProfileNonSettingsOrigins();
  void ClearCreditCardNonSettingsOrigins();

  // Appends the value of the city field of a JP address to its street address
  // field, separated by a newline, and clears the city field.
  // TODO(rouslan): Remove this migration in or after October 2019. See bug:
  // https://crbug.com/871301
  void MoveJapanCityToStreetAddress();

  // Get the profiles fields validity map by |guid|.
  const ProfileValidityMap& GetProfileValidityByGUID(const std::string& guid);

  // Decides which database type to use for server and local cards.
  std::unique_ptr<PersonalDatabaseHelper> database_helper_;

  // True if personal data has been loaded from the web database.
  bool is_data_loaded_ = false;

  // The loaded web profiles. These are constructed from entries on web pages
  // and from manually editing in the settings.
  std::vector<std::unique_ptr<AutofillProfile>> web_profiles_;

  // Profiles read from the user's account stored on the server.
  mutable std::vector<std::unique_ptr<AutofillProfile>> server_profiles_;

  // Stores the PaymentsCustomerData obtained from the database.
  std::unique_ptr<PaymentsCustomerData> payments_customer_data_;

  // Storage for web profiles.  Contents are weak references.  Lifetime managed
  // by |web_profiles_|.
  mutable std::vector<AutofillProfile*> profiles_;

  // Cached versions of the local and server credit cards.
  std::vector<std::unique_ptr<CreditCard>> local_credit_cards_;
  std::vector<std::unique_ptr<CreditCard>> server_credit_cards_;

  // When the manager makes a request from WebDataServiceBase, the database
  // is queried on another sequence, we record the query handle until we
  // get called back.  We store handles for both profile and credit card queries
  // so they can be loaded at the same time.
  WebDataServiceBase::Handle pending_profiles_query_ = 0;
  WebDataServiceBase::Handle pending_server_profiles_query_ = 0;
  WebDataServiceBase::Handle pending_creditcards_query_ = 0;
  WebDataServiceBase::Handle pending_server_creditcards_query_ = 0;
  WebDataServiceBase::Handle pending_customer_data_query_ = 0;

  // The observers.
  base::ObserverList<PersonalDataManagerObserver>::Unchecked observers_;

  // |profile_valditiies_need_update| whenever the profile validities are out of
  bool profile_validities_need_update = true;

 private:
  // Saves |imported_credit_card| to the WebDB if it exists. Returns the guid of
  // the new or updated card, or the empty string if no card was saved.
  virtual std::string SaveImportedCreditCard(
      const CreditCard& imported_credit_card);

  // Finds the country code that occurs most frequently among all profiles.
  // Prefers verified profiles over unverified ones.
  std::string MostCommonCountryCodeFromProfiles() const;

  // Called when the value of prefs::kAutofillWalletImportEnabled changes.
  void EnableWalletIntegrationPrefChanged();

  // Called when the value of prefs::kAutofillCreditCardEnabled or
  // prefs::kAutofillProfileEnabled changes.
  void EnableAutofillPrefChanged();

  // Returns credit card suggestions based on the |cards_to_suggest| and the
  // |type| and |field_contents| of the credit card field.
  std::vector<Suggestion> GetSuggestionsForCards(
      const AutofillType& type,
      const base::string16& field_contents,
      const std::vector<CreditCard*>& cards_to_suggest) const;

  // Returns true if the given credit card can be deleted in a major version
  // upgrade. The card will need to be local and disused, to be deletable.
  bool IsCreditCardDeletable(CreditCard* card);

  // Runs the routine that removes the orphan rows in the autofill tables if
  // it's never been done.
  void RemoveOrphanAutofillTableRows();

  // Applies the deduping routine once per major version if the feature is
  // enabled. Calls DedupeProfiles with the content of |web_profiles_| as a
  // parameter. Removes the profiles to delete from the database and updates the
  // others. Also updates the credit cards' billing address references. Returns
  // true if the routine was run.
  bool ApplyDedupingRoutine();

  // Goes through all the |existing_profiles| and merges all similar unverified
  // profiles together. Also discards unverified profiles that are similar to a
  // verified profile. All the profiles except the results of the merges will be
  // added to |profile_guids_to_delete|. This routine should be run once per
  // major version. Records all the merges into the |guids_merge_map|.
  //
  // This method should only be called by ApplyDedupingRoutine. It is split for
  // testing purposes.
  void DedupeProfiles(
      std::vector<std::unique_ptr<AutofillProfile>>* existing_profiles,
      std::unordered_set<AutofillProfile*>* profile_guids_to_delete,
      std::unordered_map<std::string, std::string>* guids_merge_map);

  // Updates the credit cards' billing address reference based on the merges
  // that happened during the dedupe, as defined in |guids_merge_map|. Also
  // updates the cards entries in the database.
  void UpdateCardsBillingAddressReference(
      const std::unordered_map<std::string, std::string>& guids_merge_map);

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
  // refert to it.
  bool UpdateWalletCardsAlreadyConvertedBillingAddresses(
      std::vector<AutofillProfile>* local_profiles,
      std::unordered_map<std::string, AutofillProfile*>* server_id_profiles_map,
      std::unordered_map<std::string, std::string>* guids_merge_map);

  // Tries to merge the |server_address| into the |existing_profiles| if
  // possible. Adds it to the list if no match is found. The existing profiles
  // should be sorted by decreasing frecency outside of this method, since this
  // will be called multiple times in a row. Returns the guid of the new or
  // updated profile.
  std::string MergeServerAddressesIntoProfiles(
      const AutofillProfile& server_address,
      std::vector<AutofillProfile>* existing_profiles);

  // Removes profile from web database according to |guid| and resets credit
  // card's billing address if that address is used by any credit cards.
  // The method does not refresh, this allows multiple removal with one
  // refreshing in the end.
  void RemoveAutofillProfileByGUIDAndBlankCreditCardReferecne(
      const std::string& guid);

  // Returns true if an address can be deleted in a major version upgrade.
  // An address is deletable if it is unverified, and not used by a valid
  // credit card as billing address, and not used for a long time(13 months).
  bool IsAddressDeletable(
      AutofillProfile* profile,
      const std::unordered_set<std::string>& used_billing_address_guids);

  // If the AutofillCreateDataForTest feature is enabled, this helper creates
  // autofill address data that would otherwise be difficult to create
  // manually using the UI.
  void MaybeCreateTestAddresses();

  // If the AutofillCreateDataForTest feature is enabled, this helper creates
  // autofill credit card data that would otherwise be difficult to create
  // manually using the UI.
  void MaybeCreateTestCreditCards();

  // Applies various fixes and cleanups on autofill addresses.
  void ApplyAddressFixesAndCleanups();

  // Applies various fixes and cleanups on autofill credit cards.
  void ApplyCardFixesAndCleanups();

  // Resets |synced_profile_validity_|.
  void ResetProfileValidity() {
    synced_profile_validity_.reset();
    profile_validities_need_update = true;
  };

  // Called when the |profile| is validated by the AutofillProfileValidator.
  void OnValidated(AutofillProfile* profile);

  const std::string app_locale_;

  // The default country code for new addresses.
  mutable std::string default_country_code_;

  // The PrefService that this instance uses. Must outlive this instance.
  PrefService* pref_service_ = nullptr;

  // The HistoryService to observed by the personal data manager. Must
  // outlive this instance. This unowned pointer is retained so the PDM can
  // remove itself from the history service's observer list on shutdown.
  history::HistoryService* history_service_ = nullptr;

  // Pref registrar for managing the change observers.
  PrefChangeRegistrar pref_registrar_;

  // Profiles validity read from the prefs. They are kept updated by
  // observing changes in pref_services. We need to set the
  // |profile_validities_need_update| whenever this is changed.
  std::unique_ptr<UserProfileValidityMap> synced_profile_validity_;

  AutofillProfileValidator* client_profile_validator_;

  // The identity manager that this instance uses. Must outlive this instance.
  identity::IdentityManager* identity_manager_ = nullptr;

  // The sync service this instances uses. Must outlive this instance.
  syncer::SyncService* sync_service_ = nullptr;

  // Whether the user is currently operating in an off-the-record context.
  // Default value is false.
  bool is_off_the_record_ = false;

  // Whether we have already logged the stored profile metrics this session.
  mutable bool has_logged_stored_profile_metrics_ = false;

  // Whether we have already logged the stored credit card metrics this session.
  mutable bool has_logged_stored_credit_card_metrics_ = false;

  // An observer to listen for changes to prefs::kAutofillCreditCardEnabled.
  std::unique_ptr<BooleanPrefMember> credit_card_enabled_pref_;

  // An observer to listen for changes to prefs::kAutofillProfileEnabled.
  std::unique_ptr<BooleanPrefMember> profile_enabled_pref_;

  // An observer to listen for changes to prefs::kAutofillWalletImportEnabled.
  std::unique_ptr<BooleanPrefMember> wallet_enabled_pref_;

  // True if autofill profile cleanup needs to be performed.
  bool is_autofill_profile_cleanup_pending_ = false;

  // Whether new information was received from the sync server.
  bool has_synced_new_data_ = false;

  // True if test data has been created this session.
  bool has_created_test_addresses_ = false;
  bool has_created_test_credit_cards_ = false;

  // Whether sync should be considered on in a test.
  bool is_syncing_for_test_ = false;

  DISALLOW_COPY_AND_ASSIGN(PersonalDataManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_
