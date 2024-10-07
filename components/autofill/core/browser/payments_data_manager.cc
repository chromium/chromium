// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments_data_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/timezone.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_optimization_guide.h"
#include "components/autofill/core/browser/autofill_shared_storage_handler.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/data_model/credit_card_art_image.h"
#include "components/autofill/core/browser/data_model/ewallet.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/metrics/autofill_settings_metrics.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/metrics/payments/cvc_storage_metrics.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/metrics/payments/offers_metrics.h"
#include "components/autofill/core/browser/metrics/payments/wallet_usage_data_metrics.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/payments_data_cleaner.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/autofill_image_fetcher_base.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/credit_card_number_validation.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/service/sync_user_settings.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace autofill {

using autofill_metrics::LogMandatoryReauthOfferOptInDecision;
using autofill_metrics::MandatoryReauthOfferOptInDecision;

namespace {

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
const T& Deref(T* x) {
  return *x;
}

template <typename T, base::RawPtrTraits Traits = base::RawPtrTraits::kEmpty>
const T& Deref(const raw_ptr<T, Traits>& x) {
  return *x;
}

template <typename T>
const T& Deref(const std::unique_ptr<T>& x) {
  return *x;
}

template <typename T>
const T& Deref(const T& x) {
  return x;
}

template <typename T>
typename std::vector<T>::const_iterator FindElementByGUID(
    const std::vector<T>& container,
    std::string_view guid) {
  return base::ranges::find(container, guid, [](const auto& element) {
    return Deref(element).guid();
  });
}

template <typename C>
bool FindByGUID(const C& container, std::string_view guid) {
  return FindElementByGUID(container, guid) != container.end();
}

template <typename C, typename T>
bool FindByContents(const C& container, const T& needle) {
  return std::ranges::any_of(container, [&needle](const auto& element) {
    return element->Compare(needle) == 0;
  });
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
    PrefService* pref_service,
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager,
    GeoIpCountryCode variations_country_code,
    const std::string& app_locale)
    : image_fetcher_(image_fetcher),
      shared_storage_handler_(std::move(shared_storage_handler)),
      sync_service_(sync_service),
      identity_manager_(identity_manager),
      variations_country_code_(std::move(variations_country_code)),
      app_locale_(app_locale) {
  database_helper_ = std::make_unique<PaymentsDatabaseHelper>(
      this, profile_database, account_database);
  SetPrefService(pref_service);
  if (pref_service_) {
    autofill_metrics::LogIsAutofillPaymentMethodsEnabledAtStartup(
        IsAutofillPaymentMethodsEnabled());
    if (IsAutofillPaymentMethodsEnabled()) {
      autofill_metrics::LogIsAutofillPaymentsCvcStorageEnabledAtStartup(
          IsPaymentCvcStorageEnabled());
      if (IsCardBenefitsFeatureEnabled()) {
        autofill_metrics::LogIsCreditCardBenefitsEnabledAtStartup(
            prefs::IsPaymentCardBenefitsEnabled(pref_service_));
      }
    } else {
      autofill_metrics::LogAutofillPaymentMethodsDisabledReasonAtStartup(
          *pref_service_);
    }
  }
  if (sync_service_) {
    sync_observer_.Observe(sync_service_);
  }
  OnStateChanged(sync_service_);
  if (identity_manager_) {
    identity_observer_.Observe(identity_manager_);
  }
}

PaymentsDataManager::~PaymentsDataManager() {
  CancelPendingLocalQuery(&pending_creditcards_query_);
  CancelPendingServerQueries();
}

void PaymentsDataManager::Shutdown() {
  sync_observer_.Reset();
}

void PaymentsDataManager::OnAutofillChangedBySync(syncer::DataType data_type) {
  if (data_type == syncer::AUTOFILL_WALLET_CREDENTIAL ||
      data_type == syncer::AUTOFILL_WALLET_DATA ||
      data_type == syncer::AUTOFILL_WALLET_METADATA ||
      data_type == syncer::AUTOFILL_WALLET_OFFER ||
      data_type == syncer::AUTOFILL_WALLET_USAGE) {
    Refresh();
  }
}

void PaymentsDataManager::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK(HasPendingPaymentQueries());

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
    } else if (h == pending_masked_bank_accounts_query_) {
      CHECK(AreBankAccountsSupported());
      pending_masked_bank_accounts_query_ = 0;
    } else if (h == pending_payment_instruments_query_) {
      CHECK(ArePaymentInsrumentsSupported());
      pending_payment_instruments_query_ = 0;
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
      case MASKED_BANK_ACCOUNTS_RESULT:
        CHECK(AreBankAccountsSupported());
        DCHECK_EQ(h, pending_masked_bank_accounts_query_)
            << "received masked bank accounts from invalid request.";
        ReceiveLoadedDbValues(h, result.get(),
                              &pending_masked_bank_accounts_query_,
                              &masked_bank_accounts_);
        OnMaskedBankAccountsRefreshed();
        break;
      case PAYMENT_INSTRUMENT_RESULT: {
        CHECK(ArePaymentInsrumentsSupported());
        DCHECK_EQ(h, pending_payment_instruments_query_)
            << "received payment instruments from invalid request.";
        std::vector<sync_pb::PaymentInstrument> payment_instruments;
        ReceiveLoadedDbValues(h, result.get(),
                              &pending_payment_instruments_query_,
                              &payment_instruments);
        for (sync_pb::PaymentInstrument& payment_instrument :
             payment_instruments) {
          CacheIfEwalletPaymentInstrument(payment_instrument);
        }
        OnPaymentInstrumentsRefreshed(payment_instruments);
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
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
    PaymentsDataCleaner(this).CleanupPaymentsData();
  }

  NotifyObservers();
}

CoreAccountInfo PaymentsDataManager::GetAccountInfoForPaymentsServer() const {
  // Return the account of the active signed-in user irrespective of whether
  // they enabled sync or not.
  return identity_manager_->GetPrimaryAccountInfo(
      signin::ConsentLevel::kSignin);
}

bool PaymentsDataManager::IsSyncFeatureEnabledForPaymentsServerMetrics() const {
  // TODO(crbug.com/40066949): Simplify once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  return sync_service_ && sync_service_->IsSyncFeatureEnabled();
}

void PaymentsDataManager::OnStateChanged(syncer::SyncService* sync_service) {
  DCHECK_EQ(sync_service_, sync_service);

  // Use the ephemeral account storage when the user didn't enable the sync
  // feature explicitly. `sync_service` is nullptr-checked because this
  // method can also be used (apart from the Sync service observer's calls) in
  // SetSyncService() where setting a nullptr is possible.
  // TODO(crbug.com/40066949): Simplify once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  database_helper_->SetUseAccountStorageForServerData(
      sync_service && !sync_service->IsSyncFeatureEnabled());
}

