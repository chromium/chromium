// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments_data_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_shared_storage_handler.h"
#include "components/autofill/core/browser/data_model/credit_card_art_image.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/metrics/payments/offers_metrics.h"
#include "components/autofill/core/browser/metrics/payments/wallet_usage_data_metrics.h"
#include "components/autofill/core/browser/payments/payments_data_cleaner.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/autofill_image_fetcher_base.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/sync/base/model_type.h"

namespace autofill {

namespace {

// Checks the order of preference of the `original_card` with the
// `duplicate_card` and returns whether to dedupe/erase the `duplicate_card`
// based on the order of preference. We assume that both the cards in params are
// duplicates of each other.
//
// This function returns true in the following situations:
// Case 1: `original_card` = RecordType::kLocalCard
//         `duplicate_card` = RecordType::kMaskedServerCard
//         `should_suggest_server_cards_for_deduped_cards` = false
//
// Case 2: `original_card` = RecordType::kFullServerCard
//         `duplicate_card` = RecordType::kLocalCard
//         `should_suggest_server_cards_for_deduped_cards` = irrelevant
//
// Case 3: `original_card` = RecordType::kMaskedServerCard
//         `duplicate_card` = RecordType::kLocalCard
//         `should_suggest_server_cards_for_deduped_cards` = true
bool ShouldDedupeDuplicateCard(CreditCard* original_card,
                               CreditCard* duplicate_card) {
  // FULL_SERVER_CARDs have the highest priority and should never be removed
  // from the suggestion list.
  if (duplicate_card->record_type() ==
      CreditCard::RecordType::kFullServerCard) {
    return false;
  }
  const bool should_suggest_server_cards_for_deduped_cards =
      base::FeatureList::IsEnabled(
          features::kAutofillSuggestServerCardInsteadOfLocalCard);

  // Delete duplicated MASKED_SERVER_CARD if the original_card is a LOCAL_CARD
  // and we are NOT suggesting MASKED_SERVER_CARD for duplicates.
  if (duplicate_card->record_type() ==
          CreditCard::RecordType::kMaskedServerCard &&
      original_card->record_type() == CreditCard::RecordType::kLocalCard &&
      !should_suggest_server_cards_for_deduped_cards) {
    return true;
  }
  // Delete duplicated LOCAL_CARD if the original_card is a FULL_SERVER_CARD
  // or we are suggesting MASKED_SERVER_CARD for duplicates.
  if (duplicate_card->record_type() == CreditCard::RecordType::kLocalCard &&
      (original_card->record_type() ==
           CreditCard::RecordType::kFullServerCard ||
       should_suggest_server_cards_for_deduped_cards)) {
    return true;
  }
  return false;
}

// Receives the loaded profiles from the web data service and stores them in
// |*dest|. The pending handle is the address of the pending handle
// corresponding to this request type. This function is used to save both server
// and local profiles and credit cards.
template <typename ValueType>
void ReceiveLoadedDbValues(WebDataServiceBase::Handle h,
                           WDTypedResult* result,
                           WebDataServiceBase::Handle* pending_handle,
                           std::vector<ValueType>* dest) {
  DCHECK_EQ(*pending_handle, h);
  *pending_handle = 0;

  *dest = std::move(
      static_cast<WDResult<std::vector<ValueType>>*>(result)->GetValue());
}

template <typename T>
typename std::vector<T>::const_iterator FindElementByGUID(
    const std::vector<T>& container,
    std::string_view guid) {
  return base::ranges::find(
      container, guid, [](const auto& element) { return element->guid(); });
}

}  // namespace

// Helper class to abstract the switching between account and profile storage
// for server cards away from the rest of PaymentsDataManager.
class PaymentsDatabaseHelper {
 public:
  PaymentsDatabaseHelper(PaymentsDataManager* payments_data_manager,
                         scoped_refptr<AutofillWebDataService> profile_database,
                         scoped_refptr<AutofillWebDataService> account_database)
      : profile_database_(profile_database),
        account_database_(account_database),
        payments_data_manager_(payments_data_manager) {
    if (!profile_database_) {
      // In some tests, there are no dbs.
      return;
    }

    // Start observing the profile database. Don't observe the account database
    // until we know that we should use it.
    profile_database_->AddObserver(payments_data_manager_);

    // If we don't have an account_database , we always use the profile database
    // for server data.
    if (!account_database_) {
      server_database_ = profile_database_;
    } else {
      // Wait for the call to SetUseAccountStorageForServerData to decide
      // which database to use for server data.
      server_database_ = nullptr;
    }
  }

