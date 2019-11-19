// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/test_content_payment_request_delegate.h"

#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/core/error_strings.h"

namespace payments {

TestContentPaymentRequestDelegate::TestContentPaymentRequestDelegate(
    autofill::PersonalDataManager* pdm)
    : core_delegate_(pdm) {}

TestContentPaymentRequestDelegate::~TestContentPaymentRequestDelegate() {}

scoped_refptr<PaymentManifestWebDataService>
TestContentPaymentRequestDelegate::GetPaymentManifestWebDataService() const {
  return nullptr;
}

PaymentRequestDisplayManager*
TestContentPaymentRequestDelegate::GetDisplayManager() {
  return nullptr;
}

void TestContentPaymentRequestDelegate::ShowDialog(PaymentRequest* request) {
  core_delegate_.ShowDialog(request);
}

void TestContentPaymentRequestDelegate::RetryDialog() {
  core_delegate_.RetryDialog();
}

void TestContentPaymentRequestDelegate::CloseDialog() {
  core_delegate_.CloseDialog();
}

void TestContentPaymentRequestDelegate::ShowErrorMessage() {
  core_delegate_.ShowErrorMessage();
}

void TestContentPaymentRequestDelegate::ShowProcessingSpinner() {
  core_delegate_.ShowProcessingSpinner();
}

bool TestContentPaymentRequestDelegate::IsBrowserWindowActive() const {
  return core_delegate_.IsBrowserWindowActive();
}

bool TestContentPaymentRequestDelegate::SkipUiForBasicCard() const {
  return false;
}

autofill::PersonalDataManager*
TestContentPaymentRequestDelegate::GetPersonalDataManager() {
  return core_delegate_.GetPersonalDataManager();
}

const std::string& TestContentPaymentRequestDelegate::GetApplicationLocale()
    const {
  return core_delegate_.GetApplicationLocale();
}

bool TestContentPaymentRequestDelegate::IsIncognito() const {
  return core_delegate_.IsIncognito();
}

const GURL& TestContentPaymentRequestDelegate::GetLastCommittedURL() const {
  return core_delegate_.GetLastCommittedURL();
}

void TestContentPaymentRequestDelegate::DoFullCardRequest(
    const autofill::CreditCard& credit_card,
    base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
        result_delegate) {
  return core_delegate_.DoFullCardRequest(credit_card, result_delegate);
}

autofill::AddressNormalizer*
TestContentPaymentRequestDelegate::GetAddressNormalizer() {
  return core_delegate_.GetAddressNormalizer();
}

autofill::RegionDataLoader*
TestContentPaymentRequestDelegate::GetRegionDataLoader() {
  return core_delegate_.GetRegionDataLoader();
}

ukm::UkmRecorder* TestContentPaymentRequestDelegate::GetUkmRecorder() {
  return core_delegate_.GetUkmRecorder();
}

std::string TestContentPaymentRequestDelegate::GetAuthenticatedEmail() const {
  return core_delegate_.GetAuthenticatedEmail();
}

PrefService* TestContentPaymentRequestDelegate::GetPrefService() {
  return core_delegate_.GetPrefService();
}

void TestContentPaymentRequestDelegate::EmbedPaymentHandlerWindow(
    const GURL& url,
    PaymentHandlerOpenWindowCallback callback) {}

bool TestContentPaymentRequestDelegate::IsInteractive() const {
  return true;
}

std::string
TestContentPaymentRequestDelegate::GetInvalidSslCertificateErrorMessage() {
  return "";  // Empty string indicates valid SSL certificate.
}

autofill::TestAddressNormalizer*
TestContentPaymentRequestDelegate::test_address_normalizer() {
  return core_delegate_.test_address_normalizer();
}

void TestContentPaymentRequestDelegate::DelayFullCardRequestCompletion() {
  core_delegate_.DelayFullCardRequestCompletion();
}

void TestContentPaymentRequestDelegate::CompleteFullCardRequest() {
  core_delegate_.CompleteFullCardRequest();
}

}  // namespace payments
