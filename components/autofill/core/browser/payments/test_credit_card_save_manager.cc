// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"

#include "components/autofill/core/browser/payments/test_payments_network_interface.h"

namespace autofill {

TestCreditCardSaveManager::TestCreditCardSaveManager(
    AutofillDriver* driver,
    AutofillClient* client,
    payments::TestPaymentsNetworkInterface* payments_network_interface,
    PersonalDataManager* personal_data_manager)
    : CreditCardSaveManager(client,
                            payments_network_interface,
                            "en-US",
                            personal_data_manager) {}

TestCreditCardSaveManager::~TestCreditCardSaveManager() {}

bool TestCreditCardSaveManager::IsCreditCardUploadEnabled() {
  return credit_card_upload_enabled_;
}

void TestCreditCardSaveManager::SetCreditCardUploadEnabled(
    bool credit_card_upload_enabled) {
  credit_card_upload_enabled_ = credit_card_upload_enabled;
}

bool TestCreditCardSaveManager::CreditCardWasUploaded() {
  return credit_card_was_uploaded_;
}

bool TestCreditCardSaveManager::CvcLocalSaveStarted() {
  return cvc_local_save_started_;
}

bool TestCreditCardSaveManager::AttemptToOfferCvcLocalSave(
    const CreditCard& card) {
  cvc_local_save_started_ = true;
  return CreditCardSaveManager::AttemptToOfferCvcLocalSave(card);
}

bool TestCreditCardSaveManager::CvcUploadSaveStarted() {
  return cvc_upload_save_started_;
}

void TestCreditCardSaveManager::AttemptToOfferCvcUploadSave(
    const CreditCard& card) {
  cvc_upload_save_started_ = true;
  CreditCardSaveManager::AttemptToOfferCvcUploadSave(card);
}

bool TestCreditCardSaveManager::CardLocalSaveStarted() {
  return card_local_save_started_;
}

bool TestCreditCardSaveManager::AttemptToOfferCardLocalSave(
    const CreditCard& card) {
  card_local_save_started_ = true;
  return CreditCardSaveManager::AttemptToOfferCardLocalSave(card);
}

void TestCreditCardSaveManager::set_show_save_prompt(bool show_save_prompt) {
  show_save_prompt_ = show_save_prompt;
}

void TestCreditCardSaveManager::set_upload_request_card_number(
    const std::u16string& credit_card_number) {
  upload_request_.card.SetNumber(credit_card_number);
}

void TestCreditCardSaveManager::set_upload_request_card(
    const CreditCard& card) {
  upload_request_.card = std::move(card);
}

payments::PaymentsNetworkInterface::UploadRequestDetails*
TestCreditCardSaveManager::upload_request() {
  return &upload_request_;
}

void TestCreditCardSaveManager::OnDidUploadCard(
    AutofillClient::PaymentsRpcResult result,
    const payments::PaymentsNetworkInterface::UploadCardResponseDetails&
        upload_card_response_details) {
  credit_card_was_uploaded_ = true;
  CreditCardSaveManager::OnDidUploadCard(result, upload_card_response_details);
}

}  // namespace autofill
