// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_DATA_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/span.h"
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
#include "components/webdata/common/web_data_service_consumer.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace autofill {

class AutofillImageFetcherBase;
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

  // TODO(b/322170538): Remove.
  scoped_refptr<AutofillWebDataService> GetLocalDatabase();
  scoped_refptr<AutofillWebDataService> GetServerDatabase();
  void SetUseAccountStorageForServerData(bool use_account_storage);
  bool IsUsingAccountStorageForServerData();

  // Cancels any pending queries to the server web database.
  void CancelPendingServerQueries();

 protected:
  // TODO(b/322170538): Remove dependency.
  friend class PersonalDataManager;
  friend class TestPersonalDataManager;

  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           AddAndGetCreditCardArtImage);

  // Loads the saved credit cards from the web database.
  void LoadCreditCards();

  // Loads the saved credit card cloud token data from the web database.
  void LoadCreditCardCloudTokenData();

  // Loads the saved IBANs from the web database.
  void LoadIbans();

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
  WebDataServiceBase::Handle pending_customer_data_query_ = 0;
  WebDataServiceBase::Handle pending_offer_data_query_ = 0;
  WebDataServiceBase::Handle pending_virtual_card_usage_data_query_ = 0;
  WebDataServiceBase::Handle pending_credit_card_benefit_query_ = 0;

  // TODO(b/322170538): Remove dependency.
  raw_ptr<PersonalDataManager> pdm_;

  // The image fetcher to fetch customized images for Autofill data.
  raw_ptr<AutofillImageFetcherBase> image_fetcher_ = nullptr;

  // The shared storage handler this instance uses.
  std::unique_ptr<AutofillSharedStorageHandler> shared_storage_handler_;

  base::WeakPtrFactory<PaymentsDataManager> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_DATA_MANAGER_H_