void PaymentsDataManager::OnAccountsCookieDeletedByUserAction() {
  // Clear all the Sync Transport feature opt-ins.
  prefs::ClearSyncTransportOptIns(pref_service_);
}

void PaymentsDataManager::Refresh() {
  LoadCreditCards();
  LoadCreditCardCloudTokenData();
  LoadIbans();
  if (AreBankAccountsSupported()) {
    LoadMaskedBankAccounts();
  }
  if (ArePaymentInsrumentsSupported()) {
    LoadPaymentInstruments();
  }
  LoadPaymentsCustomerData();
  LoadAutofillOffers();
  LoadVirtualCardUsageData();
  if (IsCardBenefitsSyncEnabled() && IsCardBenefitsPrefEnabled()) {
    LoadCreditCardBenefits();
  }
}

void PaymentsDataManager::AddServerIbanForTest(std::unique_ptr<Iban> iban) {
  server_ibans_.push_back(std::move(iban));
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
    base::FunctionRef<bool(const T&)> filter) const {
  if (!IsAutofillWalletImportEnabled() || !IsAutofillPaymentMethodsEnabled()) {
    return std::nullopt;
  }
  base::Time now = AutofillClock::Now();
  for (const CreditCardBenefit& benefit : credit_card_benefits_) {
    if (const auto* b = absl::get_if<T>(&benefit);
        b && b->linked_card_instrument_id() == instrument_id &&
        b->start_time() <= now && now < b->expiry_time() && filter(*b)) {
      return *b;
    }
  }
  return std::nullopt;
}

std::optional<CreditCardFlatRateBenefit>
PaymentsDataManager::GetFlatRateBenefitByInstrumentId(
    const CreditCardBenefitBase::LinkedCardInstrumentId instrument_id) const {
  return GetCreditCardBenefitByInstrumentId<CreditCardFlatRateBenefit>(
      instrument_id);
}

std::optional<CreditCardCategoryBenefit>
PaymentsDataManager::GetCategoryBenefitByInstrumentIdAndCategory(
    const CreditCardBenefitBase::LinkedCardInstrumentId instrument_id,
    const CreditCardCategoryBenefit::BenefitCategory benefit_category) const {
  return GetCreditCardBenefitByInstrumentId<CreditCardCategoryBenefit>(
      instrument_id, [&benefit_category](const CreditCardCategoryBenefit& b) {
        return b.benefit_category() == benefit_category;
      });
}

std::optional<CreditCardMerchantBenefit>
PaymentsDataManager::GetMerchantBenefitByInstrumentIdAndOrigin(
    const CreditCardBenefitBase::LinkedCardInstrumentId instrument_id,
    const url::Origin& merchant_origin) const {
  return GetCreditCardBenefitByInstrumentId<CreditCardMerchantBenefit>(
      instrument_id, [&merchant_origin](const CreditCardMerchantBenefit& b) {
        return b.merchant_domains().contains(merchant_origin);
      });
}

