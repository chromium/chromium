// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/test_content_payment_request_delegate.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/core/error_strings.h"
#include "content/public/browser/render_frame_host.h"

namespace payments {

TestContentPaymentRequestDelegate::TestContentPaymentRequestDelegate(
    std::unique_ptr<base::SingleThreadTaskExecutor> task_executor,
    autofill::PersonalDataManager* pdm)
    : core_delegate_(std::move(task_executor), pdm) {}

TestContentPaymentRequestDelegate::~TestContentPaymentRequestDelegate() =
    default;

content::RenderFrameHost*
TestContentPaymentRequestDelegate::GetRenderFrameHost() const {
  return content::RenderFrameHost::FromID(frame_routing_id_);
}

std::unique_ptr<webauthn::InternalAuthenticator>
TestContentPaymentRequestDelegate::CreateInternalAuthenticator() const {
  return nullptr;
}

scoped_refptr<PaymentManifestWebDataService>
TestContentPaymentRequestDelegate::GetPaymentManifestWebDataService() const {
  return nullptr;
}

PaymentRequestDisplayManager*
TestContentPaymentRequestDelegate::GetDisplayManager() {
  return &payment_request_display_manager_;
}

void TestContentPaymentRequestDelegate::ShowDialog(
    base::WeakPtr<PaymentRequest> request) {
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

void TestContentPaymentRequestDelegate::GetTwaPackageName(
    GetTwaPackageNameCallback callback) const {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), ""));
}

PaymentRequestDialog* TestContentPaymentRequestDelegate::GetDialogForTesting() {
  return nullptr;
}

SecurePaymentConfirmationNoCreds*
TestContentPaymentRequestDelegate::GetNoMatchingCredentialsDialogForTesting() {
  return nullptr;
}

autofill::PersonalDataManager*
TestContentPaymentRequestDelegate::GetPersonalDataManager() {
  return core_delegate_.GetPersonalDataManager();
}

const std::string& TestContentPaymentRequestDelegate::GetApplicationLocale()
    const {
  return core_delegate_.GetApplicationLocale();
}

bool TestContentPaymentRequestDelegate::IsOffTheRecord() const {
  return core_delegate_.IsOffTheRecord();
}

const GURL& TestContentPaymentRequestDelegate::GetLastCommittedURL() const {
  return core_delegate_.GetLastCommittedURL();
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

const base::WeakPtr<PaymentUIObserver>
TestContentPaymentRequestDelegate::GetPaymentUIObserver() const {
  return nullptr;
}

void TestContentPaymentRequestDelegate::ShowNoMatchingPaymentCredentialDialog(
    const std::u16string& merchant_name,
    const std::string& rp_id,
    base::OnceClosure response_callback,
    base::OnceClosure opt_out_callback) {}

std::optional<base::UnguessableToken>
TestContentPaymentRequestDelegate::GetChromeOSTWAInstanceId() const {
  return std::nullopt;
}

}  // namespace payments
