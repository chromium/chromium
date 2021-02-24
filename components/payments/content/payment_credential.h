// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_credential_enrollment_controller.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/payments/payment_credential.mojom.h"

namespace content {
class WebContents;
}  // namespace content

namespace payments {

class PaymentManifestWebDataService;

// Implementation of the mojom::PaymentCredential interface for storing
// PaymentCredential instruments and their associated WebAuthn credential IDs.
// These can be retrieved later to authenticate during a PaymentRequest
// that uses Secure Payment Confirmation.
class PaymentCredential : public mojom::PaymentCredential,
                          public WebDataServiceConsumer,
                          public content::WebContentsObserver {
 public:
  static bool IsFrameAllowedToUseSecurePaymentConfirmation(
      content::RenderFrameHost* rfh);

  PaymentCredential(
      content::WebContents* web_contents,
      content::GlobalFrameRoutingId initiator_frame_routing_id,
      scoped_refptr<PaymentManifestWebDataService> web_data_service,
      mojo::PendingReceiver<mojom::PaymentCredential> receiver);
  ~PaymentCredential() override;

  PaymentCredential(const PaymentCredential&) = delete;
  PaymentCredential& operator=(const PaymentCredential&) = delete;

  // mojom::PaymentCredential:
  void DownloadIconAndShowUserPrompt(
      payments::mojom::PaymentCredentialInstrumentPtr instrument,
      DownloadIconAndShowUserPromptCallback callback) override;
  void StorePaymentCredentialAndHideUserPrompt(
      payments::mojom::PaymentCredentialInstrumentPtr instrument,
      const std::vector<uint8_t>& credential_id,
      const std::string& rp_id,
      StorePaymentCredentialAndHideUserPromptCallback callback) override;
  void HideUserPrompt(HideUserPromptCallback callback) override;

 private:
  // States of the enrollment flow, necessary to ensure correctness with
  // multiple round-trips to the renderer process. Each state is allowed to
  // transition only to the next state (if any) or back to idle.
  enum class State {
    kIdle,
    kDownloadingIcon,
    kShowingUserPrompt,
    kMakingCredential,
    kStoringCredential
  };

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

  bool IsCurrentStateValid() const;

  void DidDownloadIcon(DownloadIconAndShowUserPromptCallback callback,
                       int request_id,
                       int unused_http_status_code,
                       const GURL& image_url,
                       const std::vector<SkBitmap>& bitmaps,
                       const std::vector<gfx::Size>& unused_sizes);

  void OnUserResponseFromUI(DownloadIconAndShowUserPromptCallback callback,
                            bool user_confirm_from_ui);

  void AbortAndCleanup();

  State state_ = State::kIdle;
  const content::GlobalFrameRoutingId initiator_frame_routing_id_;
  scoped_refptr<PaymentManifestWebDataService> web_data_service_;
  std::map<WebDataServiceBase::Handle,
           StorePaymentCredentialAndHideUserPromptCallback>
      storage_callbacks_;
  mojo::Receiver<mojom::PaymentCredential> receiver_{this};
  base::Optional<int> pending_icon_download_request_id_;
  std::vector<uint8_t> encoded_icon_;
  std::unique_ptr<PaymentCredentialEnrollmentController::ScopedToken>
      ui_controller_token_;
  base::WeakPtr<PaymentCredentialEnrollmentController> ui_controller_;

  base::WeakPtrFactory<PaymentCredential> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_H_
