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
#include "components/payments/core/secure_payment_confirmation_metrics.h"
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
      content::GlobalRenderFrameHostId initiator_frame_routing_id,
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
  // Methods that perform async actions (like DownloadIconAndShowUserPrompt,
  // StorePaymentCredentialAndHideUserPrompt, DidDownloadIcon,
  // OnUserResponseFromUI) have procedure:
  //   1. Validate state.
  //   2. Validate parameters.
  //   3. Use parameters.
  //   4. Update the state.
  //   5. Make the async call.
  // Methods that perform terminating actions (like HideUserPrompt,
  // OnWebDataServiceRequestDone, or OnUserResponseFromUI telling the renderer
  // that the user has rejected the prompt) have procedure:
  //   1. Validate state.
  //   2. Validate parameters.
  //   3. Use parameters.
  //   4. Call Reset() to close UI and perform cleanup.
  //   5. Invoke a mojo callback to the renderer.
  // Any method may call Reset() to ensure callbacks are called and return to a
  // valid Idle state.
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

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  bool IsCurrentStateValid() const;

  void DidDownloadIcon(const std::u16string instrument_name,
                       int request_id,
                       int unused_http_status_code,
                       const GURL& unused_image_url,
                       const std::vector<SkBitmap>& bitmaps,
                       const std::vector<gfx::Size>& unused_sizes);

  void OnUserResponseFromUI(bool user_confirm_from_ui);

  void RecordFirstDialogShown(SecurePaymentConfirmationEnrollDialogShown shown);
  void RecordFirstSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult result);

  void Reset();

  State state_ = State::kIdle;
  const content::GlobalRenderFrameHostId initiator_frame_routing_id_;
  scoped_refptr<PaymentManifestWebDataService> web_data_service_;
  absl::optional<WebDataServiceBase::Handle> data_service_request_handle_;
  DownloadIconAndShowUserPromptCallback prompt_callback_;
  StorePaymentCredentialAndHideUserPromptCallback storage_callback_;
  mojo::Receiver<mojom::PaymentCredential> receiver_{this};
  absl::optional<int> pending_icon_download_request_id_;
  std::vector<uint8_t> encoded_icon_;
  std::unique_ptr<PaymentCredentialEnrollmentController::ScopedToken>
      ui_controller_token_;
  base::WeakPtr<PaymentCredentialEnrollmentController> ui_controller_;
  bool is_dialog_shown_recorded_ = false;
  bool is_system_prompt_result_recorded_ = false;

  base::WeakPtrFactory<PaymentCredential> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_H_
