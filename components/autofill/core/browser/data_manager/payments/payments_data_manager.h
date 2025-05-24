// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_PAYMENTS_PAYMENTS_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_PAYMENTS_PAYMENTS_DATA_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/function_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/autofill/core/browser/autofill_shared_storage_handler.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/payments/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/payments/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/account_info_getter.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/ui/autofill_image_fetcher_base.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service_observer.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace syncer {
class SyncService;
}  // namespace syncer

namespace sync_pb {
class PaymentInstrument;
class PaymentInstrumentCreationOption;
}  // namespace sync_pb

namespace autofill {

class AutofillOptimizationGuide;
class BankAccount;
class BnplIssuer;
class Ewallet;
class PaymentsDatabaseHelper;

// Contains all payments-related logic of the `PersonalDataManager`. See comment
// above the `PersonalDataManager` first.
//
// Technical details on how modifications are implemented:
// `PaymentsDataManager` (PayDM) code simply posts a task to the DB sequence and
// triggers a `Refresh()` afterwards. Since `Refresh()` itself simply posts
// several read requests on the DB sequence, and because the DB sequence is a
// sequence, the `Refresh()` is guaranteed to read the latest data. This is
// unnecessarily inefficient, since any change causes the PayDM to reload all of
// its data.
class PaymentsDataManager : public AutofillWebDataServiceObserverOnUISequence,
                            public AccountInfoGetter,
                            public syncer::SyncServiceObserver,
                            public signin::IdentityManager::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Triggered after all pending read and write operations have finished.
    virtual void OnPaymentsDataChanged() = 0;
  };

  // `profile_database` is a profile-scoped database that will be used to save
  // local data. `account_database` is scoped to the currently signed-in
  // account, and is wiped on signout and browser exit. This can be a nullptr
  // if PaymentsDataManager should use `profile_database` for all data.
  // If passed in, the `account_database` is used by default for server data.
  PaymentsDataManager(
      scoped_refptr<AutofillWebDataService> profile_database,
      scoped_refptr<AutofillWebDataService> account_database,
      AutofillImageFetcherBase* image_fetcher,
      std::unique_ptr<AutofillSharedStorageHandler> shared_storage_handler,
      PrefService* pref_service,
      syncer::SyncService* sync_service,
      signin::IdentityManager* identity_manager,
      GeoIpCountryCode variations_country_code,
      std::string app_locale);

  PaymentsDataManager(const PaymentsDataManager&) = delete;
  PaymentsDataManager& operator=(const PaymentsDataManager&) = delete;
  ~PaymentsDataManager() override;

  // Only intended to be called during shutdown of the parent `KeyedService`.
  void Shutdown();

  void AddObserver(Observer* obs) { observers_.AddObserver(obs); }
  void RemoveObserver(Observer* obs) { observers_.RemoveObserver(obs); }

  // AutofillWebDataServiceObserverOnUISequence:
  void OnAutofillChangedBySync(syncer::DataType data_type) override;

  void OnWebDataServiceRequestDone(WebDataServiceBase::Handle h,
                                   std::unique_ptr<WDTypedResult> result);

  // AccountInfoGetter:
  CoreAccountInfo GetAccountInfoForPaymentsServer() const override;
  bool IsSyncFeatureEnabledForPaymentsServerMetrics() const override;

  // SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

  // signin::IdentityManager::Observer:
  void OnAccountsCookieDeletedByUserAction() override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  // Reloads all payments data from the database.
  void Refresh();

  // Add a server IBAN to the cached list of IBANs in PaymentsDataManager.
  void AddServerIbanForTest(std::unique_ptr<Iban> iban);

  // Returns the IBAN with the specified `guid`, or nullptr if there is no IBAN
  // with the specified `guid`.
  const Iban* GetIbanByGUID(const std::string& guid) const;

  // Returns the IBAN if any cached IBAN in `server_ibans_` has the same
  // `instrument_id` as the given `instrument_id`, otherwise returns nullptr.
  const Iban* GetIbanByInstrumentId(int64_t instrument_id) const;

  // Returns the credit card with the specified |guid|, or nullptr if there is
  // no credit card with the specified |guid|.
  virtual const CreditCard* GetCreditCardByGUID(const std::string& guid) const;

  // Returns the credit card with the specified `number`, or nullptr if there is
  // no credit card with the specified `number`.
  const CreditCard* GetCreditCardByNumber(const std::string& number) const;

  // Returns the credit card with the specified |instrument_id|, or nullptr if
  // there is no credit card with the specified |instrument_id|.
  const CreditCard* GetCreditCardByInstrumentId(int64_t instrument_id) const;

  // Returns the credit card with the given server id, or nullptr if there is no
  // match.
  const CreditCard* GetCreditCardByServerId(const std::string& server_id) const;

  // Return the first valid flat rate benefit linked with the card with the
  // specific `instrument_id`.
  std::optional<CreditCardFlatRateBenefit> GetFlatRateBenefitByInstrumentId(
      CreditCardBenefitBase::LinkedCardInstrumentId instrument_id) const;

  // Return the first valid category benefit for the specific
  // `benefit_category` and linked with the card with the specific
  // `instrument_id`.
  std::optional<CreditCardCategoryBenefit>
  GetCategoryBenefitByInstrumentIdAndCategory(
      CreditCardBenefitBase::LinkedCardInstrumentId instrument_id,
      CreditCardCategoryBenefit::BenefitCategory benefit_category) const;

  // Return the first valid merchant benefit for the specific
  // `merchant_origin` and linked with the card with the specific
  // `instrument_id`.
  std::optional<CreditCardMerchantBenefit>
  GetMerchantBenefitByInstrumentIdAndOrigin(
      CreditCardBenefitBase::LinkedCardInstrumentId instrument_id,
      const url::Origin& merchant_origin) const;

  // Returns an applicable benefit description string to display to the user
  // based on the combination of `credit_card` and `origin`. However, if
  // `credit_card.IsCardEligibleForBenefits()` == false, the benefit description
  // will still be returned but not displayed to users.
  std::u16string GetApplicableBenefitDescriptionForCardAndOrigin(
      const CreditCard& credit_card,
      const url::Origin& origin,
      const AutofillOptimizationGuide* optimization_guide) const;

  // Returns just LOCAL_CARD cards.
  virtual std::vector<const CreditCard*> GetLocalCreditCards() const;
  // Returns just server cards.
  virtual std::vector<const CreditCard*> GetServerCreditCards() const;
  // Returns all credit cards, server and local.
  virtual std::vector<const CreditCard*> GetCreditCards() const;

  // Returns local IBANs.
  virtual std::vector<const Iban*> GetLocalIbans() const;
  // Returns server IBANs.
  virtual std::vector<const Iban*> GetServerIbans() const;
  // Returns all IBANs, server and local.
  virtual std::vector<const Iban*> GetIbans() const;
  // Returns all IBANs, server and local. All local IBANs that share the same
  // prefix, suffix, and length as any existing server IBAN will be considered a
  // duplicate IBAN. These duplicate IBANs will not be returned in the list.
  // The returned IBANs are ranked by ranking score (see UsageHistoryInformation
  // for details).
  std::vector<Iban> GetOrderedIbansToSuggest() const;

  // Returns true if the user has at least 1 masked bank account.
  bool HasMaskedBankAccounts() const;
  // Returns the masked bank accounts that can be suggested to the user.
  // The returned span may be invalidated asynchronously.
  base::span<const BankAccount> GetMaskedBankAccounts() const;

  // Returns the linked BNPL issuers that can be shown to the user. If the "Save
  // and fill payment methods" toggle is off, this will return no BNPL issuers
  // automatically.
  // The returned span may be invalidated asynchronously.
  base::span<const BnplIssuer> GetLinkedBnplIssuers() const;

  // Returns true if the user has at least 1 eWallet account.
  bool HasEwalletAccounts() const;

  // Returns the eWallet accounts that can be suggested to the user.
  // The returned span may be invalidated asynchronously.
  base::span<const Ewallet> GetEwalletAccounts() const;

  // Returns the Payments customer data. Returns nullptr if no data is present.
  virtual PaymentsCustomerData* GetPaymentsCustomerData() const;

  // Returns the credit card cloud token data.
  virtual std::vector<CreditCardCloudTokenData*> GetCreditCardCloudTokenData()
      const;

  // Returns autofill offer data, including card-linked and promo code offers.
  std::vector<const AutofillOfferData*> GetAutofillOffers() const;

  // Returns autofill offer data, but only promo code offers that are not
  // expired and that are for the given |origin|.
  std::vector<const AutofillOfferData*>
  GetActiveAutofillPromoCodeOffersForOrigin(GURL origin) const;

  AutofillImageFetcherBase* GetImageFetcher() { return image_fetcher_; }

  // Return the URL for the card art image, if available.
  GURL GetCardArtURL(const CreditCard& credit_card) const;

  // Returns the customized credit card art image for the `card_art_url`. If no
  // image has been cached, an asynchronous request will be sent to fetch the
  // image and this function will return nullptr.
  const gfx::Image* GetCreditCardArtImageForUrl(const GURL& card_art_url) const;

  // Returns all virtual card usage data linked to the credit card.
  // The returned span may be invalidated asynchronously.
  base::span<const VirtualCardUsageData> GetVirtualCardUsageData() const;

  // Returns the unlinked buy-now-pay-later issuers. This is a list of BNPL
  // issuers that are available to be used but have NOT been linked to the
  // payments account by the user.
  // The returned span may be invalidated asynchronously.
  base::span<const BnplIssuer> GetUnlinkedBnplIssuers() const;

  // Returns all BNPL issuers, both linked and unlinked.
  std::vector<BnplIssuer> GetBnplIssuers() const;

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
  void DeleteAllLocalCreditCards();

  // Updates |credit_card| which already exists in the web database. This
  // can only be used on local credit cards.
  virtual void UpdateCreditCard(const CreditCard& credit_card);

  // Updates a local CVC in the web database.
  virtual void UpdateLocalCvc(const std::string& guid,
                              const std::u16string& cvc);

  // Updates the use stats and billing address id for the server |credit_cards|.
  // Looks up the cards by server_id.
  void UpdateServerCardsMetadata(const std::vector<CreditCard>& credit_cards);

  // Methods to add, update, remove, or clear server CVC in the web database.
  virtual void AddServerCvc(int64_t instrument_id, const std::u16string& cvc);
  virtual void UpdateServerCvc(int64_t instrument_id,
                               const std::u16string& cvc);
  void RemoveServerCvc(int64_t instrument_id);
  virtual void ClearServerCvcs();

  // Method to clear all local CVCs from the local web database.
  virtual void ClearLocalCvcs();

  // Method to clean up for crbug.com/411681430.
  virtual void CleanupForCrbug411681430();

  // Deletes all server cards (both masked and unmasked).
  void ClearAllServerDataForTesting();

  // Sets |credit_cards_| to the contents of |credit_cards| and updates the web
  // database by adding, updating and removing credit cards.
  void SetCreditCards(std::vector<CreditCard>* credit_cards);

  // Try to save a credit card locally. If the card already exists, do nothing
  // and return false. If the card is new, save it locally and return true.
  virtual bool SaveCardLocallyIfNew(const CreditCard& imported_credit_card);

  // Removes the credit card or IBAN identified by `guid`.
  virtual void RemoveByGUID(const std::string& guid);

  // Removes all local credit cards and CVCs modified on or after `delete_begin`
  // and strictly before `delete_end`. Used for browsing data deletion purposes.
  // TODO(crbug.com/310301981): Consider local IBANs?
  void RemoveLocalDataModifiedBetween(base::Time begin, base::Time end);

  // Called to indicate `credit_card` was used (to fill in a form).
  // Updates the database accordingly.
  virtual void RecordUseOfCard(const CreditCard& card);

  // Called to indicate `iban` was used (to fill in a form). Updates the
  // database accordingly.
  virtual void RecordUseOfIban(Iban& iban);

  // Returns the cached card art image for the |card_art_url| if it was synced
  // locally to the client. This function is called within
  // GetCreditCardArtImageForUrl(), but can also be called separately as an
  // optimization for situations where a separate fetch request after trying to
  // retrieve local card art images is not needed. If the card art image is not
  // present in the cache, this function will return a nullptr.
  virtual const gfx::Image* GetCachedCardArtImageForUrl(
      const GURL& card_art_url) const;

  // Checks if a specific card is eligible to see benefits based on its issuer
  // id.
  bool IsCardEligibleForBenefits(const CreditCard& card) const;

  // Checks if the user is in an experiment for seeing credit card benefits in
  // Autofill suggestions.
  bool IsCardBenefitsFeatureEnabled();

  // Returns the value of the PaymentsCardBenefits pref.
  // `IsCardBenefitsPrefEnabled() == false` indicates the user does not see card
  // benefits and will not have card benefit metrics logged.
  bool IsCardBenefitsPrefEnabled() const;

  // Returns the value of the AutofillPaymentMethodsEnabled pref.
  virtual bool IsAutofillPaymentMethodsEnabled() const;

  // Returns the value of the kAutofillHasSeenIban pref.
  bool IsAutofillHasSeenIbanPrefEnabled() const;

  // Sets the value of the kAutofillHasSeenIban pref to true.
  void SetAutofillHasSeenIban();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  // Returns the value of the kAutofillHasSeenBnpl pref.
  bool IsAutofillHasSeenBnplPrefEnabled() const;

  // Sets the value of the kAutofillHasSeenBnpl pref to true.
  void SetAutofillHasSeenBnpl();
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

  // Returns if the user has seen a BNPL suggestion before and if the BNPL
  // feature is enabled. Does not check for user's locale.
  bool ShouldShowBnplSettings() const;

  // Returns whether sync's integration with payments is on.
  virtual bool IsAutofillWalletImportEnabled() const;

  // Returns true if wallet sync is running in transport mode (meaning that
  // Sync-the-feature is disabled).
  virtual bool IsPaymentsWalletSyncTransportEnabled() const;

  // Returns whether credit card download is active (meaning that wallet sync is
  // running at least in transport mode).
  bool IsPaymentsDownloadActive() const;

  // Returns the current sync status for the purpose of metrics only (do not
  // guard actual logic behind this value).
  AutofillMetrics::PaymentsSigninState GetPaymentsSigninStateForMetrics() const;

  // Check if `credit_card` has a duplicate card present in either Local or
  // Server card lists.
  bool IsCardPresentAsBothLocalAndServerCards(const CreditCard& credit_card);

  // Returns a pointer to the server card that has duplicate information of the
  // `local_card`. It is not guaranteed that a server card is found. If not,
  // nullptr is returned.
  const CreditCard* GetServerCardForLocalCard(
      const CreditCard* local_card) const;

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

  // This function assumes |credit_card| contains the full PAN. Returns |true|
  // if the card number of |credit_card| is equal to any local card or any
  // unmasked server card known by the browser, or |TypeAndLastFourDigits| of
  // |credit_card| is equal to any masked server card known by the browser.
  bool IsKnownCard(const CreditCard& credit_card) const;

  // Check whether a card is a server card or has a duplicated server card.
  bool IsServerCard(const CreditCard* credit_card) const;

  // Records the sync transport consent if the user is in sync transport mode.
  virtual void OnUserAcceptedUpstreamOffer();

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

  // TODO(crbug.com/322170538): Remove.
  scoped_refptr<AutofillWebDataService> GetLocalDatabase();
  scoped_refptr<AutofillWebDataService> GetServerDatabase();
  bool IsUsingAccountStorageForServerDataForTest();

  // Cancels any pending queries to the server web database.
  void CancelPendingServerQueries();

  // Logs the fact that the server card link was clicked including information
  // about the current sync state.
  void LogServerCardLinkClicked() const;

  // Logs the fact that the server IBAN link was clicked including information
  // about the current sync state.
  void LogServerIbanLinkClicked() const;

  // Returns our best guess for the country a user is in, for experiment group
  // purposes. The value is calculated once and cached, so it will only update
  // when Chrome is restarted.
  const std::string& GetCountryCodeForExperimentGroup() const;

  const std::string& app_locale() const { return app_locale_; }

  // Returns if there are any pending queries to the web database.
  bool HasPendingPaymentQueries() const;

  bool is_payments_data_loaded() const { return is_payments_data_loaded_; }

  void SetSyncServiceForTest(syncer::SyncService* sync_service);
  void SetSyncingForTest(bool is_syncing_for_test) {
    is_syncing_for_test_ = is_syncing_for_test;
  }

  // Add a bank account to the cached list of bank accounts in
  // PaymentsDataManager.
  void AddMaskedBankAccountForTest(const BankAccount& bank_account);

  // Add an eWallet account to the cached list of eWallet accounts in
  // PaymentsDataManager.
  void AddEwalletForTest(const Ewallet& ewallet);

  // Sets a server credit card for test.
  //
  // TODO(crbug.com/330865438): This method currently sets `server_cards_`
  // directly which is not correct for the real PaymentsDataManager. It should
  // be moved to TestPaymentsDataManager, and unittests should switch to that.
  void AddServerCreditCardForTest(std::unique_ptr<CreditCard> credit_card);

  // Add the credit-card-linked benefit to local cache for tests. This does
  // not affect data in the real database.
  void AddCreditCardBenefitForTest(CreditCardBenefit benefit);

  // Returns the value of the FacilitatedPaymentsPix user pref.
  bool IsFacilitatedPaymentsPixUserPrefEnabled() const;

  // Returns the value of the FacilitatedPaymentsEwallet user pref.
  bool IsFacilitatedPaymentsEwalletUserPrefEnabled() const;

  // Whether server cards or IBANs are enabled and should be suggested to the
  // user.
  virtual bool ShouldSuggestServerPaymentMethods() const;

 protected:
  friend class PaymentsDataManagerTestApi;

  // Loads the saved credit cards from the web database.
  virtual void LoadCreditCards();

  // Loads the saved credit card cloud token data from the web database.
  virtual void LoadCreditCardCloudTokenData();

  // Loads the saved IBANs from the web database.
  virtual void LoadIbans();

  // Loads the masked bank accounts from the web database.
  void LoadMaskedBankAccounts();

  // Loads the generic payment instruments from the web database.
  void LoadPaymentInstruments();

  // Loads the payments customer data from the web database.
  void LoadPaymentsCustomerData();

  // Loads the autofill offer data from the web database.
  void LoadAutofillOffers();

  // Loads the virtual card usage data from the web database.
  void LoadVirtualCardUsageData();

  // Loads the credit card benefits from the web database.
  void LoadCreditCardBenefits();

  // Loads the payment instrument creation options from the web database.
  void LoadPaymentInstrumentCreationOptions();

  // Cancels a pending query to the local web database.  |handle| is a pointer
  // to the query handle.
  void CancelPendingLocalQuery(WebDataServiceBase::Handle* handle);

  // Cancels a pending query to the server web database.  |handle| is a pointer
  // to the query handle.
  void CancelPendingServerQuery(WebDataServiceBase::Handle* handle);

  // The first time this is called, logs a UMA metrics about the user's credit
  // card, offer and IBAN.
  void LogStoredPaymentsDataMetrics() const;

  void SetPrefService(PrefService* pref_service);

  void NotifyObservers();

  CreditCard* GetMutableCreditCardByGUID(const std::string& guid);

  // Stores the PaymentsCustomerData obtained from the database.
  std::unique_ptr<PaymentsCustomerData> payments_customer_data_;

  // Cached versions of the local and server credit cards.
  std::vector<std::unique_ptr<CreditCard>> local_credit_cards_;
  std::vector<std::unique_ptr<CreditCard>> server_credit_cards_;

  // Cached versions of the local and server IBANs.
  std::vector<std::unique_ptr<Iban>> local_ibans_;
  std::vector<std::unique_ptr<Iban>> server_ibans_;

  // Cached versions of the masked bank accounts.
  std::vector<BankAccount> masked_bank_accounts_;

  // Cached versions of the linked BNPL issuers.
  std::vector<BnplIssuer> linked_bnpl_issuers_;

  // Cached versions of the eWallet accounts.
  std::vector<Ewallet> ewallet_accounts_;

  // Cached versions of the unlinked buy-now-pay-later issuers.
  std::vector<BnplIssuer> unlinked_bnpl_issuers_;

  // Cached version of the CreditCardCloudTokenData obtained from the database.
  std::vector<std::unique_ptr<CreditCardCloudTokenData>>
      server_credit_card_cloud_token_data_;

  // Autofill offer data, including card-linked offers for the user's credit
  // cards as well as promo code offers.
  std::vector<std::unique_ptr<AutofillOfferData>> autofill_offer_data_;

  // Virtual card usage data, which contains information regarding usages of a
  // virtual card related to a specific merchant website.
  std::vector<VirtualCardUsageData> autofill_virtual_card_usage_data_;

  // Cached version of the credit card benefits obtained from the database.
  // Including credit-card-linked flat rate benefits, category benefits and
  // merchant benefits that are available for users' online purchases.
  std::vector<CreditCardBenefit> credit_card_benefits_;

  // When the manager makes a request from WebDataServiceBase, the database
  // is queried on another sequence, we record the query handle until we
  // get called back.
  WebDataServiceBase::Handle pending_creditcards_query_ = 0;
  WebDataServiceBase::Handle pending_server_creditcards_query_ = 0;
  WebDataServiceBase::Handle pending_server_creditcard_cloud_token_data_query_ =
      0;
  WebDataServiceBase::Handle pending_local_ibans_query_ = 0;
  WebDataServiceBase::Handle pending_server_ibans_query_ = 0;
  WebDataServiceBase::Handle pending_masked_bank_accounts_query_ = 0;
  WebDataServiceBase::Handle pending_payment_instruments_query_ = 0;
  WebDataServiceBase::Handle pending_customer_data_query_ = 0;
  WebDataServiceBase::Handle pending_offer_data_query_ = 0;
  WebDataServiceBase::Handle pending_virtual_card_usage_data_query_ = 0;
  WebDataServiceBase::Handle pending_credit_card_benefit_query_ = 0;
  WebDataServiceBase::Handle
      pending_payment_instrument_creation_options_query_ = 0;

  // True if personal data has been loaded from the web database.
  bool is_payments_data_loaded_ = false;

  // The image fetcher to fetch customized images for Autofill data.
  raw_ptr<AutofillImageFetcherBase> image_fetcher_ = nullptr;

 private:
  // Check if credit card benefits sync flag is enabled.
  bool IsCardBenefitsSyncEnabled() const;

  // Returns whether Autofill card benefit suggestion labels should be blocked.
  bool ShouldBlockCardBenefitSuggestionLabels(
      const CreditCard& credit_card,
      const url::Origin& origin,
      const AutofillOptimizationGuide* optimization_guide) const;

  // Returns the value of the AutofillBnplEnabled pref.
  virtual bool IsAutofillBnplPrefEnabled() const;

  // Checks whether any new card art url is synced. If so, attempt to fetch the
  // image based on the url.
  void ProcessCardArtUrlChanges();

  // Invoked when server credit card cache is refreshed.
  void OnServerCreditCardsRefreshed();

  // Returns the number of server credit cards that have a valid credit card art
  // image.
  size_t GetServerCardWithArtImageCount() const;

  template <typename T>
  std::optional<T> GetCreditCardBenefitByInstrumentId(
      CreditCardBenefitBase::LinkedCardInstrumentId instrument_id,
      base::FunctionRef<bool(const T&)> filter = [](const T&) {
        return true;
      }) const;

  // Whether MaskedBankAccounts are supported for the platform OS.
  bool AreBankAccountsSupported() const;

  // Whether eWallet accounts are supported for the platform OS.
  bool AreEwalletAccountsSupported() const;

  // Whether buy-now-pay-later issuers are supported for the platform OS.
  // Checks if the user's locale is supported for BNPL, and if the BNPL feature
  // is enabled.
  bool AreBnplIssuersSupported() const;

  // Whether generic payment instruments are supported.
  bool ArePaymentInstrumentsSupported() const;

  // Whether payment instrument creation options are supported.
  bool ArePaymentInstrumentCreationOptionsSupported() const;

  // Monitors the `kAutofillPaymentCardBenefits` preference for changes and
  // controls the clearing/loading of credit card benefits into
  // `credit_card_benefits_` accordingly.
  void OnAutofillPaymentsCardBenefitsPrefChange();

  // Clears all credit card benefits in `credit_card_benefits_`.
  void ClearAllCreditCardBenefits();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  // Monitors the `kAutofillBnplEnabled` preference for changes and controls the
  // clearing/loading of payment instruments accordingly. Will also log the
  // `Autofill.SettingsPage.BnplToggled` metric.
  void OnBnplEnabledPrefChange();
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

  // Saves |imported_credit_card| to the WebDB if it exists. Returns the guid of
  // the new or updated card, or the empty string if no card was saved.
  virtual std::string SaveImportedCreditCard(
      const CreditCard& imported_credit_card);

  // Invoked when the masked bank accounts cache is refreshed. This happens when
  // the masked bank accounts are loaded for the first time as well as for any
  // subsequent updates via ChromeSync invalidations.
  void OnMaskedBankAccountsRefreshed();

  // Invoked when the eWallet accounts cache is refreshed. This happens when the
  // eWallet accounts are loaded for the first time as well as for any
  // subsequent updates via ChromeSync invalidations.
  void OnPaymentInstrumentsRefreshed(
      const std::vector<sync_pb::PaymentInstrument>& payment_instruments);

  // If a payment instrument contains linked BNPL issuer details, caches the
  // relevant information in `linked_bnpl_issuers_`.
  void CacheIfLinkedBnplPaymentInstrument(
      sync_pb::PaymentInstrument& payment_instrument);

  // Checks whether a payment instrument contains eWallet details. If yes,
  // caches relevant information in `ewallet_accounts_`.
  void CacheIfEwalletPaymentInstrument(
      sync_pb::PaymentInstrument& payment_instrument);

  // Invoked when the payment instrument creation options cache is refreshed.
  // This happens when the payment instrument creation options are loaded for
  // the first time as well as for any subsequent updates via ChromeSync
  // invalidations.
  void OnPaymentInstrumentCreationOptionsRefreshed(
      const std::vector<sync_pb::PaymentInstrumentCreationOption>&
          payment_instrument_creation_options);

  // Checks whether a payment instrument creation option contains
  // buy-now-pay-later options. If it does, it caches relevant information in
  // `unlinked_bnpl_issuers_`.
  void CacheIfBnplPaymentInstrumentCreationOption(
      const sync_pb::PaymentInstrumentCreationOption&
          payment_instrument_creation_option);

  // Checks whether at least one eligible price range specifies `currency_code`
  // as the currency.
  bool HasEligibleCurrencyPriceRangeForBnplIssuer(
      const std::vector<BnplIssuer::EligiblePriceRange>& eligible_price_ranges,
      const std::string& currency_code) const;

  // Decides which database type to use for server and local cards.
  std::unique_ptr<PaymentsDatabaseHelper> database_helper_;

  // The shared storage handler this instance uses.
  std::unique_ptr<AutofillSharedStorageHandler> shared_storage_handler_;

  // The sync service this instance uses. Must outlive this instance.
  raw_ptr<syncer::SyncService> sync_service_ = nullptr;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observer_{this};

  // The identity manager that this instance uses. Must outlive this instance.
  const raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_observer_{this};

  // The determined country code for experiment group purposes. Uses
  // |variations_country_code_| if it exists but falls back to other methods if
  // necessary to ensure it always has a value.
  mutable std::string experiment_country_code_;

  const GeoIpCountryCode variations_country_code_;

  // Stores the |app_locale| supplied on construction.
  const std::string app_locale_;

  base::ObserverList<Observer> observers_;

  // The PrefService that this instance uses to read and write preferences.
  // Must outlive this instance.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // Pref registrar for managing the change observers.
  PrefChangeRegistrar pref_registrar_;

  // Whether sync should be considered on in a test.
  bool is_syncing_for_test_ = false;

  base::WeakPtrFactory<PaymentsDataManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_PAYMENTS_PAYMENTS_DATA_MANAGER_H_