std::u16string
PaymentsDataManager::GetApplicableBenefitDescriptionForCardAndOrigin(
    const CreditCard& credit_card,
    const url::Origin& origin,
    const AutofillOptimizationGuide* optimization_guide) const {
  // Benefits are only supported for app locale set to U.S. English.
  if (app_locale_ != "en-US") {
    return std::u16string();
  }
  // Ensure that benefit suggestions can be displayed for this card on the
  // current origin.
  if (optimization_guide &&
      optimization_guide->ShouldBlockBenefitSuggestionLabelsForCardAndUrl(
          credit_card, origin.GetURL())) {
    return std::u16string();
  }
  CreditCardBenefitBase::LinkedCardInstrumentId benefit_instrument_id(
      credit_card.instrument_id());

  // 1. Check merchant benefit.
  std::optional<CreditCardMerchantBenefit> merchant_benefit =
      GetMerchantBenefitByInstrumentIdAndOrigin(benefit_instrument_id, origin);
  if (merchant_benefit && merchant_benefit->IsActiveBenefit()) {
    return merchant_benefit->benefit_description();
  }

  // 2. Check category benefit.
  // TODO(crbug.com/331961211): Query PaymentsDataManager before Optimization
  // Guide for category benefits
  if (optimization_guide) {
    CreditCardCategoryBenefit::BenefitCategory category_benefit_type =
        optimization_guide->AttemptToGetEligibleCreditCardBenefitCategory(
            credit_card.issuer_id(), origin.GetURL());
    if (category_benefit_type !=
        CreditCardCategoryBenefit::BenefitCategory::kUnknownBenefitCategory) {
      std::optional<CreditCardCategoryBenefit> category_benefit =
          GetCategoryBenefitByInstrumentIdAndCategory(benefit_instrument_id,
                                                      category_benefit_type);
      if (category_benefit && category_benefit->IsActiveBenefit()) {
        return category_benefit->benefit_description();
      }
    }
  }

  // 3. Check flat rate benefit.
  std::optional<CreditCardFlatRateBenefit> flat_rate_benefit =
      GetFlatRateBenefitByInstrumentId(benefit_instrument_id);
  if (flat_rate_benefit && flat_rate_benefit->IsActiveBenefit()) {
    return flat_rate_benefit->benefit_description();
  }

  // No eligible benefit to display.
  return std::u16string();
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
  if (!IsAutofillWalletImportEnabled()) {
    return {};
  }
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
  if (IsAutofillWalletImportEnabled()) {
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
  if (!IsAutofillWalletImportEnabled()) {
    return {};
  }
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
  if (IsAutofillWalletImportEnabled()) {
    for (const std::unique_ptr<Iban>& iban : server_ibans_) {
      result.push_back(iban.get());
    }
  }

  for (const std::unique_ptr<Iban>& iban : local_ibans_) {
    result.push_back(iban.get());
  }
  return result;
}

std::vector<Iban> PaymentsDataManager::GetOrderedIbansToSuggest() const {
  std::vector<const Iban*> available_ibans =
      ShouldSuggestServerPaymentMethods() ? GetIbans() : GetLocalIbans();
  // Remove any IBAN from the returned list if it's a local IBAN and its
  // prefix, suffix, and length matches any existing server IBAN.
  std::erase_if(available_ibans, [this](const Iban* iban) {
    return iban->record_type() == Iban::kLocalIban &&
           std::ranges::any_of(
               server_ibans_, [&](const std::unique_ptr<Iban>& server_iban) {
                 return server_iban->MatchesPrefixAndSuffix(*iban);
               });
  });

  base::ranges::sort(
      available_ibans, [comparison_time = AutofillClock::Now()](
                           const Iban* iban0, const Iban* iban1) {
        return iban0->HasGreaterRankingThan(iban1, comparison_time);
      });

  std::vector<Iban> ibans_to_suggest;
  ibans_to_suggest.reserve(available_ibans.size());
  for (const Iban* iban : available_ibans) {
    ibans_to_suggest.push_back(*iban);
  }
  return ibans_to_suggest;
}

bool PaymentsDataManager::HasMaskedBankAccounts() const {
  if (!IsAutofillPaymentMethodsEnabled()) {
    return false;
  }
  return !masked_bank_accounts_.empty();
}

bool PaymentsDataManager::HasEwalletAccounts() const {
  if (!IsAutofillPaymentMethodsEnabled()) {
    return false;
  }
  return !ewallet_accounts_.empty();
}

base::span<const BankAccount> PaymentsDataManager::GetMaskedBankAccounts()
    const {
  if (!HasMaskedBankAccounts()) {
    return {};
  }
  return masked_bank_accounts_;
}

base::span<const Ewallet> PaymentsDataManager::GetEwalletAccounts() const {
  if (!HasEwalletAccounts()) {
    return {};
  }
  return ewallet_accounts_;
}

PaymentsCustomerData* PaymentsDataManager::GetPaymentsCustomerData() const {
  return payments_customer_data_ ? payments_customer_data_.get() : nullptr;
}

std::vector<CreditCardCloudTokenData*>
PaymentsDataManager::GetCreditCardCloudTokenData() const {
  if (!IsAutofillWalletImportEnabled()) {
    return {};
  }
  std::vector<CreditCardCloudTokenData*> result;
  result.reserve(server_credit_card_cloud_token_data_.size());
  for (const auto& data : server_credit_card_cloud_token_data_) {
    result.push_back(data.get());
  }
  return result;
}

std::vector<AutofillOfferData*> PaymentsDataManager::GetAutofillOffers() const {
  if (!IsAutofillWalletImportEnabled() || !IsAutofillPaymentMethodsEnabled()) {
    return {};
  }
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
  if (!IsAutofillWalletImportEnabled() || !IsAutofillPaymentMethodsEnabled()) {
    return {};
  }
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
  if (credit_card.card_art_url().is_valid()) {
    return credit_card.card_art_url();
  }

  if (credit_card.record_type() == CreditCard::RecordType::kLocalCard) {
    const CreditCard* server_duplicate_card =
        GetServerCardForLocalCard(&credit_card);
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
  // The sizes are used on Android, but ignored on desktop.
  FetchImagesForURLs(base::span_from_ref(card_art_url),
                     base::span({AutofillImageFetcherBase::ImageSize::kSmall,
                                 AutofillImageFetcherBase::ImageSize::kLarge}));
  return nullptr;
}

gfx::Image* PaymentsDataManager::GetCachedCardArtImageForUrl(
    const GURL& card_art_url) const {
  if (!IsAutofillWalletImportEnabled()) {
    return nullptr;
  }
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

void PaymentsDataManager::SetPrefService(PrefService* pref_service) {
  pref_registrar_.Reset();
  pref_service_ = pref_service;
  // `pref_service_` can be nullptr in tests.
  if (!pref_service_) {
    return;
  }
  pref_registrar_.Init(pref_service);
  pref_registrar_.Add(prefs::kAutofillCreditCardEnabled,
                      base::BindRepeating(&PaymentsDataManager::Refresh,
                                          base::Unretained(this)));
  pref_registrar_.Add(
      prefs::kAutofillPaymentCardBenefits,
      base::BindRepeating(
          &PaymentsDataManager::OnAutofillPaymentsCardBenefitsPrefChange,
          base::Unretained(this)));
}

void PaymentsDataManager::NotifyObservers() {
  if (!HasPendingPaymentQueries()) {
    for (Observer& o : observers_) {
      o.OnPaymentsDataChanged();
    }
  }
}

bool PaymentsDataManager::IsCardEligibleForBenefits(
    const CreditCard& card) const {
  return (card.issuer_id() == kAmexCardIssuerId &&
          base::FeatureList::IsEnabled(
              features::kAutofillEnableCardBenefitsForAmericanExpress)) ||
         (card.issuer_id() == kCapitalOneCardIssuerId &&
          base::FeatureList::IsEnabled(
              features::kAutofillEnableCardBenefitsForCapitalOne));
}

bool PaymentsDataManager::IsCardBenefitsFeatureEnabled() {
  return base::FeatureList::IsEnabled(
             features::kAutofillEnableCardBenefitsForAmericanExpress) ||
         base::FeatureList::IsEnabled(
             features::kAutofillEnableCardBenefitsForCapitalOne);
}

bool PaymentsDataManager::IsCardBenefitsPrefEnabled() const {
  return prefs::IsPaymentCardBenefitsEnabled(pref_service_);
}

bool PaymentsDataManager::IsCardBenefitsSyncEnabled() const {
  return base::FeatureList::IsEnabled(
      features::kAutofillEnableCardBenefitsSync);
}

bool PaymentsDataManager::IsAutofillPaymentMethodsEnabled() const {
  return prefs::IsAutofillPaymentMethodsEnabled(pref_service_);
}

bool PaymentsDataManager::IsAutofillHasSeenIbanPrefEnabled() const {
  return prefs::HasSeenIban(pref_service_);
}

void PaymentsDataManager::SetAutofillHasSeenIban() {
  prefs::SetAutofillHasSeenIban(pref_service_);
}

bool PaymentsDataManager::IsAutofillWalletImportEnabled() const {
  if (is_syncing_for_test_) {
    return true;
  }

  if (!sync_service_) {
    // Without `sync_service_`, namely in off-the-record profiles, wallet import
    // is effectively disabled.
    return false;
  }

  return sync_service_->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPayments);
}

bool PaymentsDataManager::IsPaymentsWalletSyncTransportEnabled() const {
  if (!sync_service_ || !identity_manager_ ||
      sync_service_->GetAccountInfo().IsEmpty() ||
      sync_service_->GetTransportState() ==
          syncer::SyncService::TransportState::PAUSED) {
    return false;
  }
  // TODO(crbug.com/40066949): Simplify (merge with IsPaymentsDownloadActive())
  // once ConsentLevel::kSync and SyncService::IsSyncFeatureEnabled() are
  // deleted from the codebase.
  return !sync_service_->IsSyncFeatureEnabled() &&
         sync_service_->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA);
}

bool PaymentsDataManager::IsPaymentsDownloadActive() const {
  if (!sync_service_ || !identity_manager_ ||
      sync_service_->GetAccountInfo().IsEmpty() ||
      sync_service_->GetTransportState() ==
          syncer::SyncService::TransportState::PAUSED) {
    return false;
  }
  // TODO(crbug.com/40066949): Simplify (merge with
  // IsPaymentsWalletSyncTransportEnabled()) once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  return sync_service_->IsSyncFeatureEnabled() ||
         sync_service_->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA);
}

