// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_TEST_CONTENT_PAYMENT_REQUEST_DELEGATE_H_
#define COMPONENTS_PAYMENTS_CONTENT_TEST_CONTENT_PAYMENT_REQUEST_DELEGATE_H_

#include <memory>

#include "components/payments/content/content_payment_request_delegate.h"
#include "components/payments/content/payment_request_display_manager.h"
#include "components/payments/core/test_payment_request_delegate.h"
#include "content/public/browser/global_routing_id.h"

namespace autofill {
class PersonalDataManager;
}  // namespace autofill

namespace base {
class SingleThreadTaskExecutor;
}  // namespace base

namespace payments {

class PaymentUIObserver;

class TestContentPaymentRequestDelegate : public ContentPaymentRequestDelegate {
 public:
  TestContentPaymentRequestDelegate(
      std::unique_ptr<base::SingleThreadTaskExecutor> task_executor,
      autofill::PersonalDataManager* pdm);

  TestContentPaymentRequestDelegate(const TestContentPaymentRequestDelegate&) =
      delete;
  TestContentPaymentRequestDelegate& operator=(
      const TestContentPaymentRequestDelegate&) = delete;

  ~TestContentPaymentRequestDelegate() override;

  // ContentPaymentRequestDelegate:
  content::RenderFrameHost* GetRenderFrameHost() const override;
  std::unique_ptr<webauthn::InternalAuthenticator> CreateInternalAuthenticator()
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
  void GetTwaPackageName(GetTwaPackageNameCallback callback) const override;
  PaymentRequestDialog* GetDialogForTesting() override;
  SecurePaymentConfirmationNoCreds* GetNoMatchingCredentialsDialogForTesting()
      override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  const std::string& GetApplicationLocale() const override;
  bool IsOffTheRecord() const override;
  const GURL& GetLastCommittedURL() const override;
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
  const base::WeakPtr<PaymentUIObserver> GetPaymentUIObserver() const override;
  void ShowNoMatchingPaymentCredentialDialog(
      const std::u16string& merchant_name,
      const std::string& rp_id,
      base::OnceClosure response_callback,
      base::OnceClosure opt_out_callback) override;
  std::optional<base::UnguessableToken> GetChromeOSTWAInstanceId()
      const override;

  // Must be called if GetRenderFrameHost() needs to return non-null.
  void set_frame_routing_id(content::GlobalRenderFrameHostId frame_routing_id) {
    frame_routing_id_ = frame_routing_id;
  }

 private:
  TestPaymentRequestDelegate core_delegate_;
  PaymentRequestDisplayManager payment_request_display_manager_;
  content::GlobalRenderFrameHostId frame_routing_id_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_TEST_CONTENT_PAYMENT_REQUEST_DELEGATE_H_