  PaymentsDatabaseHelper(const PaymentsDatabaseHelper&) = delete;
  PaymentsDatabaseHelper& operator=(const PaymentsDatabaseHelper&) = delete;

  ~PaymentsDatabaseHelper() {
    if (profile_database_) {
      profile_database_->RemoveObserver(payments_data_manager_);
    }

    // If we have a different server database, also remove its observer.
    if (server_database_ && server_database_ != profile_database_) {
      server_database_->RemoveObserver(payments_data_manager_);
    }
  }

  // Returns the database that should be used for storing local data.
  scoped_refptr<AutofillWebDataService> GetLocalDatabase() {
    return profile_database_;
  }

  // Returns the database that should be used for storing server data.
  scoped_refptr<AutofillWebDataService> GetServerDatabase() {
    return server_database_;
  }

  // Whether we're currently using the ephemeral account storage for saving
  // server data.
  bool IsUsingAccountStorageForServerData() {
    return server_database_ != profile_database_;
  }

  // Set whether this should use the passed in account storage for server
  // addresses. If false, this will use the profile_storage.
  // It's an error to call this if no account storage was passed in at
  // construction time.
  void SetUseAccountStorageForServerData(
      bool use_account_storage_for_server_cards) {
    if (!profile_database_) {
      // In some tests, there are no dbs.
      return;
    }
    scoped_refptr<AutofillWebDataService> new_server_database =
        use_account_storage_for_server_cards ? account_database_
                                             : profile_database_;
    DCHECK(new_server_database != nullptr)
        << "SetUseAccountStorageForServerData("
        << use_account_storage_for_server_cards << "): storage not available.";

    if (new_server_database == server_database_) {
      // Nothing to do :)
      return;
    }

    if (server_database_ != nullptr) {
      if (server_database_ != profile_database_) {
        // Remove the previous observer if we had any.
        server_database_->RemoveObserver(payments_data_manager_);
      }
      payments_data_manager_->CancelPendingServerQueries();
    }
    server_database_ = new_server_database;
    // We don't need to add an observer if server_database_ is equal to
    // profile_database_, because we're already observing that.
    if (server_database_ != profile_database_) {
      server_database_->AddObserver(payments_data_manager_);
    }
    // Notify the manager that the database changed.
    payments_data_manager_->Refresh();
  }

 private:
  scoped_refptr<AutofillWebDataService> profile_database_;
  scoped_refptr<AutofillWebDataService> account_database_;

  // The database that should be used for server data. This will always be equal
  // to either profile_database_, or account_database_.
  scoped_refptr<AutofillWebDataService> server_database_;

  raw_ptr<PaymentsDataManager> payments_data_manager_;
};

PaymentsDataManager::PaymentsDataManager(
    scoped_refptr<AutofillWebDataService> profile_database,
    scoped_refptr<AutofillWebDataService> account_database,
    AutofillImageFetcherBase* image_fetcher,
    std::unique_ptr<AutofillSharedStorageHandler> shared_storage_handler,
    PersonalDataManager* pdm)
    : pdm_(pdm),
      image_fetcher_(image_fetcher),
      shared_storage_handler_(std::move(shared_storage_handler)) {
  database_helper_ = std::make_unique<PaymentsDatabaseHelper>(
      this, profile_database, account_database);
}

PaymentsDataManager::~PaymentsDataManager() {
  CancelPendingLocalQuery(&pending_creditcards_query_);
  CancelPendingServerQueries();
}

void PaymentsDataManager::OnAutofillChangedBySync(
    syncer::ModelType model_type) {
  if (model_type == syncer::AUTOFILL_WALLET_CREDENTIAL ||
      model_type == syncer::AUTOFILL_WALLET_DATA ||
      model_type == syncer::AUTOFILL_WALLET_METADATA ||
      model_type == syncer::AUTOFILL_WALLET_OFFER ||
      model_type == syncer::AUTOFILL_WALLET_USAGE) {
    Refresh();
  }
}

void PaymentsDataManager::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK(pending_creditcards_query_ || pending_server_creditcards_query_ ||
         pending_server_creditcard_cloud_token_data_query_ ||
         pending_local_ibans_query_ || pending_server_ibans_query_ ||
         pending_customer_data_query_ || pending_offer_data_query_ ||
         pending_virtual_card_usage_data_query_ ||
         pending_credit_card_benefit_query_);