AutofillMetrics::PaymentsSigninState
PaymentsDataManager::GetPaymentsSigninStateForMetrics() const {
  using PaymentsSigninState = AutofillMetrics::PaymentsSigninState;

  // Check if the user is signed out.
  if (!sync_service_ || !identity_manager_ ||
      sync_service_->GetAccountInfo().IsEmpty()) {
    return PaymentsSigninState::kSignedOut;
  }

  if (sync_service_->GetTransportState() ==
      syncer::SyncService::TransportState::PAUSED) {
    return PaymentsSigninState::kSyncPaused;
  }

  // Check if the user has turned on sync.
  // TODO(crbug.com/40066949): Simplify once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  if (sync_service_->IsSyncFeatureEnabled()) {
    return PaymentsSigninState::kSignedInAndSyncFeatureEnabled;
  }

  // Check if Wallet data types are supported.
  if (sync_service_->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA)) {
    return PaymentsSigninState::kSignedInAndWalletSyncTransportEnabled;
  }

  return PaymentsSigninState::kSignedIn;
}

bool PaymentsDataManager::IsCardPresentAsBothLocalAndServerCards(
    const CreditCard& credit_card) {
  for (const CreditCard* card_from_list : GetCreditCards()) {
    if (credit_card.IsLocalOrServerDuplicateOf(*card_from_list)) {
      return true;
    }
  }
  return false;
}

const CreditCard* PaymentsDataManager::GetServerCardForLocalCard(
    const CreditCard* local_card) const {
  DCHECK(local_card);
  if (local_card->record_type() != CreditCard::RecordType::kLocalCard) {
    return nullptr;
  }

  std::vector<CreditCard*> server_cards = GetServerCreditCards();
  auto it =
      base::ranges::find_if(server_cards, [&](const CreditCard* server_card) {
        return local_card->IsLocalOrServerDuplicateOf(*server_card);
      });

  if (it != server_cards.end()) {
    return *it;
  }

  return nullptr;
}

std::string PaymentsDataManager::OnAcceptedLocalCreditCardSave(
    const CreditCard& imported_card) {
  DCHECK(!imported_card.number().empty());
  return SaveImportedCreditCard(imported_card);
}

std::string PaymentsDataManager::OnAcceptedLocalIbanSave(Iban imported_iban) {
  DCHECK(!imported_iban.value().empty());
  // If an existing IBAN is found, call `UpdateIban()`, otherwise,
  // `AddAsLocalIban()`. `local_ibans_` will be in sync with the local web
  // database as of `Refresh()` which will be called by both `UpdateIban()` and
  // `AddAsLocalIban()`.
  for (auto& iban : local_ibans_) {
    if (iban->value() == imported_iban.value()) {
      // Set the GUID of the IBAN to the one that matches it in
      // `local_ibans_` so that UpdateIban() will be able to update the
      // specific IBAN.
      imported_iban.set_identifier(Iban::Guid(iban->guid()));
      return UpdateIban(imported_iban);
    }
  }
  return AddAsLocalIban(std::move(imported_iban));
}

bool PaymentsDataManager::IsKnownCard(const CreditCard& credit_card) const {
  const auto stripped_pan = StripCardNumberSeparators(credit_card.number());
  for (const auto& card : local_credit_cards_) {
    if (stripped_pan == StripCardNumberSeparators(card->number())) {
      return true;
    }
  }

  const auto masked_info = credit_card.NetworkAndLastFourDigits();
  for (const auto& card : server_credit_cards_) {
    if (masked_info == card->NetworkAndLastFourDigits()) {
      return true;
    }
  }

  return false;
}

bool PaymentsDataManager::IsServerCard(const CreditCard* credit_card) const {
  // Check whether the current card itself is a server card.
  if (credit_card->record_type() != CreditCard::RecordType::kLocalCard) {
    return true;
  }

  std::vector<CreditCard*> server_credit_cards = GetServerCreditCards();
  // Check whether the current card is already uploaded.
  for (const CreditCard* server_card : server_credit_cards) {
    if (credit_card->MatchingCardDetails(*server_card)) {
      return true;
    }
  }
  return false;
}

bool PaymentsDataManager::ShouldShowCardsFromAccountOption() const {
// The feature is only for Linux, Windows, Mac, and Fuchsia.
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
  // This option should only be shown for users that have not enabled the Sync
  // Feature and that have server credit cards available.
  // TODO(crbug.com/40066949): Simplify once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  if (!sync_service_ || sync_service_->IsSyncFeatureEnabled() ||
      GetServerCreditCards().empty()) {
    return false;
  }

  bool is_opted_in = prefs::IsUserOptedInWalletSyncTransport(
      pref_service_, sync_service_->GetAccountInfo().account_id);

  // The option should only be shown if the user has not already opted-in and
  // the flag to remove the dropdown is disabled.
  return !is_opted_in && !base::FeatureList::IsEnabled(
                             features::kAutofillRemovePaymentsButterDropdown);
#else
  return false;
#endif  // #if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) ||
        // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
}

void PaymentsDataManager::OnUserAcceptedCardsFromAccountOption() {
  DCHECK(IsPaymentsWalletSyncTransportEnabled());
  prefs::SetUserOptedInWalletSyncTransport(
      pref_service_, sync_service_->GetAccountInfo().account_id,
      /*opted_in=*/true);
}

void PaymentsDataManager::OnUserAcceptedUpstreamOffer() {
  // If the user is in sync transport mode for Wallet, record an opt-in.
  if (IsPaymentsWalletSyncTransportEnabled()) {
    prefs::SetUserOptedInWalletSyncTransport(
        pref_service_, sync_service_->GetAccountInfo().account_id,
        /*opted_in=*/true);
  }
}

void PaymentsDataManager::SetPaymentMethodsMandatoryReauthEnabled(
    bool enabled) {
  prefs::SetPaymentMethodsMandatoryReauthEnabled(pref_service_, enabled);
}

bool PaymentsDataManager::IsPaymentMethodsMandatoryReauthEnabled() {
  return prefs::IsPaymentMethodsMandatoryReauthEnabled(pref_service_);
}

