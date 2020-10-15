// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/developer_console_logger.h"
#include "components/payments/content/initialization_task.h"
#include "components/payments/content/payment_handler_host.h"
#include "components/payments/content/payment_request_display_manager.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/content/service_worker_payment_app.h"
#include "components/payments/core/journey_logger.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace payments {

class ContentPaymentRequestDelegate;
class PaymentRequestWebContentsManager;

// This class manages the interaction between the renderer (through the
// PaymentRequestClient and Mojo stub implementation) and the desktop Payment UI
// (through the PaymentRequestDelegate). The API user (merchant) specification
// (supported payment methods, required information, order details) is stored in
// PaymentRequestSpec, and the current user selection state (and related data)
// is stored in PaymentRequestState.
class PaymentRequest : public mojom::PaymentRequest,
                       public PaymentHandlerHost::Delegate,
                       public PaymentRequestSpec::Observer,
                       public PaymentRequestState::Delegate,
                       public InitializationTask::Observer {
 public:
  class ObserverForTest {
   public:
    virtual void OnCanMakePaymentCalled() = 0;
    virtual void OnCanMakePaymentReturned() = 0;
    virtual void OnHasEnrolledInstrumentCalled() = 0;
    virtual void OnHasEnrolledInstrumentReturned() = 0;
    virtual void OnAppListReady(base::WeakPtr<PaymentRequest> payment_request) {
    }
    virtual void OnNotSupportedError() = 0;
    virtual void OnConnectionTerminated() = 0;
    virtual void OnAbortCalled() = 0;
    virtual void OnCompleteCalled() {}

   protected:
    virtual ~ObserverForTest() {}
  };

  PaymentRequest(content::RenderFrameHost* render_frame_host,
                 std::unique_ptr<ContentPaymentRequestDelegate> delegate,
                 PaymentRequestWebContentsManager* manager,
                 PaymentRequestDisplayManager* display_manager,
                 mojo::PendingReceiver<mojom::PaymentRequest> receiver,
                 ObserverForTest* observer_for_testing);
  ~PaymentRequest() override;

  // mojom::PaymentRequest
  void Init(mojo::PendingRemote<mojom::PaymentRequestClient> client,
            std::vector<mojom::PaymentMethodDataPtr> method_data,
            mojom::PaymentDetailsPtr details,
            mojom::PaymentOptionsPtr options) override;
  void Show(bool is_user_gesture, bool wait_for_updated_details) override;
  void Retry(mojom::PaymentValidationErrorsPtr errors) override;
  void UpdateWith(mojom::PaymentDetailsPtr details) override;
  void OnPaymentDetailsNotUpdated() override;
  void Abort() override;
  void Complete(mojom::PaymentComplete result) override;
  void CanMakePayment() override;
  void HasEnrolledInstrument() override;

  // PaymentHandlerHost::Delegate
  bool ChangePaymentMethod(const std::string& method_name,
                           const std::string& stringified_data) override;
  bool ChangeShippingOption(const std::string& shipping_option_id) override;
  bool ChangeShippingAddress(
      mojom::PaymentAddressPtr shipping_address) override;

  // PaymentRequestSpec::Observer:
  void OnSpecUpdated() override {}

  // PaymentRequestState::Delegate:
  void OnPaymentResponseAvailable(mojom::PaymentResponsePtr response) override;
  void OnPaymentResponseError(const std::string& error_message) override;
  void OnShippingOptionIdSelected(std::string shipping_option_id) override;
  void OnShippingAddressSelected(mojom::PaymentAddressPtr address) override;
  void OnPayerInfoSelected(mojom::PayerDetailPtr payer_info) override;

  // Called when the user explicitly cancelled the flow. Will send a message
  // to the renderer which will indirectly destroy this object (through
  // OnConnectionTerminated).
  void UserCancelled();

  // Called when the main frame attached to this PaymentRequest is navigating to
  // another document, but before the PaymentRequest is destroyed.
  void DidStartMainFrameNavigationToDifferentDocument(bool is_user_initiated);

  // Called when the frame attached to this PaymentRequest is about to be
  // destroyed. This is used to clean up before the RenderFrameHost is actually
  // destroyed because some objects held by the PaymentRequest (e.g.
  // InternalAuthenticator) must be out-lived by the RenderFrameHost.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host);

  // As a result of a browser-side error or renderer-initiated mojo channel
  // closure (e.g. there was an error on the renderer side, or payment was
  // successful), this method is called. It is responsible for cleaning up,
  // such as possibly closing the dialog.
  void OnConnectionTerminated();

  // Called when the user clicks on the "Pay" button.
  void Pay();

  // Hide this Payment Request if it's already showing.
  void HideIfNecessary();

  bool IsOffTheRecord() const;

  // Called when the payment handler requests to open a payment handler window.
  void OnPaymentHandlerOpenWindowCalled();

  content::WebContents* web_contents();

  const content::GlobalFrameRoutingId& initiator_frame_routing_id() const {
    return initiator_frame_routing_id_;
  }

  bool skipped_payment_request_ui() { return skipped_payment_request_ui_; }
  bool is_show_user_gesture() const { return is_show_user_gesture_; }

  base::WeakPtr<PaymentRequestSpec> spec() { return spec_->AsWeakPtr(); }
  base::WeakPtr<PaymentRequestState> state() { return state_->AsWeakPtr(); }

  base::WeakPtr<PaymentRequestSpec> spec() const { return spec_->AsWeakPtr(); }
  base::WeakPtr<PaymentRequestState> state() const {
    return state_->AsWeakPtr();
  }

  base::WeakPtr<PaymentRequest> GetWeakPtr();

 private:
  // InitializationTask::Observer.
  void OnInitialized(InitializationTask* initialization_task) override;

  // Returns true after init() has been called and the mojo connection has been
  // established. If the mojo connection gets later disconnected, this will
  // returns false.
  bool IsInitialized() const;

  // Returns true after show() has been called and the payment sheet is showing.
  // If the payment sheet is later hidden, this will return false.
  bool IsThisPaymentRequestShowing() const;

  // Returns true when there is exactly one available payment app which can
  // provide all requested information including shipping address and payer's
  // contact information whenever needed.
  bool OnlySingleAppCanProvideAllRequiredInformation() const;

  // Returns true if this payment request supports skipping the Payment Sheet.
  // Typically, this means that exactly one payment app can provide requested
  // information.
  bool SatisfiesSkipUIConstraints();

  // Only records the abort reason if it's the first completion for this Payment
  // Request. This is necessary since the aborts cascade into one another with
  // the first one being the most precise.
  void RecordFirstAbortReason(JourneyLogger::AbortReason completion_status);

  // The callback for PaymentRequestState::CanMakePayment.
  void CanMakePaymentCallback(bool can_make_payment);

  // The callback for PaymentRequestState::HasEnrolledInstrument. Checks for
  // query quota and may send QUERY_QUOTA_EXCEEDED.
  void HasEnrolledInstrumentCallback(bool has_enrolled_instrument);

  // The callback for PaymentRequestState::AreRequestedMethodsSupported.
  void AreRequestedMethodsSupportedCallback(bool methods_supported,
                                            const std::string& error_message);

  // Sends either HAS_ENROLLED_INSTRUMENT or HAS_NO_ENROLLED_INSTRUMENT to the
  // renderer, depending on |has_enrolled_instrument| value. Does not check
  // query quota so never sends QUERY_QUOTA_EXCEEDED. If
  // |warn_localhost_or_file| is true, then sends
  // WARNING_HAS_ENROLLED_INSTRUMENT or WARNING_HAS_NO_ENROLLED_INSTRUMENT
  // version of the values instead.
  void RespondToHasEnrolledInstrumentQuery(bool has_enrolled_instrument,
                                           bool warn_localhost_or_file);

  void OnAbortResult(bool aborted);

  const content::GlobalFrameRoutingId initiator_frame_routing_id_;
  DeveloperConsoleLogger log_;
  std::unique_ptr<ContentPaymentRequestDelegate> delegate_;
  // |manager_| owns this PaymentRequest.
  PaymentRequestWebContentsManager* manager_;
  PaymentRequestDisplayManager* display_manager_;
  std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle> display_handle_;
  mojo::Receiver<mojom::PaymentRequest> receiver_{this};
  mojo::Remote<mojom::PaymentRequestClient> client_;

  std::unique_ptr<PaymentRequestSpec> spec_;
  std::unique_ptr<PaymentRequestState> state_;

  // The end-point for the payment handler renderer process to call into the
  // browser process.
  std::unique_ptr<PaymentHandlerHost> payment_handler_host_;

  // The scheme, host, and port of the top level frame that has invoked
  // PaymentRequest API as formatted by
  // url_formatter::FormatUrlForSecurityDisplay(). This is what the user sees in
  // the address bar.
  const GURL top_level_origin_;

  // The scheme, host, and port of the frame that has invoked PaymentRequest API
  // as formatted by url_formatter::FormatUrlForSecurityDisplay(). This can be
  // either the main frame or an iframe.
  const GURL frame_origin_;

  // The security origin of the frame that has invoked PaymentRequest API. This
  // can be opaque. Used by security features like 'Sec-Fetch-Site' and
  // 'Cross-Origin-Resource-Policy'.
  const url::Origin frame_security_origin_;

  // May be null, must outlive this object.
  ObserverForTest* observer_for_testing_;

  JourneyLogger journey_logger_;

  // Whether a completion was already recorded for this Payment Request.
  bool has_recorded_completion_ = false;

  // Whether PaymentRequest.show() was invoked with a user gesture.
  bool is_show_user_gesture_ = false;

  // Whether PaymentRequest.show() was invoked by skipping payment request UI.
  bool skipped_payment_request_ui_ = false;

  // Whether PaymentRequest mojo connection has been initialized from the
  // renderer.
  bool is_initialized_ = false;

  // Whether PaymentRequest.show() has been called.
  bool is_show_called_ = false;

  // If not empty, use this error message for rejecting PaymentRequest.show().
  std::string reject_show_error_message_;

  base::WeakPtrFactory<PaymentRequest> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PaymentRequest);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_H_
