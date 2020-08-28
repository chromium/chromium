// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Majority of the methods forward the calls to the |delegate_|, except for a
// few methods that invoke the secure payment confirmation dialog:
//  - ShowDialog(request)
//  - CloseDialog()
//  - ShowProcessingSpinner()
//
// A few methods are no-op because secure payment confirmation does not support
// such features as showing payment handler web page, retry, shipping address,
// and credit card CVC number.

#include "components/payments/content/secure_payment_confirmation_payment_request_delegate.h"

#include <utility>

#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_request.h"

namespace payments {

SecurePaymentConfirmationPaymentRequestDelegate::
    SecurePaymentConfirmationPaymentRequestDelegate(
        std::unique_ptr<ContentPaymentRequestDelegate> delegate)
    : delegate_(std::move(delegate)) {}

SecurePaymentConfirmationPaymentRequestDelegate::
    ~SecurePaymentConfirmationPaymentRequestDelegate() = default;

std::unique_ptr<autofill::InternalAuthenticator>
SecurePaymentConfirmationPaymentRequestDelegate::CreateInternalAuthenticator(
    content::RenderFrameHost* rfh) const {
  return delegate_->CreateInternalAuthenticator(rfh);
}

scoped_refptr<PaymentManifestWebDataService>
SecurePaymentConfirmationPaymentRequestDelegate::
    GetPaymentManifestWebDataService() const {
  return delegate_->GetPaymentManifestWebDataService();
}

PaymentRequestDisplayManager*
SecurePaymentConfirmationPaymentRequestDelegate::GetDisplayManager() {
  return delegate_->GetDisplayManager();
}

void SecurePaymentConfirmationPaymentRequestDelegate::EmbedPaymentHandlerWindow(
    const GURL& url,
    PaymentHandlerOpenWindowCallback callback) {
  // No payment handler windows allowed with Secure Payment Confirmation dialog.
  std::move(callback).Run(/*success=*/false, /*render_process_id=*/0,
                          /*render_frame_id=*/0);
}

bool SecurePaymentConfirmationPaymentRequestDelegate::IsInteractive() const {
  return delegate_->IsInteractive();
}

std::string SecurePaymentConfirmationPaymentRequestDelegate::
    GetInvalidSslCertificateErrorMessage() {
  return delegate_->GetInvalidSslCertificateErrorMessage();
}

bool SecurePaymentConfirmationPaymentRequestDelegate::SkipUiForBasicCard()
    const {
  return delegate_->SkipUiForBasicCard();
}

std::string SecurePaymentConfirmationPaymentRequestDelegate::GetTwaPackageName()
    const {
  return delegate_->GetTwaPackageName();
}

void SecurePaymentConfirmationPaymentRequestDelegate::ShowDialog(
    PaymentRequest* request) {
  ui_controller_.ShowDialog(request->GetWeakPtr());
}

void SecurePaymentConfirmationPaymentRequestDelegate::RetryDialog() {
  NOTREACHED();
}

void SecurePaymentConfirmationPaymentRequestDelegate::CloseDialog() {
  ui_controller_.CloseDialog();
}

void SecurePaymentConfirmationPaymentRequestDelegate::ShowErrorMessage() {
  // No-op.
}

void SecurePaymentConfirmationPaymentRequestDelegate::ShowProcessingSpinner() {
  ui_controller_.ShowProcessingSpinner();
}

bool SecurePaymentConfirmationPaymentRequestDelegate::IsBrowserWindowActive()
    const {
  return delegate_->IsBrowserWindowActive();
}

autofill::PersonalDataManager*
SecurePaymentConfirmationPaymentRequestDelegate::GetPersonalDataManager() {
  return delegate_->GetPersonalDataManager();
}

const std::string&
SecurePaymentConfirmationPaymentRequestDelegate::GetApplicationLocale() const {
  return delegate_->GetApplicationLocale();
}

bool SecurePaymentConfirmationPaymentRequestDelegate::IsOffTheRecord() const {
  return delegate_->IsOffTheRecord();
}

const GURL&
SecurePaymentConfirmationPaymentRequestDelegate::GetLastCommittedURL() const {
  return delegate_->GetLastCommittedURL();
}

void SecurePaymentConfirmationPaymentRequestDelegate::DoFullCardRequest(
    const autofill::CreditCard& credit_card,
    base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
        result_delegate) {
  NOTREACHED();
}

autofill::AddressNormalizer*
SecurePaymentConfirmationPaymentRequestDelegate::GetAddressNormalizer() {
  NOTREACHED();
  return nullptr;
}

autofill::RegionDataLoader*
SecurePaymentConfirmationPaymentRequestDelegate::GetRegionDataLoader() {
  NOTREACHED();
  return nullptr;
}

ukm::UkmRecorder*
SecurePaymentConfirmationPaymentRequestDelegate::GetUkmRecorder() {
  return delegate_->GetUkmRecorder();
}

std::string
SecurePaymentConfirmationPaymentRequestDelegate::GetAuthenticatedEmail() const {
  NOTREACHED();
  return "";
}

PrefService* SecurePaymentConfirmationPaymentRequestDelegate::GetPrefService() {
  return delegate_->GetPrefService();
}

}  // namespace payments
