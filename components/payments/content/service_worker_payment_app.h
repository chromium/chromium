// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SERVICE_WORKER_PAYMENT_APP_H_
#define COMPONENTS_PAYMENTS_CONTENT_SERVICE_WORKER_PAYMENT_APP_H_

#include <stdint.h>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_app.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/web_app_manifest.h"
#include "content/public/browser/stored_payment_app.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_handler_host.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace content {
class PaymentAppProvider;
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace payments {

class PaymentHandlerHost;

// Represents a service worker based payment app.
class ServiceWorkerPaymentApp : public PaymentApp {
 public:
  // This constructor is used for a payment app that has been installed in
  // Chrome. The `spec` parameter should not be null.
  ServiceWorkerPaymentApp(
      content::WebContents* web_contents,
      const GURL& top_origin,
      const GURL& frame_origin,
      base::WeakPtr<PaymentRequestSpec> spec,
      std::unique_ptr<content::StoredPaymentApp> stored_payment_app_info,
      bool is_incognito,
      const base::RepeatingClosure& show_processing_spinner);

  // This constructor is used for a payment app that has not been installed in
  // Chrome but can be installed when paying with it. The `spec` parameter
  // should not be null.
  ServiceWorkerPaymentApp(
      content::WebContents* web_contents,
      const GURL& top_origin,
      const GURL& frame_origin,
      base::WeakPtr<PaymentRequestSpec> spec,
      std::unique_ptr<WebAppInstallationInfo> installable_payment_app_info,
      const std::string& enabled_method,
      bool is_incognito,
      const base::RepeatingClosure& show_processing_spinner);

  ServiceWorkerPaymentApp(const ServiceWorkerPaymentApp&) = delete;
  ServiceWorkerPaymentApp& operator=(const ServiceWorkerPaymentApp&) = delete;

  ~ServiceWorkerPaymentApp() override;

  // The callback for ValidateCanMakePayment.
  using ValidateCanMakePaymentCallback =
      base::OnceCallback<void(base::WeakPtr<ServiceWorkerPaymentApp>)>;

  // Validates whether this payment app can be used for this payment request. It
  // fires CanMakePaymentEvent to the payment app to do validation. The result
  // is returned through callback. If the returned result is false, then this
  // app should not be used for this payment request. This interface must be
  // called before any other interfaces in this class.
  void ValidateCanMakePayment(ValidateCanMakePaymentCallback callback);

  // PaymentApp:
  void InvokePaymentApp(base::WeakPtr<Delegate> delegate) override;
  void OnPaymentAppWindowClosed() override;
  bool IsCompleteForPayment() const override;
  bool CanPreselect() const override;
  std::u16string GetMissingInfoLabel() const override;
  bool HasEnrolledInstrument() const override;
  void RecordUse() override;
  bool NeedsInstallation() const override;
  std::string GetId() const override;
  std::u16string GetLabel() const override;
  std::u16string GetSublabel() const override;
  bool IsValidForModifier(const std::string& method) const override;
  base::WeakPtr<PaymentApp> AsWeakPtr() override;
  const SkBitmap* icon_bitmap() const override;
  std::set<std::string> GetApplicationIdentifiersThatHideThisApp()
      const override;
  bool HandlesShippingAddress() const override;
  bool HandlesPayerName() const override;
  bool HandlesPayerEmail() const override;
  bool HandlesPayerPhone() const override;
  ukm::SourceId UkmSourceId() override;
  void SetPaymentHandlerHost(
      base::WeakPtr<PaymentHandlerHost> payment_handler_host) override;
  bool IsWaitingForPaymentDetailsUpdate() const override;
  void UpdateWith(
      mojom::PaymentRequestDetailsUpdatePtr details_update) override;
  void OnPaymentDetailsNotUpdated() override;
  void AbortPaymentApp(base::OnceCallback<void(bool)> abort_callback) override;

 private:
  friend class ServiceWorkerPaymentAppTest;

  void OnPaymentAppResponse(mojom::PaymentHandlerResponsePtr response);
  mojom::PaymentRequestEventDataPtr CreatePaymentRequestEventData();

  mojom::CanMakePaymentEventDataPtr CreateCanMakePaymentEventData();
  void OnCanMakePaymentEventSkipped(ValidateCanMakePaymentCallback callback);
  void OnCanMakePaymentEventResponded(
      ValidateCanMakePaymentCallback callback,
      mojom::CanMakePaymentResponsePtr response);
  void CallValidateCanMakePaymentCallback(
      ValidateCanMakePaymentCallback callback);

  // Called from two places:
  // 1) From PaymentAppProvider after a just-in-time installable payment handler
  //    has been installed.
  // 2) From this class when an already installed payment handler is about to be
  //    invoked.
  void OnPaymentAppIdentity(const url::Origin& origin, int64_t registration_id);

  content::PaymentAppProvider* GetPaymentAppProvider();

  GURL top_origin_;
  GURL frame_origin_;
  base::WeakPtr<PaymentRequestSpec> spec_;
  std::unique_ptr<content::StoredPaymentApp> stored_payment_app_info_;

  base::WeakPtr<Delegate> delegate_;

  bool is_incognito_;

  // Disables user interaction by showing a spinner. Used when the app is
  // invoked.
  base::RepeatingClosure show_processing_spinner_;

  base::WeakPtr<PaymentHandlerHost> payment_handler_host_;
  mojo::PendingRemote<mojom::PaymentHandlerHost> payment_handler_host_remote_;

  // Service worker registration identifier. Used for aborting the payment app.
  int64_t registration_id_ = 0;

  // PaymentAppProvider::CanMakePayment result of this payment app.
  bool can_make_payment_result_;
  bool has_enrolled_instrument_result_;

  // Below variables are used for installable ServiceWorkerPaymentApp
  // specifically.
  bool needs_installation_;
  std::unique_ptr<WebAppInstallationInfo> installable_web_app_info_;
  std::string installable_enabled_method_;

  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  base::WeakPtr<content::WebContents> web_contents_;

  base::WeakPtrFactory<ServiceWorkerPaymentApp> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SERVICE_WORKER_PAYMENT_APP_H_