  if (!result) {
    // Error from the web database.
    if (h == pending_creditcards_query_) {
      pending_creditcards_query_ = 0;
    } else if (h == pending_server_creditcards_query_) {
      pending_server_creditcards_query_ = 0;
    } else if (h == pending_server_creditcard_cloud_token_data_query_) {
      pending_server_creditcard_cloud_token_data_query_ = 0;
    } else if (h == pending_local_ibans_query_) {
      pending_local_ibans_query_ = 0;
    } else if (h == pending_server_ibans_query_) {
      pending_server_ibans_query_ = 0;
    } else if (h == pending_customer_data_query_) {
      pending_customer_data_query_ = 0;
    } else if (h == pending_offer_data_query_) {
      pending_offer_data_query_ = 0;
    } else if (h == pending_virtual_card_usage_data_query_) {
      pending_virtual_card_usage_data_query_ = 0;
    } else if (h == pending_credit_card_benefit_query_) {
      pending_credit_card_benefit_query_ = 0;
    }
  } else {
    switch (result->GetType()) {
      case AUTOFILL_CREDITCARDS_RESULT:
        if (h == pending_creditcards_query_) {
          ReceiveLoadedDbValues(h, result.get(), &pending_creditcards_query_,
                                &local_credit_cards_);
        } else {
          DCHECK_EQ(h, pending_server_creditcards_query_)
              << "received creditcards from invalid request.";
          ReceiveLoadedDbValues(h, result.get(),
                                &pending_server_creditcards_query_,
                                &server_credit_cards_);
          OnServerCreditCardsRefreshed();
        }
        break;
      case AUTOFILL_CLOUDTOKEN_RESULT:
        DCHECK_EQ(h, pending_server_creditcard_cloud_token_data_query_)
            << "received credit card cloud token data from invalid request.";
        ReceiveLoadedDbValues(
            h, result.get(), &pending_server_creditcard_cloud_token_data_query_,
            &server_credit_card_cloud_token_data_);
        break;
      case AUTOFILL_IBANS_RESULT:
        if (h == pending_local_ibans_query_) {
          ReceiveLoadedDbValues(h, result.get(), &pending_local_ibans_query_,
                                &local_ibans_);
        } else {
          DCHECK_EQ(h, pending_server_ibans_query_)
              << "received ibans from invalid request.";
          ReceiveLoadedDbValues(h, result.get(), &pending_server_ibans_query_,
                                &server_ibans_);
        }
        break;
      case AUTOFILL_CUSTOMERDATA_RESULT:
        DCHECK_EQ(h, pending_customer_data_query_)
            << "received customer data from invalid request.";
        pending_customer_data_query_ = 0;

        payments_customer_data_ =
            static_cast<WDResult<std::unique_ptr<PaymentsCustomerData>>*>(
                result.get())
                ->GetValue();
        break;
      case AUTOFILL_OFFER_DATA:
        DCHECK_EQ(h, pending_offer_data_query_)
            << "received autofill offer data from invalid request.";
        ReceiveLoadedDbValues(h, result.get(), &pending_offer_data_query_,
                              &autofill_offer_data_);
        break;
      case AUTOFILL_VIRTUAL_CARD_USAGE_DATA:
        DCHECK_EQ(h, pending_virtual_card_usage_data_query_)
            << "received autofill virtual card usage data from invalid "
               "request.";
        ReceiveLoadedDbValues(h, result.get(),
                              &pending_virtual_card_usage_data_query_,
                              &autofill_virtual_card_usage_data_);
        break;
      case CREDIT_CARD_BENEFIT_RESULT:
        DCHECK_EQ(h, pending_credit_card_benefit_query_)
            << "received credit card benefit from invalid request.";
        ReceiveLoadedDbValues(h, result.get(),
                              &pending_credit_card_benefit_query_,
                              &credit_card_benefits_);
        break;
      default:
        NOTREACHED();
    }
  }

  if (HasPendingPaymentQueries()) {
    return;
  }

