// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_DATA_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/function_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_shared_storage_handler.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace autofill {

class AutofillImageFetcherBase;
class AutofillOptimizationGuide;
class BankAccount;
struct CreditCardArtImage;
class PaymentsDatabaseHelper;
class PersonalDataManager;
class TestPersonalDataManager;

class PaymentsDataManager : public AutofillWebDataServiceObserverOnUISequence,
                            public WebDataServiceConsumer {
 public:
  PaymentsDataManager(
      scoped_refptr<AutofillWebDataService> profile_database,
      scoped_refptr<AutofillWebDataService> account_database,
      AutofillImageFetcherBase* image_fetcher,
      std::unique_ptr<AutofillSharedStorageHandler> shared_storage_handler,
      PrefService* pref_service,
      const std::string& app_locale,
      PersonalDataManager* pdm);

  PaymentsDataManager(const PaymentsDataManager&) = delete;
  PaymentsDataManager& operator=(const PaymentsDataManager&) = delete;
  ~PaymentsDataManager() override;

  // AutofillWebDataServiceObserverOnUISequence:
  void OnAutofillChangedBySync(syncer::ModelType model_type) override;

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

  // Reloads all payments data from the database.
  void Refresh();

  // Returns the IBAN with the specified `guid`, or nullptr if there is no IBAN
  // with the specified `guid`.
  const Iban* GetIbanByGUID(const std::string& guid) const;

  // Returns the IBAN if any cached IBAN in `server_ibans_` has the same
  // `instrument_id` as the given `instrument_id`, otherwise returns nullptr.
  const Iban* GetIbanByInstrumentId(int64_t instrument_id) const;

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

  // Returns the masked bank accounts that can be suggested to the user.
  std::vector<BankAccount> GetMaskedBankAccounts() const;

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

  // Returns all virtual card usage data linked to the credit card.
  virtual std::vector<VirtualCardUsageData*> GetVirtualCardUsageData() const;

  // Returns the credit cards to suggest to the user. Those have been deduped
  // and ordered by frecency with the expired cards put at the end of the
  // vector.
  std::vector<CreditCard*> GetCreditCardsToSuggest() const;

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

  // Sets |credit_cards_| to the contents of |credit_cards| and updates the web
  // database by adding, updating and removing credit cards.
  void SetCreditCards(std::vector<CreditCard>* credit_cards);

  // Removes the credit card or IBAN identified by `guid`.
  // Returns true if something was removed.
  virtual bool RemoveByGUID(const std::string& guid);

  // Called to indicate `credit_card` was used (to fill in a form).
  // Updates the database accordingly.
  virtual void RecordUseOfCard(const CreditCard* card);

  // Called to indicate `iban` was used (to fill in a form). Updates the
  // database accordingly.
  virtual void RecordUseOfIban(Iban& iban);

  // De-dupe credit card to suggest. Full server cards are preferred over their
  // local duplicates, and local cards are preferred over their masked server
  // card duplicate.
  // TODO(b/326408802): Move to suggestion generator?
  static void DedupeCreditCardToSuggest(
      std::list<CreditCard*>* cards_to_suggest);

  // Returns the cached card art image for the |card_art_url| if it was synced
  // locally to the client. This function is called within
  // GetCreditCardArtImageForUrl(), but can also be called separately as an
  // optimization for situations where a separate fetch request after trying to
  // retrieve local card art images is not needed. If the card art image is not
  // present in the cache, this function will return a nullptr.
  gfx::Image* GetCachedCardArtImageForUrl(const GURL& card_art_url) const;

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

  // TODO(b/322170538): Remove.
  scoped_refptr<AutofillWebDataService> GetLocalDatabase();
  scoped_refptr<AutofillWebDataService> GetServerDatabase();
  void SetUseAccountStorageForServerData(bool use_account_storage);
  bool IsUsingAccountStorageForServerData();

  // Cancels any pending queries to the server web database.
  void CancelPendingServerQueries();

 protected:
  friend class PaymentsDataManagerTestApi;
  // TODO(b/322170538): Remove dependency.
  friend class PersonalDataManager;
  friend class TestPaymentsDataManager;
  friend class TestPersonalDataManager;

  // Loads the saved credit cards from the web database.
  virtual void LoadCreditCards();

  // Loads the saved credit card cloud token data from the web database.
  virtual void LoadCreditCardCloudTokenData();

  // Loads the saved IBANs from the web database.
  virtual void LoadIbans();

  // Loads the masked bank accounts from the web database.
  void LoadMaskedBankAccounts();

  // Loads the payments customer data from the web database.
  void LoadPaymentsCustomerData();

  // Loads the autofill offer data from the web database.
  void LoadAutofillOffers();

  // Loads the virtual card usage data from the web database.
  void LoadVirtualCardUsageData();

  // Loads the credit card benefits from the web database.
  void LoadCreditCardBenefits();

  // Cancels a pending query to the local web database.  |handle| is a pointer
  // to the query handle.
  void CancelPendingLocalQuery(WebDataServiceBase::Handle* handle);

  // Cancels a pending query to the server web database.  |handle| is a pointer
  // to the query handle.
  void CancelPendingServerQuery(WebDataServiceBase::Handle* handle);

  // Asks `image_fetcher_` to fetch images.
  void FetchImagesForURLs(base::span<const GURL> updated_urls) const;

  // The first time this is called, logs a UMA metrics about the user's credit
  // card, offer and IBAN.
  void LogStoredPaymentsDataMetrics() const;

  void SetPrefService(PrefService* pref_service);

  // Stores the PaymentsCustomerData obtained from the database.
  std::unique_ptr<PaymentsCustomerData> payments_customer_data_;

  // Cached versions of the local and server credit cards.
  std::vector<std::unique_ptr<CreditCard>> local_credit_cards_;
  std::vector<std::unique_ptr<CreditCard>> server_credit_cards_;

  // Cached versions of the local and server IBANs.
  std::vector<std::unique_ptr<Iban>> local_ibans_;
  std::vector<std::unique_ptr<Iban>> server_ibans_;

  // Cached versions of the masked bank accounts.
  std::vector<std::unique_ptr<BankAccount>> masked_bank_accounts_;

  // Cached version of the CreditCardCloudTokenData obtained from the database.
  std::vector<std::unique_ptr<CreditCardCloudTokenData>>
      server_credit_card_cloud_token_data_;

  // Autofill offer data, including card-linked offers for the user's credit
  // cards as well as promo code offers.
  std::vector<std::unique_ptr<AutofillOfferData>> autofill_offer_data_;

  // Virtual card usage data, which contains information regarding usages of a
  // virtual card related to a specific merchant website.
  std::vector<std::unique_ptr<VirtualCardUsageData>>
      autofill_virtual_card_usage_data_;

  // The customized card art images for the URL.
  std::map<GURL, std::unique_ptr<gfx::Image>> credit_card_art_images_;

  // Cached version of the credit card benefits obtained from the database.
  // Including credit-card-linked flat rate benefits, category benefits and
  // merchant benefits that are available for users' online purchases.
  std::vector<CreditCardBenefit> credit_card_benefits_;

  // True if personal data has been loaded from the web database.
  bool is_payments_data_loaded_ = false;

  // TODO(b/322170538): Remove dependency.
  raw_ptr<PersonalDataManager> pdm_;

 private:
  // Returns if there are any pending queries to the web database.
  bool HasPendingPaymentQueries() const;

  // Triggered when all the card art image fetches have been completed,
  // regardless of whether all of them succeeded.
  void OnCardArtImagesFetched(
      const std::vector<std::unique_ptr<CreditCardArtImage>>& art_images);

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

  // Clear all credit card benefits when the `kAutofillPaymentCardBenefits`
  // preference is turned off.
  void OnAutofillPaymentsCardBenefitsPrefChange();

  // Clears all credit card benefits in `credit_card_benefits_`.
  void ClearAllCreditCardBenefits();

  // Decides which database type to use for server and local cards.
  std::unique_ptr<PaymentsDatabaseHelper> database_helper_;

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
  WebDataServiceBase::Handle pending_customer_data_query_ = 0;
  WebDataServiceBase::Handle pending_offer_data_query_ = 0;
  WebDataServiceBase::Handle pending_virtual_card_usage_data_query_ = 0;
  WebDataServiceBase::Handle pending_credit_card_benefit_query_ = 0;

  // The image fetcher to fetch customized images for Autofill data.
  raw_ptr<AutofillImageFetcherBase> image_fetcher_ = nullptr;

  // The shared storage handler this instance uses.
  std::unique_ptr<AutofillSharedStorageHandler> shared_storage_handler_;

  // Stores the |app_locale| supplied on construction.
  const std::string app_locale_;

  // The PrefService that this instance uses to read and write preferences.
  // Must outlive this instance.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // Pref registrar for managing the change observers.
  PrefChangeRegistrar pref_registrar_;

  base::WeakPtrFactory<PaymentsDataManager> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_DATA_MANAGER_H_
