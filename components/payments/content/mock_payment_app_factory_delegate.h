// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_MOCK_PAYMENT_APP_FACTORY_DELEGATE_H_
#define COMPONENTS_PAYMENTS_CONTENT_MOCK_PAYMENT_APP_FACTORY_DELEGATE_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_app_factory.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments {

class MockPaymentAppFactoryDelegate : public PaymentAppFactory::Delegate {
 public:
  MockPaymentAppFactoryDelegate(content::WebContents* web_contents,
                                mojom::PaymentMethodDataPtr method_data);
  ~MockPaymentAppFactoryDelegate() override;

  void SetRequestedPaymentMethod(mojom::PaymentMethodDataPtr method_data);

  void set_is_off_the_record() { is_off_the_record_ = true; }

  // PaymentAppFactory::Delegate implementation:
  content::WebContents* GetWebContents() override { return web_contents_; }
  const GURL& GetTopOrigin() override { return top_origin_; }
  const GURL& GetFrameOrigin() override { return frame_origin_; }
  MOCK_METHOD0(GetFrameSecurityOrigin, const url::Origin&());
  MOCK_CONST_METHOD0(GetInitiatorRenderFrameHost, content::RenderFrameHost*());
  MOCK_CONST_METHOD0(GetInitiatorRenderFrameHostId,
                     content::GlobalRenderFrameHostId());
  MOCK_CONST_METHOD0(GetMethodData,
                     const std::vector<mojom::PaymentMethodDataPtr>&());
  MOCK_CONST_METHOD0(CreateInternalAuthenticator,
                     std::unique_ptr<webauthn::InternalAuthenticator>());
  MOCK_CONST_METHOD0(GetPaymentManifestWebDataService,
                     scoped_refptr<PaymentManifestWebDataService>());
  MOCK_METHOD0(MayCrawlForInstallablePaymentApps, bool());
  bool IsOffTheRecord() const override { return is_off_the_record_; }
  base::WeakPtr<PaymentRequestSpec> GetSpec() const override {
    return spec_->AsWeakPtr();
  }
  MOCK_METHOD1(GetTwaPackageName, void(GetTwaPackageNameCallback));
  MOCK_METHOD0(ShowProcessingSpinner, void());
  MOCK_CONST_METHOD0(GetPaymentRequestDelegate,
                     base::WeakPtr<ContentPaymentRequestDelegate>());
  MOCK_METHOD1(OnPaymentAppCreated, void(std::unique_ptr<PaymentApp> app));
  MOCK_METHOD2(OnPaymentAppCreationError,
               void(const std::string& error_message,
                    AppCreationFailureReason reason));
  MOCK_METHOD0(OnDoneCreatingPaymentApps, void());
  MOCK_METHOD0(SetCanMakePaymentEvenWithoutApps, void());
  MOCK_METHOD0(GetCSPChecker, base::WeakPtr<CSPChecker>());
  MOCK_METHOD0(SetOptOutOffered, void());
  MOCK_CONST_METHOD0(GetChromeOSTWAInstanceId,
                     std::optional<base::UnguessableToken>());

  base::WeakPtr<PaymentAppFactory::Delegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  raw_ptr<content::WebContents> web_contents_;
  GURL top_origin_;
  GURL frame_origin_;
  std::unique_ptr<PaymentRequestSpec> spec_;
  bool is_off_the_record_ = false;
  base::WeakPtrFactory<PaymentAppFactory::Delegate> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_MOCK_PAYMENT_APP_FACTORY_DELEGATE_H_