bool PaymentsDataManager::ShouldShowPaymentMethodsMandatoryReauthPromo() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  // There is no need to show the promo if the feature is already enabled.
  if (prefs::IsPaymentMethodsMandatoryReauthEnabled(pref_service_)) {
#if BUILDFLAG(IS_ANDROID)
    // The mandatory reauth feature is always enabled on automotive, there
    // is/was no opt-in. As such, there is no need to log anything here on
    // automotive.
    if (!base::android::BuildInfo::GetInstance()->is_automotive()) {
      LogMandatoryReauthOfferOptInDecision(
          MandatoryReauthOfferOptInDecision::kAlreadyOptedIn);
    }
#else
    LogMandatoryReauthOfferOptInDecision(
        MandatoryReauthOfferOptInDecision::kAlreadyOptedIn);
#endif  // BUILDFLAG(IS_ANDROID)
    return false;
  }

  // If the user has explicitly opted out of this feature previously, then we
  // should not show the opt-in promo.
  if (prefs::IsPaymentMethodsMandatoryReauthSetExplicitly(pref_service_)) {
    LogMandatoryReauthOfferOptInDecision(
        MandatoryReauthOfferOptInDecision::kAlreadyOptedOut);
    return false;
  }

  // We should only show the opt-in promo if we have not reached the maximum
  // number of shows for the promo.
  bool allowed_by_strike_database =
      prefs::IsPaymentMethodsMandatoryReauthPromoShownCounterBelowMaxCap(
          pref_service_);
  if (!allowed_by_strike_database) {
    LogMandatoryReauthOfferOptInDecision(
        MandatoryReauthOfferOptInDecision::kBlockedByStrikeDatabase);
  }
  return allowed_by_strike_database;
#else
  return false;
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
}

void PaymentsDataManager::
    IncrementPaymentMethodsMandatoryReauthPromoShownCounter() {
  prefs::IncrementPaymentMethodsMandatoryReauthPromoShownCounter(pref_service_);
}

bool PaymentsDataManager::IsPaymentCvcStorageEnabled() {
  return base::FeatureList::IsEnabled(
             features::kAutofillEnableCvcStorageAndFilling) &&
         prefs::IsPaymentCvcStorageEnabled(pref_service_);
}

base::span<const VirtualCardUsageData>
PaymentsDataManager::GetVirtualCardUsageData() const {
  if (!IsAutofillWalletImportEnabled() || !IsAutofillPaymentMethodsEnabled()) {
    return {};
  }
  return autofill_virtual_card_usage_data_;
}

std::vector<CreditCard*> PaymentsDataManager::GetCreditCardsToSuggest(
    bool should_use_legacy_algorithm) const {
  if (!IsAutofillPaymentMethodsEnabled()) {
    return {};
  }
  std::vector<CreditCard*> credit_cards;
  if (ShouldSuggestServerPaymentMethods()) {
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
              [comparison_time, should_use_legacy_algorithm](
                  const CreditCard* a, const CreditCard* b) {
                const bool a_is_expired = a->IsExpired(comparison_time);
                if (a_is_expired != b->IsExpired(comparison_time)) {
                  return !a_is_expired;
                }

                return a->HasGreaterRankingThan(*b, comparison_time,
                                                should_use_legacy_algorithm);
              });
  }

  return cards_to_suggest;
}

std::string PaymentsDataManager::AddAsLocalIban(Iban iban) {
  CHECK_EQ(iban.record_type(), Iban::kUnknown);
  // IBAN shares the same pref with payment methods enablement toggle.
  if (!IsAutofillPaymentMethodsEnabled()) {
    return std::string();
  }

  // Sets the `kAutofillHasSeenIban` pref to true indicating that the user has
  // added an IBAN via Chrome payment settings page or accepted the save-IBAN
  // prompt, which indicates that the user is familiar with IBANs as a concept.
  // We set the pref so that even if the user travels to a country where IBAN
  // functionality is not typically used, they will still be able to save new
  // IBANs from the settings page using this pref.
  SetAutofillHasSeenIban();

  if (!GetLocalDatabase()) {
    return std::string();
  }

  // Set the GUID as this IBAN will be saved locally.
  iban.set_identifier(
      Iban::Guid(base::Uuid::GenerateRandomV4().AsLowercaseString()));
  iban.set_record_type(Iban::kLocalIban);
  // Search through `local_ibans_` to ensure no IBAN that already saved has the
  // same value and nickname as `iban`, because we do not want to add two IBANs
  // with the exact same data.
  if (std::ranges::any_of(local_ibans_,
                          [&iban](const std::unique_ptr<Iban>& local_iban) {
                            return iban.value() == local_iban->value() &&
                                   iban.nickname() == local_iban->nickname();
                          })) {
    return std::string();
  }

  // Add the new IBAN to the web database.
  GetLocalDatabase()->AddLocalIban(iban);

  // Refresh our local cache and send notifications to observers.
  Refresh();
  return iban.guid();
}

std::string PaymentsDataManager::UpdateIban(const Iban& iban) {
  if (!GetLocalDatabase()) {
    return std::string();
  }

  // Make the update.
  GetLocalDatabase()->UpdateLocalIban(iban);

  // Refresh our local cache and send notifications to observers.
  Refresh();
  return iban.guid();
}

