// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_TEST_CONTENT_PAYMENT_REQUEST_DELEGATE_H_
#define COMPONENTS_PAYMENTS_CONTENT_TEST_CONTENT_PAYMENT_REQUEST_DELEGATE_H_

#include <memory>

#include "components/payments/content/content_payment_request_delegate.h"
#include "components/payments/content/payment_request_display_manager.h"
#include "components/payments/core/test_payment_request_delegate.h"

namespace autofill {
class PersonalDataManager;
}  // namespace autofill

namespace base {
class SingleThreadTaskExecutor;
}  // namespace base

namespace payments {

class TestContentPaymentRequestDelegate : public ContentPaymentRequestDelegate {
 public:
  TestContentPaymentRequestDelegate(
      std::unique_ptr<base::SingleThreadTaskExecutor> task_executor,
      autofill::PersonalDataManager* pdm);
  ~TestContentPaymentRequestDelegate() override;

  // ContentPaymentRequestDelegate:
  std::unique_ptr<autofill::InternalAuthenticator> CreateInternalAuthenticator()
      const override;
  scoped_refptr<PaymentManifestWebDataService>
  GetPaymentManifestWebDataService() const override;
  PaymentRequestDisplayManager* GetDisplayManager() override;
  void ShowDialog(base::WeakPtr<PaymentRequest> request) override;
  void RetryDialog() override;
  void CloseDialog() override;
  void ShowErrorMessage() override;
  void ShowProcessingSpinner() override;
  bool IsBrowserWindowActive() const override;
  bool SkipUiForBasicCard() const override;
  std::string GetTwaPackageName() const override;
  PaymentRequestDialog* GetDialogForTesting() override;
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
  void EmbedPaymentHandlerWindow(
      const GURL& url,
      PaymentHandlerOpenWindowCallback callback) override;
  bool IsInteractive() const override;
  std::string GetInvalidSslCertificateErrorMessage() override;

  autofill::TestAddressNormalizer* test_address_normalizer();
  void DelayFullCardRequestCompletion();
  void CompleteFullCardRequest();

 private:
  TestPaymentRequestDelegate core_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestContentPaymentRequestDelegate);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_TEST_CONTENT_PAYMENT_REQUEST_DELEGATE_H_
