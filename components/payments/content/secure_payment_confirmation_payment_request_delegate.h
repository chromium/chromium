// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_PAYMENT_REQUEST_DELEGATE_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_PAYMENT_REQUEST_DELEGATE_H_

#include <memory>

#include "components/payments/content/content_payment_request_delegate.h"
#include "components/payments/content/secure_payment_confirmation_controller.h"
#include "components/payments/core/payment_request_base_delegate.h"
#include "components/payments/core/payment_request_delegate.h"

namespace payments {

// Wraps a ContentPaymentRequestDelegate to always display the secure payment
// confirmation dialog instead of the standard payment request UI.
class SecurePaymentConfirmationPaymentRequestDelegate
    : public ContentPaymentRequestDelegate {
 public:
  explicit SecurePaymentConfirmationPaymentRequestDelegate(
      std::unique_ptr<ContentPaymentRequestDelegate> delegate);
  ~SecurePaymentConfirmationPaymentRequestDelegate() override;

  SecurePaymentConfirmationPaymentRequestDelegate(
      const SecurePaymentConfirmationPaymentRequestDelegate& other) = delete;
  SecurePaymentConfirmationPaymentRequestDelegate& operator=(
      const SecurePaymentConfirmationPaymentRequestDelegate& other) = delete;

  // ContentPaymentRequestDelegate implementation:
  std::unique_ptr<autofill::InternalAuthenticator> CreateInternalAuthenticator(
      content::RenderFrameHost* rfh) const override;
  scoped_refptr<PaymentManifestWebDataService>
  GetPaymentManifestWebDataService() const override;
  PaymentRequestDisplayManager* GetDisplayManager() override;
  void EmbedPaymentHandlerWindow(
      const GURL& url,
      PaymentHandlerOpenWindowCallback callback) override;
  bool IsInteractive() const override;
  std::string GetInvalidSslCertificateErrorMessage() override;
  bool SkipUiForBasicCard() const override;
  std::string GetTwaPackageName() const override;

  // PaymentRequestDelegate implementation:
  void ShowDialog(PaymentRequest* request) override;
  void RetryDialog() override;
  void CloseDialog() override;
  void ShowErrorMessage() override;
  void ShowProcessingSpinner() override;
  bool IsBrowserWindowActive() const override;

  // PaymentRequestBaseDelegate implementation:
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  const std::string& GetApplicationLocale() const override;
  bool IsOffTheRecord() const override;
  const GURL& GetLastCommittedURL() const override;
  void DoFullCardRequest(
      const autofill::CreditCard& credit_card,
      base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
          result_delegate) override;
  autofill::AddressNormalizer* GetAddressNormalizer() override;
  autofill::RegionDataLoader* GetRegionDataLoader() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  std::string GetAuthenticatedEmail() const override;
  PrefService* GetPrefService() override;

 private:
  // Majority of the calls are forwarded to this delegate.
  std::unique_ptr<ContentPaymentRequestDelegate> delegate_;

  // Displays the secure payment confirmation dialog UI.
  SecurePaymentConfirmationController ui_controller_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_PAYMENT_REQUEST_DELEGATE_H_