  if (!database_helper_->GetServerDatabase()) {
    DLOG(WARNING) << "There are no pending queries but the server database "
                     "wasn't set yet, so some data might be missing. Maybe "
                     "SetSyncService() wasn't called yet.";
    return;
  }

  if (!is_payments_data_loaded_) {
    is_payments_data_loaded_ = true;
    LogStoredPaymentsDataMetrics();
    PaymentsDataCleaner(pdm_).CleanupPaymentsData();
  }

  pdm_->NotifyPersonalDataObserver();
}

void PaymentsDataManager::Refresh() {
  pdm_->LoadCreditCards();
  pdm_->LoadCreditCardCloudTokenData();
  pdm_->LoadIbans();
  pdm_->LoadPaymentsCustomerData();
  pdm_->LoadAutofillOffers();
  pdm_->LoadVirtualCardUsageData();
  pdm_->LoadCreditCardBenefits();
}

const Iban* PaymentsDataManager::GetIbanByGUID(const std::string& guid) const {
  auto iter = FindElementByGUID(local_ibans_, guid);
  return iter != local_ibans_.end() ? iter->get() : nullptr;
}

const Iban* PaymentsDataManager::GetIbanByInstrumentId(
    int64_t instrument_id) const {
  for (const Iban* iban : GetServerIbans()) {
    if (iban->instrument_id() == instrument_id) {
      return iban;
    }
  }
  return nullptr;
}

CreditCard* PaymentsDataManager::GetCreditCardByGUID(const std::string& guid) {
  const std::vector<CreditCard*>& credit_cards = GetCreditCards();
  auto iter = FindElementByGUID(credit_cards, guid);
  return iter != credit_cards.end() ? *iter : nullptr;
}

CreditCard* PaymentsDataManager::GetCreditCardByNumber(
    const std::string& number) {
  CreditCard numbered_card;
  numbered_card.SetNumber(base::ASCIIToUTF16(number));
  for (CreditCard* credit_card : GetCreditCards()) {
    DCHECK(credit_card);
    if (credit_card->MatchingCardDetails(numbered_card)) {
      return credit_card;
    }
  }
  return nullptr;
}

CreditCard* PaymentsDataManager::GetCreditCardByInstrumentId(
    int64_t instrument_id) {
  const std::vector<CreditCard*> credit_cards = GetCreditCards();
  for (CreditCard* credit_card : credit_cards) {
    if (credit_card->instrument_id() == instrument_id) {
      return credit_card;
    }
  }
  return nullptr;
}

CreditCard* PaymentsDataManager::GetCreditCardByServerId(
    const std::string& server_id) {
  const std::vector<CreditCard*> server_credit_cards = GetServerCreditCards();
  for (CreditCard* credit_card : server_credit_cards) {
    if (credit_card->server_id() == server_id) {
      return credit_card;
    }
  }
  return nullptr;
}

template <typename T>
std::optional<T> PaymentsDataManager::GetCreditCardBenefitByInstrumentId(
    CreditCardBenefitBase::LinkedCardInstrumentId instrument_id,
    base::FunctionRef<bool(T&)> filter) {
  if (!pdm_->IsAutofillWalletImportEnabled() ||
      !pdm_->IsAutofillPaymentMethodsEnabled()) {
    return std::nullopt;
  }
  base::Time now = AutofillClock::Now();
  for (CreditCardBenefit& benefit : credit_card_benefits_) {
    if (auto* b = absl::get_if<T>(&benefit);
        b && b->linked_card_instrument_id() == instrument_id &&
        b->start_time() <= now && now < b->expiry_time() && filter(*b)) {
      return *b;
    }
  }
  return std::nullopt;
}

std::optional<CreditCardFlatRateBenefit>
PaymentsDataManager::GetFlatRateBenefitByInstrumentId(
    const CreditCardBenefitBase::LinkedCardInstrumentId instrument_id) {
  return GetCreditCardBenefitByInstrumentId<CreditCardFlatRateBenefit>(
      instrument_id);
}

std::optional<CreditCardCategoryBenefit>
PaymentsDataManager::GetCategoryBenefitByInstrumentIdAndCategory(
    const CreditCardBenefitBase::LinkedCardInstrumentId instrument_id,
    const CreditCardCategoryBenefit::BenefitCategory benefit_category) {
  return GetCreditCardBenefitByInstrumentId<CreditCardCategoryBenefit>(
      instrument_id, [&benefit_category](CreditCardCategoryBenefit& b) {
        return b.benefit_category() == benefit_category;
      });
}

