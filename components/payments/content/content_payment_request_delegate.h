// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_CONTENT_PAYMENT_REQUEST_DELEGATE_H_
#define COMPONENTS_PAYMENTS_CONTENT_CONTENT_PAYMENT_REQUEST_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "components/payments/content/payment_request_display_manager.h"
#include "components/payments/core/payment_request_delegate.h"

template <class T>
class scoped_refptr;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace webauthn {
class InternalAuthenticator;
}  // namespace webauthn

namespace payments {

class PaymentManifestWebDataService;
class PaymentRequestDialog;
class PaymentRequestDisplayManager;
class PaymentUIObserver;
class SecurePaymentConfirmationNoCreds;

// The delegate for PaymentRequest that can use content.
class ContentPaymentRequestDelegate : public PaymentRequestDelegate {
 public:
  using GetTwaPackageNameCallback =
      base::OnceCallback<void(const std::string& twa_package_name)>;

  ~ContentPaymentRequestDelegate() override;

  // Returns the RenderFrameHost for the frame that initiated the
  // PaymentRequest.
  virtual content::RenderFrameHost* GetRenderFrameHost() const = 0;

  // Creates and returns an instance of the InternalAuthenticator interface for
  // communication with WebAuthn.
  virtual std::unique_ptr<webauthn::InternalAuthenticator>
  CreateInternalAuthenticator() const = 0;

  // Returns the web data service for caching payment method manifests.
  virtual scoped_refptr<PaymentManifestWebDataService>
  GetPaymentManifestWebDataService() const = 0;

  // Returns the PaymentRequestDisplayManager associated with this
  // PaymentRequest's BrowserContext.
  virtual PaymentRequestDisplayManager* GetDisplayManager() = 0;

  // Embed the content of the web page at |url| passed through
  // PaymentRequestEvent.openWindow inside the current Payment Request UI
  // surface. |callback| is invoked after navigation is completed, passing
  // true/false to indicate success/failure.
  virtual void EmbedPaymentHandlerWindow(
      const GURL& url,
      PaymentHandlerOpenWindowCallback callback) = 0;

  // Returns whether user interaction is enabled. (False when showing a
  // spinner.)
  virtual bool IsInteractive() const = 0;

  // Returns a developer-facing error message for invalid SSL certificate state
  // or an empty string when the SSL certificate is valid. Only SECURE and
  // SECURE_WITH_POLICY_INSTALLED_CERT are considered valid for web payments,
  // unless --ignore-certificate-errors is specified on the command line.
  //
  // The |web_contents| parameter should not be null. A null |web_contents|
  // parameter will return an "Invalid certificate" error message.
  virtual std::string GetInvalidSslCertificateErrorMessage() = 0;

  // Obtains the Android package name of the Trusted Web Activity that invoked
  // this browser, if any. Otherwise, calls `callback` with an empty string.
  virtual void GetTwaPackageName(GetTwaPackageNameCallback callback) const = 0;

  virtual PaymentRequestDialog* GetDialogForTesting() = 0;
  virtual SecurePaymentConfirmationNoCreds*
  GetNoMatchingCredentialsDialogForTesting() = 0;

  virtual const base::WeakPtr<PaymentUIObserver> GetPaymentUIObserver()
      const = 0;

  virtual void ShowNoMatchingPaymentCredentialDialog(
      const std::u16string& merchant_name,
      const std::string& rp_id,
      base::OnceClosure response_callback,
      base::OnceClosure opt_out_callback) = 0;

  // Returns an instance id for the TWA that invokes the payment app. The
  // instance id is used to find the TWA window in the ash so that we can
  // attach the payment dialog to it. This interface should only be used
  // in ChromeOS.
  virtual std::optional<base::UnguessableToken> GetChromeOSTWAInstanceId()
      const = 0;

  // Returns a weak pointer to this delegate.
  base::WeakPtr<ContentPaymentRequestDelegate> GetContentWeakPtr();

 protected:
  ContentPaymentRequestDelegate();

 private:
  base::WeakPtrFactory<ContentPaymentRequestDelegate> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_CONTENT_PAYMENT_REQUEST_DELEGATE_H_
