// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/payments/content/developer_console_logger.h"
#include "components/payments/content/initialization_task.h"
#include "components/payments/content/payment_handler_host.h"
#include "components/payments/content/payment_request_display_manager.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/content/service_worker_payment_app.h"
#include "components/payments/core/csp_checker.h"
#include "components/payments/core/journey_logger.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}

namespace payments {
class ContentPaymentRequestDelegate;

enum class SPCTransactionMode {
  NONE,
  AUTOACCEPT,
  AUTOREJECT,
  AUTOOPTOUT,
};

// This class manages the interaction between the renderer (through the
// PaymentRequestClient and Mojo stub implementation) and the desktop Payment UI
// (through the PaymentRequestDelegate). The API user (merchant) specification
// (supported payment methods, required information, order details) is stored in
// PaymentRequestSpec, and the current user selection state (and related data)
// is stored in PaymentRequestState.
// As the PaymentRequest is a DocumentService, its lifetime is managed by the
// RenderFrameHost that the request is created for, and will be destroyed when
// the current document is or when the mojom::PaymentRequest connection is lost.
// The PaymentRequest is also a WebContentsObserver, which has historically
// been used to track the lifetime of the RenderFrameHost, which is now done by
// DocumentService. Instead, the WebContentsObserver is used to watch for
// navigations that would destroy this object's document _in the future_ in
// order to record metrics.
class PaymentRequest : public content::DocumentService<mojom::PaymentRequest>,
                       public PaymentHandlerHost::Delegate,
                       public PaymentRequestSpec::Observer,
                       public PaymentRequestState::Delegate,
                       public InitializationTask::Observer,
                       public CSPChecker,
                       public content::WebContentsObserver {
 public:
  class ObserverForTest {
   public:
    virtual void OnCanMakePaymentCalled() = 0;
    virtual void OnCanMakePaymentReturned() = 0;
    virtual void OnHasEnrolledInstrumentCalled() = 0;
    virtual void OnHasEnrolledInstrumentReturned() = 0;
    virtual void OnAppListReady(base::WeakPtr<PaymentRequest> payment_request) {
    }
    virtual void OnErrorDisplayed() {}
    virtual void OnNotSupportedError() = 0;
    virtual void OnConnectionTerminated() = 0;
    virtual void OnPayCalled() = 0;
    virtual void OnAbortCalled() = 0;
    virtual void OnCompleteCalled() {}

   protected:
    virtual ~ObserverForTest() = default;
  };

  PaymentRequest(std::unique_ptr<ContentPaymentRequestDelegate> delegate,
                 mojo::PendingReceiver<mojom::PaymentRequest> receiver);

  PaymentRequest(const PaymentRequest&) = delete;
  PaymentRequest& operator=(const PaymentRequest&) = delete;

  ~PaymentRequest() override;

  // mojom::PaymentRequest
  void Init(mojo::PendingRemote<mojom::PaymentRequestClient> client,
            std::vector<mojom::PaymentMethodDataPtr> method_data,
            mojom::PaymentDetailsPtr details,
            mojom::PaymentOptionsPtr options) override;
  void Show(bool wait_for_updated_details, bool had_user_activation) override;
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

  // WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

  // PaymentRequestState::Delegate:
  void OnPaymentResponseAvailable(mojom::PaymentResponsePtr response) override;
  void OnPaymentResponseError(const std::string& error_message) override;
  void OnShippingOptionIdSelected(std::string shipping_option_id) override;
  void OnShippingAddressSelected(mojom::PaymentAddressPtr address) override;
  void OnPayerInfoSelected(mojom::PayerDetailPtr payer_info) override;

  // Called when the user explicitly cancelled the flow. Will destroy this
  // object and close any related connections.
  void OnUserCancelled();

  // Called when the user explicitly opts out of the flow. Only used for
  // SecurePaymentConfirmation currently.
  void OnUserOptedOut();

  // Called when the PaymentRequest is about to be destroyed. This reports
  // the reason for destruction.
  void WillBeDestroyed(content::DocumentServiceDestructionReason reason) final;

  // Called when the user clicks on the "Pay" button.
  void Pay();

  bool IsOffTheRecord() const;

  // Called when the payment handler requests to open a payment handler
  // window.
  void OnPaymentHandlerOpenWindowCalled();

  bool skipped_payment_request_ui() { return skipped_payment_request_ui_; }
  SPCTransactionMode spc_transaction_mode() const {
    return spc_transaction_mode_;
  }

  base::WeakPtr<PaymentRequestSpec> spec() { return spec_->AsWeakPtr(); }
  base::WeakPtr<PaymentRequestState> state() { return state_->AsWeakPtr(); }

  base::WeakPtr<PaymentRequestSpec> spec() const { return spec_->AsWeakPtr(); }
  base::WeakPtr<PaymentRequestState> state() const {
    return state_->AsWeakPtr();
  }

  base::WeakPtr<PaymentRequest> GetWeakPtr();

  void set_observer_for_test(base::WeakPtr<ObserverForTest> observer_for_test) {
    observer_for_testing_ = observer_for_test;
  }

 private:
  // CSPChecker.
  void AllowConnectToSource(
      const GURL& url,
      const GURL& url_before_redirects,
      bool did_follow_redirect,
      base::OnceCallback<void(bool)> result_callback) override;

  // InitializationTask::Observer.
  void OnInitialized(InitializationTask* initialization_task) override;

  // Returns true after init() has been called and the mojo connection has
  // been established. If the mojo connection gets later disconnected, this
  // will returns false.
  bool IsInitialized() const;

  // Returns true after show() has been called and the payment sheet is
  // showing. If the payment sheet is later hidden, this will return false.
  bool IsThisPaymentRequestShowing() const;

  // Returns true when there is exactly one available payment app which can
  // provide all requested information including shipping address and payer's
  // contact information whenever needed.
  bool OnlySingleAppCanProvideAllRequiredInformation() const;

  // Checks and records via JourneyLogger whether this payment request will skip
  // showing the Payment Sheet, and returns the result. Typically, this means
  // that exactly one payment app can provide requested information.
  bool CheckSatisfiesSkipUIConstraintsAndRecordShownState();

  // Only records the abort reason if it's the first completion for this
  // Payment Request. This is necessary since the aborts cascade into one
  // another with the first one being the most precise.
  void RecordFirstAbortReason(JourneyLogger::AbortReason completion_status);

  // The callback for PaymentRequestState::CanMakePayment.
  void CanMakePaymentCallback(bool can_make_payment);

  // The callback for PaymentRequestState::HasEnrolledInstrument. Checks for
  // query quota and may send QUERY_QUOTA_EXCEEDED.
  void HasEnrolledInstrumentCallback(bool has_enrolled_instrument);

  // The callback for PaymentRequestState::AreRequestedMethodsSupported.
  void AreRequestedMethodsSupportedCallback(
      bool methods_supported,
      const std::string& error_message,
      AppCreationFailureReason error_reason);

  // Sends either HAS_ENROLLED_INSTRUMENT or HAS_NO_ENROLLED_INSTRUMENT to the
  // renderer, depending on |has_enrolled_instrument| value. Does not check
  // query quota so never sends QUERY_QUOTA_EXCEEDED. If
  // |warn_localhost_or_file| is true, then sends
  // WARNING_HAS_ENROLLED_INSTRUMENT or WARNING_HAS_NO_ENROLLED_INSTRUMENT
  // version of the values instead.
  void RespondToHasEnrolledInstrumentQuery(bool has_enrolled_instrument,
                                           bool warn_localhost_or_file);

  void OnAbortResult(bool aborted);

  // Show an error message in the UI (if available) and abort payment.
  void ShowErrorMessageAndAbortPayment();

  // Get the payment method category from the selected app.
  JourneyLogger::PaymentMethodCategory GetSelectedMethodCategory() const;

  DeveloperConsoleLogger log_;
  std::unique_ptr<ContentPaymentRequestDelegate> delegate_;
  base::WeakPtr<PaymentRequestDisplayManager> display_manager_;
  std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle> display_handle_;
  mojo::Remote<mojom::PaymentRequestClient> client_;

  std::unique_ptr<PaymentRequestSpec> spec_;
  std::unique_ptr<PaymentRequestState> state_;

  // The end-point for the payment handler renderer process to call into the
  // browser process.
  std::unique_ptr<PaymentHandlerHost> payment_handler_host_;

  // The scheme, host, and port of the top level frame that has invoked
  // PaymentRequest API as formatted by
  // url_formatter::FormatUrlForSecurityDisplay(). This is what the user sees
  // in the address bar.
  const GURL top_level_origin_;

  // The scheme, host, and port of the frame that has invoked PaymentRequest
  // API as formatted by url_formatter::FormatUrlForSecurityDisplay(). This
  // can be either the main frame or an iframe.
  const GURL frame_origin_;

  // The security origin of the frame that has invoked PaymentRequest API.
  // This can be opaque. Used by security features like 'Sec-Fetch-Site' and
  // 'Cross-Origin-Resource-Policy'.
  const url::Origin frame_security_origin_;

  // The current SPC transaction mode; used in WPT test automation.
  SPCTransactionMode spc_transaction_mode_;

  // May be null, must outlive this object.
  base::WeakPtr<ObserverForTest> observer_for_testing_;

  JourneyLogger journey_logger_;

  // Whether a completion was already recorded for this Payment Request.
  bool has_recorded_completion_ = false;

  // Whether PaymentRequest.show() was invoked by skipping payment request UI.
  bool skipped_payment_request_ui_ = false;

  // Whether PaymentRequest mojo connection has been initialized from the
  // renderer.
  bool is_initialized_ = false;

  // Whether PaymentRequest.show() has been called.
  bool is_show_called_ = false;

  // Whether PaymentRequestState::AreRequestedMethodsSupported callback has been
  // invoked. This is distinct from state_->IsInitialized(), because the
  // callback is asynchronous.
  bool is_requested_methods_supported_invoked_ = false;

  // If not empty, use this error message for rejecting
  // PaymentRequest.show().
  std::string reject_show_error_message_;

  // Whether the PaymentRequest.show() was successfully invoked without a user
  // activation. Used to record the activationless show JourneyLogger event only
  // if UI was shown.
  bool is_activationless_show_ = false;

  base::WeakPtrFactory<PaymentRequest> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_H_