std::optional<CreditCardMerchantBenefit>
PaymentsDataManager::GetMerchantBenefitByInstrumentIdAndOrigin(
    const CreditCardBenefitBase::LinkedCardInstrumentId instrument_id,
    const url::Origin& merchant_origin) {
  return GetCreditCardBenefitByInstrumentId<CreditCardMerchantBenefit>(
      instrument_id, [&merchant_origin](CreditCardMerchantBenefit& b) {
        return b.merchant_domains().contains(merchant_origin);
      });
}

std::vector<CreditCard*> PaymentsDataManager::GetLocalCreditCards() const {
  std::vector<CreditCard*> result;
  result.reserve(local_credit_cards_.size());
  for (const auto& card : local_credit_cards_) {
    result.push_back(card.get());
  }
  return result;
}

std::vector<CreditCard*> PaymentsDataManager::GetServerCreditCards() const {
  std::vector<CreditCard*> result;
  result.reserve(server_credit_cards_.size());
  for (const auto& card : server_credit_cards_) {
    result.push_back(card.get());
  }
  return result;
}

std::vector<CreditCard*> PaymentsDataManager::GetCreditCards() const {
  std::vector<CreditCard*> result;
  result.reserve(local_credit_cards_.size() + server_credit_cards_.size());
  for (const auto& card : local_credit_cards_) {
    result.push_back(card.get());
  }
  if (pdm_->IsAutofillWalletImportEnabled()) {
    for (const auto& card : server_credit_cards_) {
      result.push_back(card.get());
    }
  }
  return result;
}

std::vector<const Iban*> PaymentsDataManager::GetLocalIbans() const {
  std::vector<const Iban*> result;
  result.reserve(local_ibans_.size());
  for (const auto& iban : local_ibans_) {
    result.push_back(iban.get());
  }
  return result;
}

std::vector<const Iban*> PaymentsDataManager::GetServerIbans() const {
  std::vector<const Iban*> result;
  result.reserve(server_ibans_.size());
  for (const std::unique_ptr<Iban>& iban : server_ibans_) {
    result.push_back(iban.get());
  }
  return result;
}

std::vector<const Iban*> PaymentsDataManager::GetIbans() const {
  std::vector<const Iban*> result;
  result.reserve(local_ibans_.size() + server_ibans_.size());
  if (pdm_->IsAutofillWalletImportEnabled()) {
    for (const std::unique_ptr<Iban>& iban : server_ibans_) {
      result.push_back(iban.get());
    }
  }

  for (const std::unique_ptr<Iban>& iban : local_ibans_) {
    result.push_back(iban.get());
  }
  return result;
}

std::vector<const Iban*> PaymentsDataManager::GetIbansToSuggest() const {
  std::vector<const Iban*> ibans_to_suggest =
      pdm_->ShouldSuggestServerPaymentMethods() ? GetIbans() : GetLocalIbans();
  // Remove any IBAN from the returned list if it's a local IBAN and its
  // prefix, suffix, and length matches any existing server IBAN.
  std::erase_if(ibans_to_suggest, [this](const Iban* iban) {
    return iban->record_type() == Iban::kLocalIban &&
           base::ranges::any_of(
               server_ibans_, [&](const std::unique_ptr<Iban>& server_iban) {
                 return server_iban->MatchesPrefixSuffixAndLength(*iban);
               });
  });

  return ibans_to_suggest;
}

PaymentsCustomerData* PaymentsDataManager::GetPaymentsCustomerData() const {
  return payments_customer_data_ ? payments_customer_data_.get() : nullptr;
}

std::vector<CreditCardCloudTokenData*>
PaymentsDataManager::GetCreditCardCloudTokenData() const {
  std::vector<CreditCardCloudTokenData*> result;
  result.reserve(server_credit_card_cloud_token_data_.size());
  for (const auto& data : server_credit_card_cloud_token_data_) {
    result.push_back(data.get());
  }
  return result;
}

std::vector<AutofillOfferData*> PaymentsDataManager::GetAutofillOffers() const {
  std::vector<AutofillOfferData*> result;
  result.reserve(autofill_offer_data_.size());
  for (const auto& data : autofill_offer_data_) {
    result.push_back(data.get());
  }
  return result;
}

