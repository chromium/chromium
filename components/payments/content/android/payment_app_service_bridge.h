// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_APP_SERVICE_BRIDGE_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_APP_SERVICE_BRIDGE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_app_factory.h"
#include "content/public/browser/global_routing_id.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace payments {
class PaymentAppService;

// A bridge that holds parameters needed by PaymentAppService and redirects
// callbacks from PaymentAppFactory to callbacks set by the caller.
class PaymentAppServiceBridge : public PaymentAppFactory::Delegate {
 public:
  using CanMakePaymentCalculatedCallback = base::OnceCallback<void(bool)>;
  using PaymentAppCreatedCallback =
      base::RepeatingCallback<void(std::unique_ptr<PaymentApp>)>;
  using PaymentAppCreationErrorCallback =
      base::RepeatingCallback<void(const std::string&,
                                   AppCreationFailureReason)>;

  // Creates a new PaymentAppServiceBridge. This object is self-deleting; its
  // memory is freed after CreatePaymentApps() is invoked and
  // OnDoneCreatingPaymentApps() is called `number_of_pending_factories_` times.
  static PaymentAppServiceBridge* Create(
      std::unique_ptr<PaymentAppService> payment_app_service,
      content::RenderFrameHost* render_frame_host,
      const GURL& top_origin,
      base::WeakPtr<PaymentRequestSpec> spec,
      const std::string& twa_package_name,
      scoped_refptr<PaymentManifestWebDataService> web_data_service,
      bool is_off_the_record,
      base::WeakPtr<CSPChecker> csp_checker,
      CanMakePaymentCalculatedCallback can_make_payment_calculated_callback,
      PaymentAppCreatedCallback payment_app_created_callback,
      PaymentAppCreationErrorCallback payment_app_creation_error_callback,
      base::OnceClosure done_creating_payment_apps_callback,
      base::RepeatingClosure set_can_make_payment_even_without_apps_callback,
      base::RepeatingClosure set_opt_out_offered_callback);

  ~PaymentAppServiceBridge() override;

  // Not copiable or movable.
  PaymentAppServiceBridge(const PaymentAppServiceBridge&) = delete;
  PaymentAppServiceBridge& operator=(const PaymentAppServiceBridge&) = delete;

  void CreatePaymentApps();

  base::WeakPtr<PaymentAppServiceBridge> GetWeakPtrForTest();

  // PaymentAppFactory::Delegate
  content::WebContents* GetWebContents() override;
  const GURL& GetTopOrigin() override;
  const GURL& GetFrameOrigin() override;
  const url::Origin& GetFrameSecurityOrigin() override;
  content::RenderFrameHost* GetInitiatorRenderFrameHost() const override;
  content::GlobalRenderFrameHostId GetInitiatorRenderFrameHostId()
      const override;
  const std::vector<mojom::PaymentMethodDataPtr>& GetMethodData()
      const override;
  std::unique_ptr<webauthn::InternalAuthenticator> CreateInternalAuthenticator()
      const override;
  scoped_refptr<PaymentManifestWebDataService>
  GetPaymentManifestWebDataService() const override;
  bool IsOffTheRecord() const override;
  base::WeakPtr<ContentPaymentRequestDelegate> GetPaymentRequestDelegate()
      const override;
  void ShowProcessingSpinner() override;
  base::WeakPtr<PaymentRequestSpec> GetSpec() const override;
  void GetTwaPackageName(GetTwaPackageNameCallback callback) override;
  void OnPaymentAppCreated(std::unique_ptr<PaymentApp> app) override;
  void OnPaymentAppCreationError(
      const std::string& error_message,
      AppCreationFailureReason error_reason) override;
  void OnDoneCreatingPaymentApps() override;
  void SetCanMakePaymentEvenWithoutApps() override;
  base::WeakPtr<CSPChecker> GetCSPChecker() override;
  void SetOptOutOffered() override;
  std::optional<base::UnguessableToken> GetChromeOSTWAInstanceId()
      const override;

 private:
  // Prevents direct instantiation. Callers should use Create() instead.
  PaymentAppServiceBridge(
      std::unique_ptr<PaymentAppService> payment_app_service,
      content::RenderFrameHost* render_frame_host,
      const GURL& top_origin,
      base::WeakPtr<PaymentRequestSpec> spec,
      const std::string& twa_package_name,
      scoped_refptr<PaymentManifestWebDataService> web_data_service,
      bool is_off_the_record,
      base::WeakPtr<CSPChecker> csp_checker,
      CanMakePaymentCalculatedCallback can_make_payment_calculated_callback,
      PaymentAppCreatedCallback payment_app_created_callback,
      PaymentAppCreationErrorCallback payment_app_creation_error_callback,
      base::OnceClosure done_creating_payment_apps_callback,
      base::RepeatingClosure set_can_make_payment_even_without_apps_callback,
      base::RepeatingClosure set_opt_out_offered_callback);

  const std::unique_ptr<PaymentAppService> payment_app_service_;
  size_t number_of_pending_factories_;
  content::GlobalRenderFrameHostId frame_routing_id_;
  const GURL top_origin_;
  const GURL frame_origin_;
  const url::Origin frame_security_origin_;
  base::WeakPtr<PaymentRequestSpec> spec_;
  const std::string twa_package_name_;
  scoped_refptr<PaymentManifestWebDataService>
      payment_manifest_web_data_service_;
  bool is_off_the_record_;
  base::WeakPtr<CSPChecker> csp_checker_;

  CanMakePaymentCalculatedCallback can_make_payment_calculated_callback_;
  PaymentAppCreatedCallback payment_app_created_callback_;
  PaymentAppCreationErrorCallback payment_app_creation_error_callback_;
  base::OnceClosure done_creating_payment_apps_callback_;
  base::RepeatingClosure set_can_make_payment_even_without_apps_callback_;
  base::RepeatingClosure set_opt_out_offered_callback_;

  base::WeakPtrFactory<PaymentAppServiceBridge> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_APP_SERVICE_BRIDGE_H_
