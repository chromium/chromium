// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_APP_FACTORY_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_APP_FACTORY_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_app.h"
#include "components/payments/content/service_worker_payment_app_finder.h"
#include "content/public/browser/payment_app_provider.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

class GURL;

namespace autofill {
class AutofillProfile;
class InternalAuthenticator;
}  // namespace autofill

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace payments {

class ContentPaymentRequestDelegate;
class PaymentManifestWebDataService;
class PaymentRequestSpec;

// Base class for a factory that can create instances of payment apps.
class PaymentAppFactory {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns the WebContents that initiated the PaymentRequest API, or null if
    // the RenderFrameHost or WebContents has been deleted, which can happen
    // when the page is being closed, for example.
    virtual content::WebContents* GetWebContents() = 0;

    virtual const GURL& GetTopOrigin() = 0;
    virtual const GURL& GetFrameOrigin() = 0;
    virtual const url::Origin& GetFrameSecurityOrigin() = 0;

    // Returns the RenderFrameHost that initiated the PaymentRequest API, or
    // null if the RenderFrameHost has been deleted, which can happen when the
    // RenderFrameHost is being unloaded, for example.
    virtual content::RenderFrameHost* GetInitiatorRenderFrameHost() const = 0;

    virtual const std::vector<mojom::PaymentMethodDataPtr>& GetMethodData()
        const = 0;
    virtual std::unique_ptr<autofill::InternalAuthenticator>
    CreateInternalAuthenticator() const = 0;
    virtual scoped_refptr<PaymentManifestWebDataService>
    GetPaymentManifestWebDataService() const = 0;
    virtual bool MayCrawlForInstallablePaymentApps() = 0;
    virtual bool IsOffTheRecord() const = 0;

    // Returns the merchant provided information, or null if the payment is
    // being aborted.
    virtual base::WeakPtr<PaymentRequestSpec> GetSpec() const = 0;

    // Returns the Android package name of the Trusted Web Activity that invoked
    // this browser, if any. Otherwise, an empty string.
    virtual std::string GetTwaPackageName() const = 0;

    // Tells the UI to show the processing spinner. Only desktop UI needs this
    // notification.
    virtual void ShowProcessingSpinner() = 0;

    // These parameters are only used to create the autofill payment app.
    virtual const std::vector<autofill::AutofillProfile*>&
    GetBillingProfiles() = 0;
    virtual bool IsRequestedAutofillDataAvailable() = 0;
    virtual ContentPaymentRequestDelegate* GetPaymentRequestDelegate()
        const = 0;

    // Called when an app is created.
    virtual void OnPaymentAppCreated(std::unique_ptr<PaymentApp> app) = 0;

    // Called when there is an error creating a payment app. Called when unable
    // to download a web app manifest, for example.
    virtual void OnPaymentAppCreationError(
        const std::string& error_message) = 0;

    // Whether the factory should early exit before creating platform-specific
    // PaymentApp objects. This is used by PaymentAppServiceBridge to skip
    // creating native AutofillPaymentApp, which currently cannot be used over
    // JNI.
    virtual bool SkipCreatingNativePaymentApps() const = 0;

    // Called when all apps of this factory have been created.
    virtual void OnDoneCreatingPaymentApps() = 0;

    // Make both canMakePayment() and hasEnrolledInstrument() return true,
    // regardless of presence of payment apps. This is used by secure payment
    // confirmation method, which returns true for canMakePayment() and
    // hasEnrolledInstrument() regardless of presence of credentials in user
    // profile or the authenticator device, as long as a user-verifying platform
    // authenticator device is available.
    virtual void SetCanMakePaymentEvenWithoutApps() = 0;
  };

  explicit PaymentAppFactory(PaymentApp::Type type);
  virtual ~PaymentAppFactory();

  PaymentApp::Type type() const { return type_; }

  virtual void Create(base::WeakPtr<Delegate> delegate) = 0;

 private:
  const PaymentApp::Type type_;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppFactory);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_APP_FACTORY_H_