std::vector<const AutofillOfferData*>
PaymentsDataManager::GetActiveAutofillPromoCodeOffersForOrigin(
    GURL origin) const {
  std::vector<const AutofillOfferData*> promo_code_offers_for_origin;
  base::ranges::for_each(
      autofill_offer_data_,
      [&](const std::unique_ptr<AutofillOfferData>& autofill_offer_data) {
        if (autofill_offer_data.get()->IsPromoCodeOffer() &&
            autofill_offer_data.get()->IsActiveAndEligibleForOrigin(origin)) {
          promo_code_offers_for_origin.push_back(autofill_offer_data.get());
        }
      });
  return promo_code_offers_for_origin;
}

GURL PaymentsDataManager::GetCardArtURL(const CreditCard& credit_card) const {
  if (credit_card.record_type() == CreditCard::RecordType::kMaskedServerCard) {
    return credit_card.card_art_url();
  }

  if (credit_card.record_type() == CreditCard::RecordType::kLocalCard) {
    const CreditCard* server_duplicate_card =
        pdm_->GetServerCardForLocalCard(&credit_card);
    if (server_duplicate_card) {
      return server_duplicate_card->card_art_url();
    }
  }

  return GURL();
}

gfx::Image* PaymentsDataManager::GetCreditCardArtImageForUrl(
    const GURL& card_art_url) const {
  if (!card_art_url.is_valid()) {
    return nullptr;
  }

  gfx::Image* cached_image = GetCachedCardArtImageForUrl(card_art_url);
  if (cached_image) {
    return cached_image;
  }

  FetchImagesForURLs(base::make_span(&card_art_url, 1u));
  return nullptr;
}

gfx::Image* PaymentsDataManager::GetCachedCardArtImageForUrl(
    const GURL& card_art_url) const {
  if (!card_art_url.is_valid()) {
    return nullptr;
  }

  auto images_iterator = credit_card_art_images_.find(card_art_url);

  // If the cache contains the image, return it.
  if (images_iterator != credit_card_art_images_.end()) {
    gfx::Image* image = images_iterator->second.get();
    if (!image->IsEmpty()) {
      return image;
    }
  }

  // The cache does not contain the image, return nullptr.
  return nullptr;
}

std::vector<VirtualCardUsageData*>
PaymentsDataManager::GetVirtualCardUsageData() const {
  std::vector<VirtualCardUsageData*> result;
  result.reserve(autofill_virtual_card_usage_data_.size());
  for (const auto& data : autofill_virtual_card_usage_data_) {
    result.push_back(data.get());
  }
  return result;
}

const std::vector<CreditCard*> PaymentsDataManager::GetCreditCardsToSuggest()
    const {
  std::vector<CreditCard*> credit_cards;
  if (pdm_->ShouldSuggestServerPaymentMethods()) {
    credit_cards = GetCreditCards();
  } else {
    credit_cards = GetLocalCreditCards();
  }

  std::list<CreditCard*> cards_to_dedupe(credit_cards.begin(),
                                         credit_cards.end());

  DedupeCreditCardToSuggest(&cards_to_dedupe);

  std::vector<CreditCard*> cards_to_suggest(
      std::make_move_iterator(std::begin(cards_to_dedupe)),
      std::make_move_iterator(std::end(cards_to_dedupe)));

  // Rank the cards by ranking score (see AutofillDataModel for details). All
  // expired cards should be suggested last, also by ranking score.
  base::Time comparison_time = AutofillClock::Now();
  if (cards_to_suggest.size() > 1) {
    std::sort(cards_to_suggest.begin(), cards_to_suggest.end(),
              [comparison_time](const CreditCard* a, const CreditCard* b) {
                const bool a_is_expired = a->IsExpired(comparison_time);
                if (a_is_expired != b->IsExpired(comparison_time)) {
                  return !a_is_expired;
                }

                return a->HasGreaterRankingThan(b, comparison_time);
              });
  }

  return cards_to_suggest;
}

