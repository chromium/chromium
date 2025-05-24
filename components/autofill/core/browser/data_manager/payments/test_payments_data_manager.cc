// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/ui/test_autofill_image_fetcher.h"

namespace autofill {

TestPaymentsDataManager::TestPaymentsDataManager(const std::string& app_locale)
    : PaymentsDataManager(/*profile_database=*/nullptr,
                          /*account_database=*/nullptr,
                          /*image_fetcher=*/nullptr,
                          /*shared_storage_handler=*/nullptr,
                          /*pref_service=*/nullptr,
                          /*sync_service=*/nullptr,
                          /*identity_manager=*/nullptr,
                          /*variations_country_code=*/GeoIpCountryCode("US"),
                          app_locale) {
  is_payments_data_loaded_ = true;
  owned_image_fetcher_ = std::make_unique<TestAutofillImageFetcher>();
  image_fetcher_ = owned_image_fetcher_.get();
}

TestPaymentsDataManager::~TestPaymentsDataManager() {
  // Clear `image_fetcher_` raw pointer because the `owned_image_fetcher_` goes
  // first out of scope.
  image_fetcher_ = nullptr;
}

void TestPaymentsDataManager::LoadCreditCards() {
  // Overridden to avoid a trip to the database.
  pending_creditcards_query_ = 125;
  pending_server_creditcards_query_ = 126;
  {
    std::vector<std::unique_ptr<CreditCard>> credit_cards;
    local_credit_cards_.swap(credit_cards);
    std::unique_ptr<WDTypedResult> result =
        std::make_unique<WDResult<std::vector<std::unique_ptr<CreditCard>>>>(
            AUTOFILL_CREDITCARDS_RESULT, std::move(credit_cards));
    OnWebDataServiceRequestDone(pending_creditcards_query_, std::move(result));
  }
  {
    std::vector<std::unique_ptr<CreditCard>> credit_cards;
    server_credit_cards_.swap(credit_cards);
    std::unique_ptr<WDTypedResult> result =
        std::make_unique<WDResult<std::vector<std::unique_ptr<CreditCard>>>>(
            AUTOFILL_CREDITCARDS_RESULT, std::move(credit_cards));
    OnWebDataServiceRequestDone(pending_server_creditcards_query_,
                                std::move(result));
  }
}

void TestPaymentsDataManager::LoadCreditCardCloudTokenData() {
  pending_server_creditcard_cloud_token_data_query_ = 127;
  {
    std::vector<std::unique_ptr<CreditCardCloudTokenData>> cloud_token_data;
    server_credit_card_cloud_token_data_.swap(cloud_token_data);
    std::unique_ptr<WDTypedResult> result = std::make_unique<
        WDResult<std::vector<std::unique_ptr<CreditCardCloudTokenData>>>>(
        AUTOFILL_CLOUDTOKEN_RESULT, std::move(cloud_token_data));
    OnWebDataServiceRequestDone(
        pending_server_creditcard_cloud_token_data_query_, std::move(result));
  }
}

void TestPaymentsDataManager::LoadIbans() {
  pending_local_ibans_query_ = 128;
  pending_server_ibans_query_ = 129;
  {
    std::vector<std::unique_ptr<Iban>> ibans;
    local_ibans_.swap(ibans);
    std::unique_ptr<WDTypedResult> result =
        std::make_unique<WDResult<std::vector<std::unique_ptr<Iban>>>>(
            AUTOFILL_IBANS_RESULT, std::move(ibans));
    OnWebDataServiceRequestDone(pending_local_ibans_query_, std::move(result));
  }
  {
    std::vector<std::unique_ptr<Iban>> server_ibans;
    server_ibans_.swap(server_ibans);
    std::unique_ptr<WDTypedResult> result =
        std::make_unique<WDResult<std::vector<std::unique_ptr<Iban>>>>(
            AUTOFILL_IBANS_RESULT, std::move(server_ibans));
    OnWebDataServiceRequestDone(pending_server_ibans_query_, std::move(result));
  }
}

void TestPaymentsDataManager::RemoveByGUID(const std::string& guid) {
  if (const CreditCard* credit_card = GetCreditCardByGUID(guid)) {
    local_credit_cards_.erase(std::ranges::find(
        local_credit_cards_, credit_card, &std::unique_ptr<CreditCard>::get));
    NotifyObservers();
  } else if (const Iban* iban = GetIbanByGUID(guid)) {
    local_ibans_.erase(
        std::ranges::find(local_ibans_, iban, &std::unique_ptr<Iban>::get));
    NotifyObservers();
  }
}

void TestPaymentsDataManager::RecordUseOfCard(const CreditCard& card) {
  if (CreditCard* credit_card = GetMutableCreditCardByGUID(card.guid())) {
    credit_card->RecordAndLogUse();
  }
}

void TestPaymentsDataManager::RecordUseOfIban(Iban& iban) {
  std::unique_ptr<Iban> updated_iban = std::make_unique<Iban>(iban);
  std::vector<std::unique_ptr<Iban>>& container =
      iban.record_type() == Iban::kLocalIban ? local_ibans_ : server_ibans_;
  auto it = std::ranges::find(container,
                              iban.record_type() == Iban::kLocalIban
                                  ? GetIbanByGUID(iban.guid())
                                  : GetIbanByInstrumentId(iban.instrument_id()),
                              &std::unique_ptr<Iban>::get);
  if (it != container.end()) {
    it->get()->RecordAndLogUse();
  }
}

void TestPaymentsDataManager::AddCreditCard(const CreditCard& credit_card) {
  std::unique_ptr<CreditCard> local_credit_card =
      std::make_unique<CreditCard>(credit_card);
  local_credit_cards_.push_back(std::move(local_credit_card));
  NotifyObservers();
}

std::string TestPaymentsDataManager::AddAsLocalIban(Iban iban) {
  CHECK_EQ(iban.record_type(), Iban::kUnknown);
  iban.set_record_type(Iban::kLocalIban);
  iban.set_identifier(
      Iban::Guid(base::Uuid::GenerateRandomV4().AsLowercaseString()));
  std::unique_ptr<Iban> local_iban = std::make_unique<Iban>(iban);
  local_ibans_.push_back(std::move(local_iban));
  NotifyObservers();
  return iban.guid();
}

std::string TestPaymentsDataManager::UpdateIban(const Iban& iban) {
  const Iban* old_iban = GetIbanByGUID(iban.guid());
  CHECK(old_iban);
  local_ibans_.push_back(std::make_unique<Iban>(iban));
  RemoveByGUID(iban.guid());
  return iban.guid();
}

void TestPaymentsDataManager::DeleteLocalCreditCards(
    const std::vector<CreditCard>& cards) {
  for (const auto& card : cards) {
    // Removed the cards silently and trigger a single notification to match the
    // behavior of PersonalDataManager.
    RemoveCardWithoutNotification(card);
  }
  NotifyObservers();
}

void TestPaymentsDataManager::UpdateCreditCard(const CreditCard& credit_card) {
  if (GetCreditCardByGUID(credit_card.guid())) {
    // AddCreditCard will trigger a notification to observers. We remove the old
    // card without notification so that exactly one notification is sent, which
    // matches the behavior of the PersonalDataManager.
    RemoveCardWithoutNotification(credit_card);
    AddCreditCard(credit_card);
  }
}

void TestPaymentsDataManager::AddServerCvc(int64_t instrument_id,
                                           const std::u16string& cvc) {
  auto card_iterator =
      std::find_if(server_credit_cards_.begin(), server_credit_cards_.end(),
                   [instrument_id](auto& card) {
                     return card->instrument_id() == instrument_id;
                   });

  if (card_iterator != server_credit_cards_.end()) {
    card_iterator->get()->set_cvc(cvc);
  }
}

std::string TestPaymentsDataManager::SaveImportedCreditCard(
    const CreditCard& imported_credit_card) {
  AddCreditCard(imported_credit_card);
  return imported_credit_card.guid();
}

void TestPaymentsDataManager::ClearServerCvcs() {
  for (const std::unique_ptr<CreditCard>& card : server_credit_cards_) {
    if (!card->cvc().empty()) {
      card->clear_cvc();
    }
  }
}

void TestPaymentsDataManager::ClearLocalCvcs() {
  for (const std::unique_ptr<CreditCard>& card : local_credit_cards_) {
    if (!card->cvc().empty()) {
      card->clear_cvc();
    }
  }
}

bool TestPaymentsDataManager::IsAutofillPaymentMethodsEnabled() const {
  // Return the value of autofill_payment_methods_enabled_ if it has been set,
  // otherwise fall back to the normal behavior of checking the pref_service.
  if (autofill_payment_methods_enabled_.has_value()) {
    return autofill_payment_methods_enabled_.value();
  }
  return PaymentsDataManager::IsAutofillPaymentMethodsEnabled();
}

bool TestPaymentsDataManager::IsAutofillWalletImportEnabled() const {
  // Return the value of autofill_wallet_import_enabled_ if it has been set,
  // otherwise fall back to the normal behavior of checking the pref_service.
  if (autofill_wallet_import_enabled_.has_value()) {
    return autofill_wallet_import_enabled_.value();
  }
  return PaymentsDataManager::IsAutofillWalletImportEnabled();
}

bool TestPaymentsDataManager::IsPaymentsWalletSyncTransportEnabled() const {
  if (payments_wallet_sync_transport_enabled_.has_value()) {
    return *payments_wallet_sync_transport_enabled_;
  }
  return PaymentsDataManager::IsPaymentsWalletSyncTransportEnabled();
}

bool TestPaymentsDataManager::ShouldSuggestServerPaymentMethods() const {
  return IsAutofillPaymentMethodsEnabled() && IsAutofillWalletImportEnabled();
}

bool TestPaymentsDataManager::IsPaymentMethodsMandatoryReauthEnabled() {
  if (payment_methods_mandatory_reauth_enabled_.has_value()) {
    return payment_methods_mandatory_reauth_enabled_.value();
  }
  return PaymentsDataManager::IsPaymentMethodsMandatoryReauthEnabled();
}

void TestPaymentsDataManager::SetPaymentMethodsMandatoryReauthEnabled(
    bool enabled) {
  payment_methods_mandatory_reauth_enabled_ = enabled;
  PaymentsDataManager::SetPaymentMethodsMandatoryReauthEnabled(enabled);
}

bool TestPaymentsDataManager::IsPaymentCvcStorageEnabled() {
  if (payments_cvc_storage_enabled_.has_value()) {
    return payments_cvc_storage_enabled_.value();
  }
  return PaymentsDataManager::IsPaymentCvcStorageEnabled();
}

bool TestPaymentsDataManager::IsSyncFeatureEnabledForPaymentsServerMetrics()
    const {
  return false;
}

CoreAccountInfo TestPaymentsDataManager::GetAccountInfoForPaymentsServer()
    const {
  return account_info_;
}

void TestPaymentsDataManager::ClearCreditCards() {
  local_credit_cards_.clear();
  server_credit_cards_.clear();
}

void TestPaymentsDataManager::ClearCreditCardOfferData() {
  autofill_offer_data_.clear();
}

void TestPaymentsDataManager::ClearAllLocalData() {
  local_credit_cards_.clear();
  local_ibans_.clear();
}

void TestPaymentsDataManager::AddServerCreditCard(
    const CreditCard& credit_card) {
  std::unique_ptr<CreditCard> server_credit_card =
      std::make_unique<CreditCard>(credit_card);
  server_credit_cards_.push_back(std::move(server_credit_card));
  NotifyObservers();
}

void TestPaymentsDataManager::AddBnplIssuer(const BnplIssuer& bnpl_issuer) {
  // No duplicated issuer should be inserted into the BNPL issuer list.
  CHECK(std::ranges::none_of(
      linked_bnpl_issuers_, [&](const BnplIssuer& saved_bnpl_issuer) {
        return saved_bnpl_issuer.issuer_id() == bnpl_issuer.issuer_id();
      }));
  CHECK(std::ranges::none_of(
      unlinked_bnpl_issuers_, [&](const BnplIssuer& saved_bnpl_issuer) {
        return saved_bnpl_issuer.issuer_id() == bnpl_issuer.issuer_id();
      }));

  if (bnpl_issuer.payment_instrument().has_value()) {
    linked_bnpl_issuers_.push_back(bnpl_issuer);
  } else {
    unlinked_bnpl_issuers_.push_back(bnpl_issuer);
  }
  NotifyObservers();
}

void TestPaymentsDataManager::ClearBnplIssuers() {
  linked_bnpl_issuers_.clear();
  unlinked_bnpl_issuers_.clear();
}

void TestPaymentsDataManager::AddAutofillOfferData(
    const AutofillOfferData& offer_data) {
  std::unique_ptr<AutofillOfferData> data =
      std::make_unique<AutofillOfferData>(offer_data);
  autofill_offer_data_.emplace_back(std::move(data));
  NotifyObservers();
}

void TestPaymentsDataManager::AddServerIban(const Iban& iban) {
  CHECK(iban.value().empty());
  server_ibans_.push_back(std::make_unique<Iban>(iban));
  NotifyObservers();
}

void TestPaymentsDataManager::CacheImage(const GURL& url,
                                         const gfx::Image& image) {
  owned_image_fetcher_->CacheImage(url, image);
  NotifyObservers();
}

void TestPaymentsDataManager::AddVirtualCardUsageData(
    const VirtualCardUsageData& usage_data) {
  autofill_virtual_card_usage_data_.push_back(usage_data);
  NotifyObservers();
}

void TestPaymentsDataManager::SetNicknameForCardWithGUID(
    std::string_view guid,
    std::string_view nickname) {
  for (auto& card : local_credit_cards_) {
    if (card->guid() == guid) {
      card->SetNickname(base::ASCIIToUTF16(nickname));
    }
  }
  for (auto& card : server_credit_cards_) {
    if (card->guid() == guid) {
      card->SetNickname(base::ASCIIToUTF16(nickname));
    }
  }
  NotifyObservers();
}

void TestPaymentsDataManager::RemoveCardWithoutNotification(
    const CreditCard& card) {
  if (auto it = std::ranges::find(local_credit_cards_, card.guid(),
                                  &CreditCard::guid);
      it != local_credit_cards_.end()) {
    local_credit_cards_.erase(it);
  }
}

}  // namespace autofill
