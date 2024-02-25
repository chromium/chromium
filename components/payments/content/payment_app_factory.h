// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_APP_FACTORY_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_APP_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_app.h"
#include "components/payments/content/service_worker_payment_app_finder.h"
#include "content/public/browser/payment_app_provider.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

class GURL;

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace webauthn {
class InternalAuthenticator;
}  // namespace webauthn

namespace payments {

// Known reasons why an app may fail to be created. Passed to a
// PaymentAppFactory Delegate to allow it to better handle the lack of creation
// of an app, if appropriate.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.payments
enum class AppCreationFailureReason {
  UNKNOWN,
  ICON_DOWNLOAD_FAILED,
};

class ContentPaymentRequestDelegate;
class CSPChecker;
class PaymentManifestWebDataService;
class PaymentRequestSpec;

// Base class for a factory that can create instances of payment apps.
class PaymentAppFactory {
 public:
  class Delegate {
   public:
    using GetTwaPackageNameCallback =
        base::OnceCallback<void(const std::string& twa_package_name)>;

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

    virtual content::GlobalRenderFrameHostId GetInitiatorRenderFrameHostId()
        const = 0;

    virtual const std::vector<mojom::PaymentMethodDataPtr>& GetMethodData()
        const = 0;
    virtual std::unique_ptr<webauthn::InternalAuthenticator>
    CreateInternalAuthenticator() const = 0;
    virtual scoped_refptr<PaymentManifestWebDataService>
    GetPaymentManifestWebDataService() const = 0;
    virtual bool IsOffTheRecord() const = 0;

    // Returns the merchant provided information, or null if the payment is
    // being aborted.
    virtual base::WeakPtr<PaymentRequestSpec> GetSpec() const = 0;

    // Obtains the Android package name of the Trusted Web Activity that invoked
    // this browser, if any. Otherwise, calls `callback` with an empty string.
    virtual void GetTwaPackageName(GetTwaPackageNameCallback callback) = 0;

    // Tells the UI to show the processing spinner. Only desktop UI needs this
    // notification.
    virtual void ShowProcessingSpinner() = 0;

    virtual base::WeakPtr<ContentPaymentRequestDelegate>
    GetPaymentRequestDelegate() const = 0;

    // Called when an app is created.
    virtual void OnPaymentAppCreated(std::unique_ptr<PaymentApp> app) = 0;

    // Called when there is an error creating a payment app. Called when unable
    // to download a web app manifest, for example.
    virtual void OnPaymentAppCreationError(
        const std::string& error_message,
        AppCreationFailureReason failure_reason =
            AppCreationFailureReason::UNKNOWN) = 0;

    // Called when all apps of this factory have been created.
    virtual void OnDoneCreatingPaymentApps() = 0;

    // Make both canMakePayment() and hasEnrolledInstrument() return true,
    // regardless of presence of payment apps. This is used by secure payment
    // confirmation method, which returns true for canMakePayment() and
    // hasEnrolledInstrument() regardless of presence of credentials in user
    // profile or the authenticator device, as long as a user-verifying platform
    // authenticator device is available.
    virtual void SetCanMakePaymentEvenWithoutApps() = 0;

    // Return a Content Security Policy checker that should be used before
    // downloading payment manifests and following their redirects.
    virtual base::WeakPtr<CSPChecker> GetCSPChecker() = 0;

    // Records that an Opt Out experience will be offered to the user in the
    // current UI flow.
    virtual void SetOptOutOffered() = 0;

    // Return the app instance id for the TWA that invokes the payment request.
    // The instance id is used to find the TWA window in the ash so that we can
    // attach the payment dialog to it. This interface should only be used
    // in ChromeOS.
    // At the moment, this interface is only implemented in Lacros and for all
    // other platforms this will return std::nullopt. In addition to that, if
    // for any reason, we failed to find the app instance, this method will
    // also return std::nullopt.
    virtual std::optional<base::UnguessableToken> GetChromeOSTWAInstanceId()
        const = 0;
  };

  explicit PaymentAppFactory(PaymentApp::Type type);

  PaymentAppFactory(const PaymentAppFactory&) = delete;
  PaymentAppFactory& operator=(const PaymentAppFactory&) = delete;

  virtual ~PaymentAppFactory();

  PaymentApp::Type type() const { return type_; }

  virtual void Create(base::WeakPtr<Delegate> delegate) = 0;

 private:
  const PaymentApp::Type type_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_APP_FACTORY_H_