// The priority ranking for deduping a duplicate card is:
// 1. RecordType::kFullServerCard
// 2. RecordType::kLocalCard
// 3. RecordType::kMaskedServerCard
// Note: 2 & 3 are swapped if experiment
// kAutofillSuggestServerCardInsteadOfLocalCard is enabled.
// static
void PaymentsDataManager::DedupeCreditCardToSuggest(
    std::list<CreditCard*>* cards_to_suggest) {
  for (auto outer_it = cards_to_suggest->begin();
       outer_it != cards_to_suggest->end(); ++outer_it) {
    for (auto inner_it = cards_to_suggest->begin();
         inner_it != cards_to_suggest->end();) {
      auto inner_it_copy = inner_it++;
      if (outer_it == inner_it_copy) {
        continue;
      }
      // Check if the cards are local or server duplicate of each other. If yes,
      // then check if we can dedupe/erase the duplicate card.
      if ((*inner_it_copy)->IsLocalOrServerDuplicateOf(**outer_it) &&
          ShouldDedupeDuplicateCard(*outer_it, *inner_it_copy)) {
        cards_to_suggest->erase(inner_it_copy);
      }
    }
  }
}

scoped_refptr<AutofillWebDataService> PaymentsDataManager::GetLocalDatabase() {
  return database_helper_->GetLocalDatabase();
}
scoped_refptr<AutofillWebDataService> PaymentsDataManager::GetServerDatabase() {
  return database_helper_->GetServerDatabase();
}
void PaymentsDataManager::SetUseAccountStorageForServerData(
    bool use_account_storage) {
  database_helper_->SetUseAccountStorageForServerData(use_account_storage);
}
bool PaymentsDataManager::IsUsingAccountStorageForServerData() {
  return database_helper_->IsUsingAccountStorageForServerData();
}

void PaymentsDataManager::CancelPendingServerQueries() {
  CancelPendingServerQuery(&pending_server_creditcards_query_);
  CancelPendingServerQuery(&pending_customer_data_query_);
  CancelPendingServerQuery(&pending_server_creditcard_cloud_token_data_query_);
  CancelPendingServerQuery(&pending_server_ibans_query_);
  CancelPendingServerQuery(&pending_offer_data_query_);
  CancelPendingServerQuery(&pending_virtual_card_usage_data_query_);
  CancelPendingServerQuery(&pending_credit_card_benefit_query_);
}

void PaymentsDataManager::LoadCreditCards() {
  if (!database_helper_->GetLocalDatabase()) {
    NOTREACHED();
    return;
  }

  CancelPendingLocalQuery(&pending_creditcards_query_);
  CancelPendingServerQuery(&pending_server_creditcards_query_);

  pending_creditcards_query_ =
      database_helper_->GetLocalDatabase()->GetCreditCards(this);
  if (database_helper_->GetServerDatabase()) {
    pending_server_creditcards_query_ =
        database_helper_->GetServerDatabase()->GetServerCreditCards(this);
  }
}

void PaymentsDataManager::LoadCreditCardCloudTokenData() {
  if (!database_helper_->GetServerDatabase()) {
    return;
  }

  CancelPendingServerQuery(&pending_server_creditcard_cloud_token_data_query_);

  pending_server_creditcard_cloud_token_data_query_ =
      database_helper_->GetServerDatabase()->GetCreditCardCloudTokenData(this);
}

void PaymentsDataManager::LoadIbans() {
  if (!database_helper_->GetLocalDatabase()) {
    NOTREACHED();
    return;
  }

  CancelPendingLocalQuery(&pending_local_ibans_query_);
  CancelPendingServerQuery(&pending_server_ibans_query_);

  pending_local_ibans_query_ =
      database_helper_->GetLocalDatabase()->GetLocalIbans(this);
  if (database_helper_->GetServerDatabase()) {
    pending_server_ibans_query_ =
        database_helper_->GetServerDatabase()->GetServerIbans(this);
  }
}

void PaymentsDataManager::LoadAutofillOffers() {
  if (!database_helper_->GetServerDatabase()) {
    return;
  }

  CancelPendingServerQuery(&pending_offer_data_query_);

  pending_offer_data_query_ =
      database_helper_->GetServerDatabase()->GetAutofillOffers(this);
}

void PaymentsDataManager::LoadVirtualCardUsageData() {
  if (!database_helper_->GetServerDatabase()) {
    return;
  }

  CancelPendingServerQuery(&pending_virtual_card_usage_data_query_);

  pending_virtual_card_usage_data_query_ =
      database_helper_->GetServerDatabase()->GetVirtualCardUsageData(this);
}

