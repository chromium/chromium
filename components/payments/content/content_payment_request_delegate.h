// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_CONTENT_PAYMENT_REQUEST_DELEGATE_H_
#define COMPONENTS_PAYMENTS_CONTENT_CONTENT_PAYMENT_REQUEST_DELEGATE_H_

#include <string>

#include "components/payments/content/payment_request_display_manager.h"
#include "components/payments/core/payment_request_delegate.h"

template <class T>
class scoped_refptr;

namespace payments {

class PaymentManifestWebDataService;
class PaymentRequestDisplayManager;

// The delegate for PaymentRequest that can use content.
class ContentPaymentRequestDelegate : public PaymentRequestDelegate {
 public:
  ~ContentPaymentRequestDelegate() override {}

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
  // or an empty string when the SSL certificate is valid. Only EV_SECURE,
  // SECURE, and SECURE_WITH_POLICY_INSTALLED_CERT are considered valid for web
  // payments, unless --ignore-certificate-errors is specified on the command
  // line.
  //
  // The |web_contents| parameter should not be null. A null |web_contents|
  // parameter will return an "Invalid certificate" error message.
  virtual std::string GetInvalidSslCertificateErrorMessage() = 0;

  // Returns whether the UI should be skipped for a "basic-card" scenario. This
  // will only be true in tests.
  virtual bool SkipUiForBasicCard() const = 0;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_CONTENT_PAYMENT_REQUEST_DELEGATE_H_
