// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_MOCK_CONTENT_PAYMENT_REQUEST_DELEGATE_H_
#define COMPONENTS_PAYMENTS_CONTENT_MOCK_CONTENT_PAYMENT_REQUEST_DELEGATE_H_

#include "base/unguessable_token.h"
#include "components/payments/content/content_payment_request_delegate.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/content/payment_ui_observer.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace payments {

// A mock implementation of ContentPaymentRequestDelegate, for testing purposes.
// See also TestContentPaymentRequestDelegate, which provides a slightly more
// 'real' implementation for tests that wish to test deeper interactions.
class MockContentPaymentRequestDelegate : public ContentPaymentRequestDelegate {
 public:
  MockContentPaymentRequestDelegate();
  MockContentPaymentRequestDelegate(const MockContentPaymentRequestDelegate&) =
      delete;
  MockContentPaymentRequestDelegate& operator=(
      const MockContentPaymentRequestDelegate&) = delete;
  ~MockContentPaymentRequestDelegate() override;

  // ContentPaymentRequestDelegate
  MOCK_METHOD(content::RenderFrameHost*,
              GetRenderFrameHost,
              (),
              (override, const));
  MOCK_METHOD(std::unique_ptr<webauthn::InternalAuthenticator>,
              CreateInternalAuthenticator,
              (),
              (override, const));
  MOCK_METHOD(scoped_refptr<PaymentManifestWebDataService>,
              GetPaymentManifestWebDataService,
              (),
              (override, const));
  MOCK_METHOD(PaymentRequestDisplayManager*, GetDisplayManager, (), (override));
  MOCK_METHOD(void,
              EmbedPaymentHandlerWindow,
              (const GURL& url, PaymentHandlerOpenWindowCallback callback),
              (override));
  MOCK_METHOD(bool, IsInteractive, (), (override, const));
  MOCK_METHOD(std::string,
              GetInvalidSslCertificateErrorMessage,
              (),
              (override));
  MOCK_METHOD(void,
              GetTwaPackageName,
              (GetTwaPackageNameCallback callback),
              (override, const));
  MOCK_METHOD(PaymentRequestDialog*, GetDialogForTesting, (), (override));
  MOCK_METHOD(SecurePaymentConfirmationNoCreds*,
              GetNoMatchingCredentialsDialogForTesting,
              (),
              (override));
  MOCK_METHOD(const base::WeakPtr<PaymentUIObserver>,
              GetPaymentUIObserver,
              (),
              (override, const));
  MOCK_METHOD(void,
              ShowNoMatchingPaymentCredentialDialog,
              (const std::u16string& merchant_name,
               const std::string& rp_id,
               base::OnceClosure response_callback,
               base::OnceClosure opt_out_callback),
              (override));
  MOCK_METHOD(std::optional<base::UnguessableToken>,
              GetChromeOSTWAInstanceId,
              (),
              (override, const));

  // PaymentRequestDelegate
  MOCK_METHOD(void,
              ShowDialog,
              (base::WeakPtr<PaymentRequest> request),
              (override));
  MOCK_METHOD(void, RetryDialog, (), (override));
  MOCK_METHOD(void, CloseDialog, (), (override));
  MOCK_METHOD(void, ShowErrorMessage, (), (override));
  MOCK_METHOD(void, ShowProcessingSpinner, (), (override));
  MOCK_METHOD(bool, IsBrowserWindowActive, (), (override, const));

  // PaymentRequestBaseDelegate
  MOCK_METHOD(autofill::PersonalDataManager*,
              GetPersonalDataManager,
              (),
              (override));
  MOCK_METHOD(const std::string&, GetApplicationLocale, (), (override, const));
  MOCK_METHOD(bool, IsOffTheRecord, (), (override, const));
  MOCK_METHOD(const GURL&, GetLastCommittedURL, (), (override, const));
  MOCK_METHOD(autofill::AddressNormalizer*,
              GetAddressNormalizer,
              (),
              (override));
  MOCK_METHOD(autofill::RegionDataLoader*, GetRegionDataLoader, (), (override));
  MOCK_METHOD(ukm::UkmRecorder*, GetUkmRecorder, (), (override));
  MOCK_METHOD(std::string, GetAuthenticatedEmail, (), (override, const));
  MOCK_METHOD(PrefService*, GetPrefService, (), (override));
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_MOCK_CONTENT_PAYMENT_REQUEST_DELEGATE_H_