void PaymentsDataManager::LoadCreditCardBenefits() {
  if (!database_helper_->GetServerDatabase()) {
    return;
  }

  CancelPendingServerQuery(&pending_credit_card_benefit_query_);

  pending_credit_card_benefit_query_ =
      database_helper_->GetServerDatabase()->GetCreditCardBenefits(this);
}

void PaymentsDataManager::CancelPendingLocalQuery(
    WebDataServiceBase::Handle* handle) {
  if (*handle) {
    if (!database_helper_->GetLocalDatabase()) {
      NOTREACHED();
      return;
    }
    database_helper_->GetLocalDatabase()->CancelRequest(*handle);
  }
  *handle = 0;
}

void PaymentsDataManager::CancelPendingServerQuery(
    WebDataServiceBase::Handle* handle) {
  if (*handle) {
    if (!database_helper_->GetServerDatabase()) {
      NOTREACHED();
      return;
    }
    database_helper_->GetServerDatabase()->CancelRequest(*handle);
  }
  *handle = 0;
}

void PaymentsDataManager::LoadPaymentsCustomerData() {
  if (!database_helper_->GetServerDatabase()) {
    return;
  }

  CancelPendingServerQuery(&pending_customer_data_query_);

  pending_customer_data_query_ =
      database_helper_->GetServerDatabase()->GetPaymentsCustomerData(this);
}

void PaymentsDataManager::FetchImagesForURLs(
    base::span<const GURL> updated_urls) const {
  if (!image_fetcher_) {
    return;
  }

  image_fetcher_->FetchImagesForURLs(
      updated_urls, base::BindOnce(&PaymentsDataManager::OnCardArtImagesFetched,
                                   weak_factory_.GetMutableWeakPtr()));
}

void PaymentsDataManager::LogStoredPaymentsDataMetrics() const {
  AutofillMetrics::LogStoredCreditCardMetrics(
      local_credit_cards_, server_credit_cards_,
      GetServerCardWithArtImageCount(), kDisusedDataModelTimeDelta);
  autofill_metrics::LogStoredIbanMetrics(local_ibans_, server_ibans_,
                                         kDisusedDataModelTimeDelta);
  autofill_metrics::LogStoredOfferMetrics(autofill_offer_data_);
  autofill_metrics::LogStoredVirtualCardUsageCount(
      autofill_virtual_card_usage_data_.size());
}

bool PaymentsDataManager::HasPendingPaymentQueries() const {
  return pending_creditcards_query_ != 0 ||
         pending_server_creditcards_query_ != 0 ||
         pending_server_creditcard_cloud_token_data_query_ != 0 ||
         pending_customer_data_query_ != 0 || pending_offer_data_query_ != 0 ||
         pending_virtual_card_usage_data_query_ != 0 ||
         pending_credit_card_benefit_query_ != 0;
}

void PaymentsDataManager::OnCardArtImagesFetched(
    const std::vector<std::unique_ptr<CreditCardArtImage>>& art_images) {
  for (auto& art_image : art_images) {
    if (!art_image->card_art_image.IsEmpty()) {
      credit_card_art_images_[art_image->card_art_url] =
          std::make_unique<gfx::Image>(art_image->card_art_image);
    }
  }
}

void PaymentsDataManager::ProcessCardArtUrlChanges() {
  std::vector<GURL> updated_urls;
  for (auto& card : server_credit_cards_) {
    if (!card->card_art_url().is_valid()) {
      continue;
    }

    // Try to find the old entry with the same url.
    auto it = credit_card_art_images_.find(card->card_art_url());
    // No existing entry found.
    if (it == credit_card_art_images_.end()) {
      updated_urls.emplace_back(card->card_art_url());
    }
  }
  if (!updated_urls.empty()) {
    pdm_->FetchImagesForURLs(updated_urls);
  }
}

void PaymentsDataManager::OnServerCreditCardsRefreshed() {
  ProcessCardArtUrlChanges();
  if (shared_storage_handler_) {
    shared_storage_handler_->OnServerCardDataRefreshed(server_credit_cards_);
  }
}

size_t PaymentsDataManager::GetServerCardWithArtImageCount() const {
  return base::ranges::count_if(
      server_credit_cards_.begin(), server_credit_cards_.end(),
      [](const auto& card) { return card->card_art_url().is_valid(); });
}

}  // namespace autofill