void PaymentsDataManager::AddCreditCard(const CreditCard& credit_card) {
  if (!IsAutofillPaymentMethodsEnabled()) {
    return;
  }

  if (credit_card.IsEmpty(app_locale_)) {
    return;
  }

  if (FindByGUID(local_credit_cards_, credit_card.guid())) {
    return;
  }

  if (!GetLocalDatabase()) {
    return;
  }

  // Don't add a duplicate.
  if (FindByContents(local_credit_cards_, credit_card)) {
    return;
  }

  // Add the new credit card to the web database.
  GetLocalDatabase()->AddCreditCard(credit_card);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PaymentsDataManager::DeleteLocalCreditCards(
    const std::vector<CreditCard>& cards) {
  DCHECK(database_helper_);
  DCHECK(GetLocalDatabase()) << "Use of local card without local storage.";

  for (const auto& card : cards) {
    GetLocalDatabase()->RemoveCreditCard(card.guid());
  }

  // Refresh the database, so latest state is reflected in all consumers.
  if (!cards.empty()) {
    Refresh();
  }
}

void PaymentsDataManager::DeleteAllLocalCreditCards() {
  std::vector<CreditCard*> credit_cards = GetLocalCreditCards();

  std::vector<CreditCard> cards_to_delete;
  cards_to_delete.reserve(credit_cards.size());
  for (const CreditCard* card : credit_cards) {
    cards_to_delete.push_back(*card);
  }

  DeleteLocalCreditCards(cards_to_delete);
}

void PaymentsDataManager::UpdateCreditCard(const CreditCard& credit_card) {
  DCHECK_EQ(CreditCard::RecordType::kLocalCard, credit_card.record_type());
  CreditCard* existing_credit_card = GetCreditCardByGUID(credit_card.guid());
  if (!existing_credit_card) {
    return;
  }

  // Don't overwrite the origin for a credit card that is already stored.
  if (existing_credit_card->Compare(credit_card) == 0) {
    return;
  }

  if (credit_card.IsEmpty(app_locale_)) {
    RemoveByGUID(credit_card.guid());
    return;
  }

  // Update the cached version.
  *existing_credit_card = credit_card;

  if (!GetLocalDatabase()) {
    return;
  }

  // Make the update.
  GetLocalDatabase()->UpdateCreditCard(credit_card);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PaymentsDataManager::UpdateLocalCvc(const std::string& guid,
                                         const std::u16string& cvc) {
  if (!GetLocalDatabase()) {
    return;
  }

  CreditCard* existing_credit_card = GetCreditCardByGUID(guid);
  if (!existing_credit_card) {
    return;
  }

  GetLocalDatabase()->UpdateLocalCvc(guid, cvc);
  Refresh();
}

void PaymentsDataManager::UpdateServerCardsMetadata(
    const std::vector<CreditCard>& credit_cards) {
  DCHECK(GetServerDatabase())
      << "Updating server card metadata without server storage.";

  for (const auto& credit_card : credit_cards) {
    DCHECK_NE(CreditCard::RecordType::kLocalCard, credit_card.record_type());
    GetServerDatabase()->UpdateServerCardMetadata(credit_card);
  }

  Refresh();
}

void PaymentsDataManager::AddServerCvc(int64_t instrument_id,
                                       const std::u16string& cvc) {
  // We don't check the validity of the instrument_id.
  // When a user saves a new card along with the CVC, we first save the card and
  // wait for the instrument id passed back from the UploadResponse. Then this
  // function is triggered to save server cvc. At this time, a new card should
  // theoretically be synced down via Chrome Sync but it can be delayed. As a
  // result, this Chrome client does not have the instrument id yet in the card
  // table but it should invoke the AddServerCvc.
  CHECK(!cvc.empty());
  CHECK(GetServerDatabase()) << "Adding Server cvc without server storage.";

  // Add the new server cvc to the web database.
  GetServerDatabase()->AddServerCvc(instrument_id, cvc);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PaymentsDataManager::UpdateServerCvc(int64_t instrument_id,
                                          const std::u16string& cvc) {
  CHECK(GetCreditCardByInstrumentId(instrument_id));
  CHECK(!cvc.empty());
  CHECK(GetServerDatabase()) << "Updating Server cvc without server storage.";

  // Update the new server cvc to the web database.
  GetServerDatabase()->UpdateServerCvc(instrument_id, cvc);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PaymentsDataManager::RemoveServerCvc(int64_t instrument_id) {
  // We don't check the validity of the instrument_id.
  // This is only called in cvc sync bridge's ApplyIncrementalSyncChanges()
  // call. If the card sync finishes before cvc sync, the card is gone before
  // removing cvc.
  CHECK(GetServerDatabase()) << "Removing Server cvc without server storage.";

  // Remove the server cvc in the web database.
  GetServerDatabase()->RemoveServerCvc(instrument_id);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PaymentsDataManager::ClearServerCvcs() {
  CHECK(GetServerDatabase()) << "Removing Server cvc without server storage.";

  // Clear the server cvc in the web database.
  GetServerDatabase()->ClearServerCvcs();

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PaymentsDataManager::ClearLocalCvcs() {
  CHECK(GetLocalDatabase()) << "Removing Local cvcs without local storage.";

  // Clear the local CVCs in the web database.
  GetLocalDatabase()->ClearLocalCvcs();

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PaymentsDataManager::ClearAllServerDataForTesting() {
  // This could theoretically be called before we get the data back from the
  // database on startup, and it could get called when the wallet pref is
  // off (meaning this class won't even query for the server data) so don't
  // check the server_credit_cards_/profiles_ before posting to the DB.

  // TODO(crbug.com/40585321): Move this null check logic to the database
  // helper. The server database can be null for a limited amount of time before
  // the sync service gets initialized. Not clearing it does not matter in that
  // case since it will not have been created yet (nothing to clear).
  if (GetServerDatabase()) {
    GetServerDatabase()->ClearAllServerData();
  }

  // The above call will eventually clear our server data by notifying us
  // that the data changed and then this class will re-fetch. Preemptively
  // clear so that tests can synchronously verify that this data was cleared.
  server_credit_cards_.clear();
  server_ibans_.clear();
  payments_customer_data_.reset();
  server_credit_card_cloud_token_data_.clear();
  autofill_offer_data_.clear();
  credit_card_art_images_.clear();
  masked_bank_accounts_.clear();
}

void PaymentsDataManager::SetCreditCards(
    std::vector<CreditCard>* credit_cards) {
  // Remove empty credit cards from input.
  std::erase_if(*credit_cards, [this](const CreditCard& credit_card) {
    return credit_card.IsEmpty(app_locale_);
  });

  if (!GetLocalDatabase()) {
    return;
  }

  // Any credit cards that are not in the new credit card list should be
  // removed.
  for (const auto& card : local_credit_cards_) {
    if (!FindByGUID(*credit_cards, card->guid())) {
      GetLocalDatabase()->RemoveCreditCard(card->guid());
    }
  }

  // Update the web database with the existing credit cards.
  for (const CreditCard& card : *credit_cards) {
    if (FindByGUID(local_credit_cards_, card.guid())) {
      GetLocalDatabase()->UpdateCreditCard(card);
    }
  }

  // Add the new credit cards to the web database.  Don't add a duplicate.
  for (const CreditCard& card : *credit_cards) {
    if (!FindByGUID(local_credit_cards_, card.guid()) &&
        !FindByContents(local_credit_cards_, card)) {
      GetLocalDatabase()->AddCreditCard(card);
    }
  }

  // Copy in the new credit cards.
  local_credit_cards_.clear();
  for (const CreditCard& card : *credit_cards) {
    local_credit_cards_.push_back(std::make_unique<CreditCard>(card));
  }

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

bool PaymentsDataManager::SaveCardLocallyIfNew(
    const CreditCard& imported_card) {
  CHECK(!imported_card.number().empty());

  std::vector<CreditCard> credit_cards;
  for (auto& card : local_credit_cards_) {
    if (card->MatchingCardDetails(imported_card)) {
      return false;
    }
    credit_cards.push_back(*card);
  }
  credit_cards.push_back(imported_card);

  SetCreditCards(&credit_cards);
  return true;
}

bool PaymentsDataManager::RemoveByGUID(const std::string& guid) {
  if (!GetLocalDatabase()) {
    return false;
  }

  if (FindByGUID(local_credit_cards_, guid)) {
    GetLocalDatabase()->RemoveCreditCard(guid);
    // Refresh our local cache and send notifications to observers.
    Refresh();
    return true;
  } else if (FindByGUID(local_ibans_, guid)) {
    GetLocalDatabase()->RemoveLocalIban(guid);
    // Refresh our local cache and send notifications to observers.
    Refresh();
    return true;
  }
  return false;
}

void PaymentsDataManager::RemoveLocalDataModifiedBetween(base::Time begin,
                                                         base::Time end) {
  if (end.is_null()) {
    end = base::Time::Max();
  }
  for (const CreditCard* card : GetLocalCreditCards()) {
    if (card->modification_date() >= begin && card->modification_date() < end) {
      RemoveByGUID(card->guid());
    } else if (base::FeatureList::IsEnabled(
                   features::kAutofillEnableCvcStorageAndFilling) &&
               card->cvc_modification_date() >= begin &&
               card->cvc_modification_date() < end) {
      UpdateLocalCvc(card->guid(), u"");
    }
  }
}

void PaymentsDataManager::RecordUseOfCard(const CreditCard* card) {
  CreditCard* credit_card = GetCreditCardByGUID(card->guid());
  if (!credit_card) {
    return;
  }

  credit_card->RecordAndLogUse();

  if (credit_card->record_type() == CreditCard::RecordType::kLocalCard) {
    // Fail silently if there's no local database, because we need to
    // support this for tests.
    if (GetLocalDatabase()) {
      GetLocalDatabase()->UpdateCreditCard(*credit_card);
    }
  } else {
    DCHECK(GetServerDatabase())
        << "Recording use of server card without server storage.";
    GetServerDatabase()->UpdateServerCardMetadata(*credit_card);
  }

  Refresh();
}

void PaymentsDataManager::RecordUseOfIban(Iban& iban) {
  iban.RecordAndLogUse();

  if (iban.record_type() == Iban::RecordType::kServerIban) {
    CHECK(GetServerDatabase())
        << "Recording use of server IBAN metadata without server storage.";
    GetServerDatabase()->UpdateServerIbanMetadata(iban);
  } else {
    if (GetLocalDatabase()) {
      GetLocalDatabase()->UpdateLocalIban(iban);
    }
  }

  Refresh();
}

// The priority ranking for deduping a duplicate card is:
// 1. RecordType::kMaskedServerCard
// 2. RecordType::kLocalCard
// static
void PaymentsDataManager::DedupeCreditCardToSuggest(
    std::list<CreditCard*>* cards_to_suggest) {
  for (auto outer_it = cards_to_suggest->begin();
       outer_it != cards_to_suggest->end(); ++outer_it) {
    // Full server cards should never be suggestions, as they exist only as a
    // cached state post-fill.
    CHECK_NE((*outer_it)->record_type(),
             CreditCard::RecordType::kFullServerCard);

    for (auto inner_it = cards_to_suggest->begin();
         inner_it != cards_to_suggest->end();) {
      auto inner_it_copy = inner_it++;
      if (outer_it == inner_it_copy) {
        continue;
      }
      // Check if the cards are local or server duplicate of each other. If yes,
      // then delete the duplicate if it's a local card.
      if ((*inner_it_copy)->IsLocalOrServerDuplicateOf(**outer_it) &&
          (*inner_it_copy)->record_type() ==
              CreditCard::RecordType::kLocalCard) {
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
bool PaymentsDataManager::IsUsingAccountStorageForServerDataForTest() {
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
  if (AreBankAccountsSupported()) {
    CancelPendingServerQuery(&pending_masked_bank_accounts_query_);
  }
  if (ArePaymentInsrumentsSupported()) {
    CancelPendingServerQuery(&pending_payment_instruments_query_);
  }
}

bool PaymentsDataManager::ShouldSuggestServerPaymentMethods() const {
  if (!IsAutofillWalletImportEnabled()) {
    return false;
  }

  if (is_syncing_for_test_) {
    return true;
  }

  CHECK(sync_service_);

  // Check if the user is in sync transport mode for wallet data.
  // TODO(crbug.com/40066949): Simplify once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  if (!sync_service_->IsSyncFeatureEnabled()) {
    // For SyncTransport, only show server payment methods if the user has
    // opted in to seeing them in the dropdown.
    if (!prefs::IsUserOptedInWalletSyncTransport(
            pref_service_, sync_service_->GetAccountInfo().account_id)) {
      // If the AutofillRemovePaymentsButterDropdown feature is enabled, all
      // users can see server payment methods, even in SyncTransport mode.
      if (!base::FeatureList::IsEnabled(
              features::kAutofillRemovePaymentsButterDropdown)) {
        return false;
      }
    }
  }

  // Server payment methods should be suggested if the sync service is active.
  return sync_service_->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA);
}

void PaymentsDataManager::LoadCreditCards() {
  if (!database_helper_->GetLocalDatabase()) {
    NOTREACHED_IN_MIGRATION();
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
    NOTREACHED_IN_MIGRATION();
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

void PaymentsDataManager::LoadMaskedBankAccounts() {
  if (!database_helper_->GetServerDatabase()) {
    return;
  }

  CancelPendingServerQuery(&pending_masked_bank_accounts_query_);

  pending_masked_bank_accounts_query_ =
      database_helper_->GetServerDatabase()->GetMaskedBankAccounts(this);
}

void PaymentsDataManager::LoadPaymentInstruments() {
  if (!database_helper_->GetServerDatabase()) {
    return;
  }

  CancelPendingServerQuery(&pending_payment_instruments_query_);

  pending_payment_instruments_query_ =
      database_helper_->GetServerDatabase()->GetPaymentInstruments(this);
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
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
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
    base::span<const GURL> updated_urls,
    base::span<const AutofillImageFetcherBase::ImageSize> image_sizes) const {
  if (!image_fetcher_) {
    return;
  }

  image_fetcher_->FetchImagesForURLs(
      updated_urls, image_sizes,
      base::BindOnce(&PaymentsDataManager::OnCardArtImagesFetched,
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

void PaymentsDataManager::LogServerCardLinkClicked() const {
  AutofillMetrics::LogServerCardLinkClicked(GetPaymentsSigninStateForMetrics());
}

void PaymentsDataManager::LogServerIbanLinkClicked() const {
  autofill_metrics::LogServerIbanLinkClicked(
      GetPaymentsSigninStateForMetrics());
}

const std::string& PaymentsDataManager::GetCountryCodeForExperimentGroup()
    const {
  // Set to |variations_country_code_| if it exists.
  if (experiment_country_code_.empty()) {
    experiment_country_code_ = variations_country_code_.value();
  }

  // Failing that, guess based on system timezone.
  if (experiment_country_code_.empty()) {
    experiment_country_code_ = base::CountryCodeForCurrentTimezone();
  }

  // Failing that, guess based on locale. This returns "US" if there is no good
  // guess.
  if (experiment_country_code_.empty()) {
    experiment_country_code_ =
        AutofillCountry::CountryCodeForLocale(app_locale_);
  }

  return experiment_country_code_;
}

void PaymentsDataManager::SetSyncServiceForTest(
    syncer::SyncService* sync_service) {
  sync_service_ = sync_service;
  sync_observer_.Reset();
  if (sync_service_) {
    sync_observer_.Observe(sync_service_);
  }
  OnStateChanged(sync_service_);
}

void PaymentsDataManager::AddMaskedBankAccountForTest(
    const BankAccount& bank_account) {
  masked_bank_accounts_.push_back(bank_account);
}

void PaymentsDataManager::AddServerCreditCardForTest(
    std::unique_ptr<CreditCard> credit_card) {
  server_credit_cards_.push_back(std::move(credit_card));
}

void PaymentsDataManager::AddCreditCardBenefitForTest(
    CreditCardBenefit benefit) {
  credit_card_benefits_.push_back(std::move(benefit));
}

bool PaymentsDataManager::IsFacilitatedPaymentsPixUserPrefEnabled() const {
  return prefs::IsFacilitatedPaymentsPixEnabled(pref_service_);
}

bool PaymentsDataManager::HasPendingPaymentQueries() const {
  return pending_creditcards_query_ != 0 ||
         pending_server_creditcards_query_ != 0 ||
         pending_server_creditcard_cloud_token_data_query_ != 0 ||
         pending_customer_data_query_ != 0 || pending_offer_data_query_ != 0 ||
         pending_virtual_card_usage_data_query_ != 0 ||
         pending_credit_card_benefit_query_ != 0 ||
         pending_local_ibans_query_ != 0 || pending_server_ibans_query_ != 0 ||
         (AreBankAccountsSupported() &&
          pending_masked_bank_accounts_query_ != 0) ||
         (ArePaymentInsrumentsSupported() &&
          pending_payment_instruments_query_ != 0);
}

bool PaymentsDataManager::AreBankAccountsSupported() const {
#if BUILDFLAG(IS_ANDROID)
  return base::FeatureList::IsEnabled(
      features::kAutofillEnableSyncingOfPixBankAccounts);
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

bool PaymentsDataManager::AreEwalletAccountsSupported() const {
#if BUILDFLAG(IS_ANDROID)
  return base::FeatureList::IsEnabled(features::kAutofillSyncEwalletAccounts);
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

bool PaymentsDataManager::ArePaymentInsrumentsSupported() const {
  // Currently only eWallet accounts are using generic payment instrument proto
  // for read from table.
  return AreEwalletAccountsSupported();
}

void PaymentsDataManager::OnAutofillPaymentsCardBenefitsPrefChange() {
  prefs::IsPaymentCardBenefitsEnabled(pref_service_)
      ? LoadCreditCardBenefits()
      : ClearAllCreditCardBenefits();
}

void PaymentsDataManager::ClearAllCreditCardBenefits() {
  credit_card_benefits_.clear();
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
    FetchImagesForURLs(
        updated_urls,
        base::span({AutofillImageFetcherBase::ImageSize::kSmall,
                    AutofillImageFetcherBase::ImageSize::kLarge}));
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

std::string PaymentsDataManager::SaveImportedCreditCard(
    const CreditCard& imported_card) {
  // Set to true if |imported_card| is merged into the credit card list.
  bool merged = false;
  std::string guid = imported_card.guid();
  std::vector<CreditCard> credit_cards;
  for (auto& card : local_credit_cards_) {
    // If |imported_card| has not yet been merged, check whether it should be
    // with the current |card|.
    if (!merged && card->UpdateFromImportedCard(imported_card, app_locale_)) {
      guid = card->guid();
      merged = true;
    }

    credit_cards.push_back(*card);
  }

  if (!merged) {
    credit_cards.push_back(imported_card);
  }

  SetCreditCards(&credit_cards);

  return guid;
}

void PaymentsDataManager::OnMaskedBankAccountsRefreshed() {
  std::vector<GURL> updated_urls;
  for (const BankAccount& bank_account : masked_bank_accounts_) {
    const GURL& display_icon_url =
        bank_account.payment_instrument().display_icon_url();
    if (!display_icon_url.is_valid()) {
      continue;
    }
    updated_urls.emplace_back(display_icon_url);
  }
  if (!updated_urls.empty()) {
    FetchImagesForURLs(
        updated_urls,
        base::span({AutofillImageFetcherBase::ImageSize::kSquare}));
  }
}

void PaymentsDataManager::OnPaymentInstrumentsRefreshed(
    const std::vector<sync_pb::PaymentInstrument>& payment_instruments) {
  std::vector<GURL> updated_urls;
  for (const sync_pb::PaymentInstrument& payment_instrument :
       payment_instruments) {
    // This check ensures only the display_icon_url of an eWallet account will
    // be cached. Expand to other kinds of payment instruments when needed.
    if (!payment_instrument.has_ewallet_details()) {
      continue;
    }
    const GURL display_icon_url(payment_instrument.display_icon_url());
    if (!display_icon_url.is_valid()) {
      continue;
    }
    updated_urls.emplace_back(display_icon_url);
  }
  if (!updated_urls.empty()) {
    FetchImagesForURLs(
        updated_urls,
        base::span({AutofillImageFetcherBase::ImageSize::kSquare}));
  }
}

void PaymentsDataManager::CacheIfEwalletPaymentInstrument(
    sync_pb::PaymentInstrument& payment_instrument) {
  if (!payment_instrument.has_ewallet_details()) {
    return;
  }
  std::vector<std::u16string> supported_payment_link_uris;
  for (const std::string& uri :
       payment_instrument.ewallet_details().supported_payment_link_uris()) {
    supported_payment_link_uris.push_back(base::UTF8ToUTF16(uri));
  }
  ewallet_accounts_.emplace_back(
      payment_instrument.instrument_id(),
      base::UTF8ToUTF16(payment_instrument.nickname()),
      GURL(payment_instrument.display_icon_url()),
      base::UTF8ToUTF16(payment_instrument.ewallet_details().ewallet_name()),
      base::UTF8ToUTF16(
          payment_instrument.ewallet_details().account_display_name()),
      supported_payment_link_uris,
      payment_instrument.device_details().is_fido_enrolled());
}

}  // namespace autofill
