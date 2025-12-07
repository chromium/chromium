// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_APP_FACTORY_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_APP_FACTORY_H_

#include <map>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_app_factory.h"
#include "components/payments/content/secure_payment_confirmation_credential_finder.h"

namespace payments {

class BrowserBoundKeyStore;
struct SecurePaymentConfirmationCredential;

class SecurePaymentConfirmationAppFactory : public PaymentAppFactory {
 public:
  SecurePaymentConfirmationAppFactory();
  ~SecurePaymentConfirmationAppFactory() override;

  SecurePaymentConfirmationAppFactory(
      const SecurePaymentConfirmationAppFactory& other) = delete;
  SecurePaymentConfirmationAppFactory& operator=(
      const SecurePaymentConfirmationAppFactory& other) = delete;

  // PaymentAppFactory:
  void Create(base::WeakPtr<Delegate> delegate) override;

  void SetBrowserBoundKeyStoreForTesting(
      scoped_refptr<BrowserBoundKeyStore> key_store);

  void SetCredentialFinderForTesting(
      std::unique_ptr<SecurePaymentConfirmationCredentialFinder>
          credential_finder);

 private:
  struct Request;

  void OnIsUserVerifyingPlatformAuthenticatorAvailable(
      std::unique_ptr<Request> request,
      bool is_available);

  void OnRetrievedCredentials(
      std::unique_ptr<Request> request,
      std::optional<
          std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>>
          credentials);

  void OnRetrievedBrowserBoundKeyId(
      std::unique_ptr<Request> request,
      std::optional<std::vector<uint8_t>> maybe_browser_bound_key_id);

  // Called once all icons are downloaded and their respective SkBitmaps have
  // been set into the Request.
  void DidDownloadAllIcons(std::unique_ptr<Request> request);

  scoped_refptr<BrowserBoundKeyStore> browser_bound_key_store_for_testing_;

  std::unique_ptr<SecurePaymentConfirmationCredentialFinder> credential_finder_;

  base::WeakPtrFactory<SecurePaymentConfirmationAppFactory> weak_ptr_factory_{
      this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_APP_FACTORY_H_
