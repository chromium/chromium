// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "components/payments/content/can_make_payment_query_factory.h"
#include "components/payments/content/content_payment_request_delegate.h"
#include "components/payments/content/payment_app.h"
#include "components/payments/content/payment_details_converter.h"
#include "components/payments/content/payment_request_converter.h"
#include "components/payments/content/payment_request_web_contents_manager.h"
#include "components/payments/core/can_make_payment_query.h"
#include "components/payments/core/error_message_util.h"
#include "components/payments/core/error_strings.h"
#include "components/payments/core/features.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/payment_details.h"
#include "components/payments/core/payment_details_validation.h"
#include "components/payments/core/payment_prefs.h"
#include "components/payments/core/payment_request_delegate.h"
#include "components/payments/core/payments_experimental_features.h"
#include "components/payments/core/payments_validators.h"
#include "components/payments/core/url_util.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/loader/network_utils.h"

namespace payments {
namespace {

using ::payments::mojom::CanMakePaymentQueryResult;
using ::payments::mojom::HasEnrolledInstrumentQueryResult;

bool IsGooglePaymentMethod(const std::string& method_name) {
  return method_name == methods::kGooglePay ||
         method_name == methods::kAndroidPay;
}

// Redact shipping address before exposing it in ShippingAddressChangeEvent.
// https://w3c.github.io/payment-request/#shipping-address-changed-algorithm
mojom::PaymentAddressPtr RedactShippingAddress(
    mojom::PaymentAddressPtr address) {
  DCHECK(address);
  if (!PaymentsExperimentalFeatures::IsEnabled(
          features::kWebPaymentsRedactShippingAddress)) {
    return address;
  }
  address->organization.clear();
  address->phone.clear();
  address->recipient.clear();
  address->address_line.clear();
  return address;
}

}  // namespace

PaymentRequest::PaymentRequest(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<ContentPaymentRequestDelegate> delegate,
    PaymentRequestWebContentsManager* manager,
    PaymentRequestDisplayManager* display_manager,
    mojo::PendingReceiver<mojom::PaymentRequest> receiver,
    ObserverForTest* observer_for_testing)
    : initiator_frame_routing_id_(content::GlobalFrameRoutingId(
          render_frame_host->GetProcess()->GetID(),
          render_frame_host->GetRoutingID())),
      log_(web_contents()),
      delegate_(std::move(delegate)),
      manager_(manager),
      display_manager_(display_manager),
      display_handle_(nullptr),
      top_level_origin_(url_formatter::FormatUrlForSecurityDisplay(
          web_contents()->GetLastCommittedURL())),
      frame_origin_(url_formatter::FormatUrlForSecurityDisplay(
          render_frame_host->GetLastCommittedURL())),
      frame_security_origin_(render_frame_host->GetLastCommittedOrigin()),
      observer_for_testing_(observer_for_testing),
      journey_logger_(delegate_->IsOffTheRecord(),
                      ukm::GetSourceIdForWebContentsDocument(web_contents())) {
  receiver_.Bind(std::move(receiver));
  // OnConnectionTerminated will be called when the Mojo pipe is closed. This
  // will happen as a result of many renderer-side events (both successful and
  // erroneous in nature).
  // TODO(crbug.com/683636): Investigate using
  // set_connection_error_with_reason_handler with Binding::CloseWithReason.
  receiver_.set_disconnect_handler(base::BindOnce(
      &PaymentRequest::OnConnectionTerminated, weak_ptr_factory_.GetWeakPtr()));

  payment_handler_host_ = std::make_unique<PaymentHandlerHost>(
      web_contents(), weak_ptr_factory_.GetWeakPtr());
}

PaymentRequest::~PaymentRequest() = default;

void PaymentRequest::Init(
    mojo::PendingRemote<mojom::PaymentRequestClient> client,
    std::vector<mojom::PaymentMethodDataPtr> method_data,
    mojom::PaymentDetailsPtr details,
    mojom::PaymentOptionsPtr options) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_initialized_) {
    log_.Error(errors::kAttemptedInitializationTwice);
    OnConnectionTerminated();
    return;
  }

  journey_logger_.RecordCheckoutStep(
      JourneyLogger::CheckoutFunnelStep::kInitiated);
  is_initialized_ = true;
  client_.Bind(std::move(client));

  const GURL last_committed_url = delegate_->GetLastCommittedURL();
  if (!blink::network_utils::IsOriginSecure(last_committed_url)) {
    log_.Error(errors::kNotInASecureOrigin);
    OnConnectionTerminated();
    return;
  }

  bool allowed_origin =
      UrlUtil::IsOriginAllowedToUseWebPaymentApis(last_committed_url);
  if (!allowed_origin) {
    reject_show_error_message_ = errors::kProhibitedOrigin;
  }

  bool invalid_ssl = false;
  if (last_committed_url.SchemeIsCryptographic()) {
    DCHECK(reject_show_error_message_.empty());
    reject_show_error_message_ =
        delegate_->GetInvalidSslCertificateErrorMessage();
    invalid_ssl = !reject_show_error_message_.empty();
  }

  if (!allowed_origin || invalid_ssl) {
    // Intentionally don't set |spec_| and |state_|, so the UI is never shown.
    log_.Error(reject_show_error_message_);
    log_.Error(errors::kProhibitedOriginOrInvalidSslExplanation);
    client_->OnError(
        mojom::PaymentErrorReason::NOT_SUPPORTED_FOR_INVALID_ORIGIN_OR_SSL,
        reject_show_error_message_);
    OnConnectionTerminated();
    return;
  }

  std::string error;
  if (!ValidatePaymentDetails(ConvertPaymentDetails(details), &error)) {
    log_.Error(error);
    OnConnectionTerminated();
    return;
  }

  if (!details->total) {
    log_.Error(errors::kTotalRequired);
    OnConnectionTerminated();
    return;
  }

  auto* initiator_frame =
      content::RenderFrameHost::FromID(initiator_frame_routing_id_);
  if (!initiator_frame) {
    log_.Error(errors::kInvalidInitiatorFrame);
    OnConnectionTerminated();
    return;
  }

  spec_ = std::make_unique<PaymentRequestSpec>(
      std::move(options), std::move(details), std::move(method_data),
      /*observer=*/weak_ptr_factory_.GetWeakPtr(),
      delegate_->GetApplicationLocale());
  state_ = std::make_unique<PaymentRequestState>(
      initiator_frame, top_level_origin_, frame_origin_, frame_security_origin_,
      spec(), /*delegate=*/weak_ptr_factory_.GetWeakPtr(),
      delegate_->GetApplicationLocale(), delegate_->GetPersonalDataManager(),
      delegate_.get(), &journey_logger_);

  journey_logger_.SetRequestedInformation(
      spec_->request_shipping(), spec_->request_payer_email(),
      spec_->request_payer_phone(), spec_->request_payer_name());

  // Log metrics around which payment methods are requested by the merchant.
  GURL google_pay_url(methods::kGooglePay);
  GURL android_pay_url(methods::kAndroidPay);
  // Looking for payment methods that are NOT google-related payment methods.
  auto non_google_it =
      std::find_if(spec_->url_payment_method_identifiers().begin(),
                   spec_->url_payment_method_identifiers().end(),
                   [google_pay_url, android_pay_url](const GURL& url) {
                     return url != google_pay_url && url != android_pay_url;
                   });
  journey_logger_.SetRequestedPaymentMethodTypes(
      /*requested_basic_card=*/!spec_->supported_card_networks().empty(),
      /*requested_method_google=*/
      base::Contains(spec_->url_payment_method_identifiers(), google_pay_url) ||
          base::Contains(spec_->url_payment_method_identifiers(),
                         android_pay_url),
      /*requested_method_secure_payment_confirmation=*/
      spec_->IsSecurePaymentConfirmationRequested(),
      /*requested_method_other=*/non_google_it !=
          spec_->url_payment_method_identifiers().end());

  payment_handler_host_->set_payment_request_id_for_logs(*spec_->details().id);

  if (spec_->IsSecurePaymentConfirmationRequested()) {
    delegate_->set_dialog_type(
        PaymentRequestDelegate::DialogType::SECURE_PAYMENT_CONFIRMATION);
  }
}

void PaymentRequest::Show(bool is_user_gesture, bool wait_for_updated_details) {
  if (!IsInitialized()) {
    log_.Error(errors::kCannotShowWithoutInit);
    OnConnectionTerminated();
    return;
  }

  if (is_show_called_) {
    log_.Error(errors::kCannotShowTwice);
    OnConnectionTerminated();
    return;
  }

  journey_logger_.RecordCheckoutStep(
      JourneyLogger::CheckoutFunnelStep::kShowCalled);
  is_show_called_ = true;
  journey_logger_.SetTriggerTime();

  // A tab can display only one PaymentRequest UI at a time.
  display_handle_ = display_manager_->TryShow(delegate_.get());
  if (!display_handle_) {
    log_.Error(errors::kAnotherUiShowing);
    DCHECK(!has_recorded_completion_);
    has_recorded_completion_ = true;
    journey_logger_.SetNotShown(
        JourneyLogger::NOT_SHOWN_REASON_CONCURRENT_REQUESTS);
    client_->OnError(mojom::PaymentErrorReason::ALREADY_SHOWING,
                     errors::kAnotherUiShowing);
    OnConnectionTerminated();
    return;
  }

  if (!delegate_->IsBrowserWindowActive()) {
    log_.Error(errors::kCannotShowInBackgroundTab);
    DCHECK(!has_recorded_completion_);
    has_recorded_completion_ = true;
    journey_logger_.SetNotShown(JourneyLogger::NOT_SHOWN_REASON_OTHER);
    client_->OnError(mojom::PaymentErrorReason::USER_CANCEL,
                     errors::kCannotShowInBackgroundTab);
    OnConnectionTerminated();
    return;
  }

  is_show_user_gesture_ = is_user_gesture;

  if (wait_for_updated_details) {
    // Put |spec_| into uninitialized state, so the UI knows to show a spinner.
    // This method does not block.
    spec_->StartWaitingForUpdateWith(
        PaymentRequestSpec::UpdateReason::INITIAL_PAYMENT_DETAILS);
    spec_->AddInitializationObserver(this);
  } else {
    DCHECK(spec_->details().total);
    journey_logger_.RecordTransactionAmount(
        spec_->details().total->amount->currency,
        spec_->details().total->amount->value, false /*completed*/);
  }

  display_handle_->Show(weak_ptr_factory_.GetWeakPtr());

  state_->set_is_show_user_gesture(is_show_user_gesture_);
  state_->AreRequestedMethodsSupported(
      base::BindOnce(&PaymentRequest::AreRequestedMethodsSupportedCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PaymentRequest::Retry(mojom::PaymentValidationErrorsPtr errors) {
  if (!IsInitialized()) {
    log_.Error(errors::kCannotRetryWithoutInit);
    OnConnectionTerminated();
    return;
  }

  if (!IsThisPaymentRequestShowing()) {
    log_.Error(errors::kCannotRetryWithoutShow);
    OnConnectionTerminated();
    return;
  }

  std::string error;
  if (!PaymentsValidators::IsValidPaymentValidationErrorsFormat(errors,
                                                                &error)) {
    log_.Error(error);
    client_->OnError(mojom::PaymentErrorReason::USER_CANCEL, error);
    OnConnectionTerminated();
    return;
  }

  state()->SetAvailablePaymentAppForRetry();
  spec()->Retry(std::move(errors));
  display_handle_->Retry();
}

void PaymentRequest::UpdateWith(mojom::PaymentDetailsPtr details) {
  if (!IsInitialized()) {
    log_.Error(errors::kCannotUpdateWithoutInit);
    OnConnectionTerminated();
    return;
  }

  if (!IsThisPaymentRequestShowing()) {
    log_.Error(errors::kCannotUpdateWithoutShow);
    OnConnectionTerminated();
    return;
  }

  std::string error;
  if (!ValidatePaymentDetails(ConvertPaymentDetails(details), &error)) {
    log_.Error(error);
    OnConnectionTerminated();
    return;
  }

  if (details->shipping_address_errors &&
      !PaymentsValidators::IsValidAddressErrorsFormat(
          details->shipping_address_errors, &error)) {
    log_.Error(error);
    OnConnectionTerminated();
    return;
  }

  if (state()->selected_app() && state()->IsPaymentAppInvoked() &&
      state()->selected_app()->IsWaitingForPaymentDetailsUpdate()) {
    state()->selected_app()->UpdateWith(
        PaymentDetailsConverter::ConvertToPaymentRequestDetailsUpdate(
            details, state()->selected_app()->HandlesShippingAddress(),
            base::BindRepeating(&PaymentApp::IsValidForPaymentMethodIdentifier,
                                state()->selected_app()->AsWeakPtr())));
  }

  bool is_resolving_promise_passed_into_show_method = !spec_->IsInitialized();

  spec_->UpdateWith(std::move(details));

  if (is_resolving_promise_passed_into_show_method) {
    DCHECK(spec_->details().total);
    journey_logger_.RecordTransactionAmount(
        spec_->details().total->amount->currency,
        spec_->details().total->amount->value, false /*completed*/);
    if (SatisfiesSkipUIConstraints()) {
      Pay();
    } else if (spec_->request_shipping()) {
      state_->SelectDefaultShippingAddressAndNotifyObservers();
    }
  }
}

void PaymentRequest::OnPaymentDetailsNotUpdated() {
  // This Mojo call is triggered by the user of the API doing nothing in
  // response to a shipping address update event, so the error messages cannot
  // be more verbose.
  if (!IsInitialized()) {
    log_.Error(errors::kNotInitialized);
    OnConnectionTerminated();
    return;
  }

  if (!IsThisPaymentRequestShowing()) {
    log_.Error(errors::kNotShown);
    OnConnectionTerminated();
    return;
  }

  spec_->RecomputeSpecForDetails();

  if (state()->IsPaymentAppInvoked() && state()->selected_app() &&
      state()->selected_app()->IsWaitingForPaymentDetailsUpdate()) {
    state()->selected_app()->OnPaymentDetailsNotUpdated();
  }
}

void PaymentRequest::Abort() {
  if (!IsInitialized()) {
    log_.Error(errors::kCannotAbortWithoutInit);
    OnConnectionTerminated();
    return;
  }

  if (!IsThisPaymentRequestShowing()) {
    log_.Error(errors::kCannotAbortWithoutShow);
    OnConnectionTerminated();
    return;
  }

  // The API user has decided to abort. If a successful abort message is
  // returned to the renderer, the Mojo message pipe is closed, which triggers
  // PaymentRequest::OnConnectionTerminated, which destroys this object.
  // Otherwise, the abort promise is rejected and the pipe is not closed.
  // The abort is only successful if the payment app wasn't yet invoked.
  // TODO(crbug.com/716546): Add a merchant abort metric

  if (observer_for_testing_)
    observer_for_testing_->OnAbortCalled();

  if (!state_->IsPaymentAppInvoked() || !state_->selected_app()) {
    OnAbortResult(/*aborted=*/true);
    return;
  }

  state_->selected_app()->AbortPaymentApp(base::BindOnce(
      &PaymentRequest::OnAbortResult, weak_ptr_factory_.GetWeakPtr()));
}

void PaymentRequest::Complete(mojom::PaymentComplete result) {
  if (!IsInitialized()) {
    log_.Error(errors::kCannotCompleteWithoutInit);
    OnConnectionTerminated();
    return;
  }

  if (!IsThisPaymentRequestShowing()) {
    log_.Error(errors::kCannotAbortWithoutShow);
    OnConnectionTerminated();
    return;
  }

  if (observer_for_testing_) {
    observer_for_testing_->OnCompleteCalled();
  }

  // Failed transactions show an error. Successful and unknown-state
  // transactions don't show an error.
  if (result == mojom::PaymentComplete::FAIL) {
    delegate_->ShowErrorMessage();
  } else {
    DCHECK(!has_recorded_completion_);
    journey_logger_.SetCompleted();
    has_recorded_completion_ = true;
    DCHECK(spec_->details().total);
    journey_logger_.RecordTransactionAmount(
        spec_->details().total->amount->currency,
        spec_->details().total->amount->value, true /*completed*/);

    delegate_->GetPrefService()->SetBoolean(kPaymentsFirstTransactionCompleted,
                                            true);
    // When the renderer closes the connection,
    // PaymentRequest::OnConnectionTerminated will be called.
    client_->OnComplete();
    state_->RecordUseStats();
  }
}

void PaymentRequest::CanMakePayment() {
  if (!IsInitialized()) {
    log_.Error(errors::kCannotCallCanMakePaymentWithoutInit);
    OnConnectionTerminated();
    return;
  }

  // It's valid to call canMakePayment() without calling show() first.

  if (observer_for_testing_)
    observer_for_testing_->OnCanMakePaymentCalled();

  if (!delegate_->GetPrefService()->GetBoolean(kCanMakePaymentEnabled)) {
    CanMakePaymentCallback(/*can_make_payment=*/false);
  } else {
    state_->CanMakePayment(
        base::BindOnce(&PaymentRequest::CanMakePaymentCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void PaymentRequest::HasEnrolledInstrument() {
  if (!IsInitialized()) {
    log_.Error(errors::kCannotCallHasEnrolledInstrumentWithoutInit);
    OnConnectionTerminated();
    return;
  }

  // It's valid to call hasEnrolledInstrument() without calling show() first.

  if (observer_for_testing_)
    observer_for_testing_->OnHasEnrolledInstrumentCalled();

  if (!delegate_->GetPrefService()->GetBoolean(kCanMakePaymentEnabled)) {
    HasEnrolledInstrumentCallback(/*has_enrolled_instrument=*/false);
  } else {
    state_->HasEnrolledInstrument(
        base::BindOnce(&PaymentRequest::HasEnrolledInstrumentCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

bool PaymentRequest::ChangePaymentMethod(const std::string& method_name,
                                         const std::string& stringified_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!method_name.empty());

  if (!state_ || !state_->IsPaymentAppInvoked() || !client_)
    return false;

  client_->OnPaymentMethodChange(method_name, stringified_data);
  return true;
}

bool PaymentRequest::ChangeShippingOption(
    const std::string& shipping_option_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!shipping_option_id.empty());

  bool is_valid_id = false;
  if (spec_->details().shipping_options) {
    for (const auto& option : spec_->GetShippingOptions()) {
      if (option->id == shipping_option_id) {
        is_valid_id = true;
        break;
      }
    }
  }

  if (!state_ || !state_->IsPaymentAppInvoked() || !client_ || !spec_ ||
      !spec_->request_shipping() || !is_valid_id) {
    return false;
  }

  client_->OnShippingOptionChange(shipping_option_id);
  return true;
}

bool PaymentRequest::ChangeShippingAddress(
    mojom::PaymentAddressPtr shipping_address) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(shipping_address);

  if (!state_ || !state_->IsPaymentAppInvoked() || !client_ || !spec_ ||
      !spec_->request_shipping()) {
    return false;
  }

  client_->OnShippingAddressChange(
      RedactShippingAddress(std::move(shipping_address)));
  return true;
}

void PaymentRequest::AreRequestedMethodsSupportedCallback(
    bool methods_supported,
    const std::string& error_message) {
  if (is_show_called_ && spec_ && spec_->IsInitialized() &&
      observer_for_testing_) {
    observer_for_testing_->OnAppListReady(weak_ptr_factory_.GetWeakPtr());
  }

  if (methods_supported) {
    if (SatisfiesSkipUIConstraints())
      Pay();
  } else {
    DCHECK(!has_recorded_completion_);
    has_recorded_completion_ = true;
    journey_logger_.SetNotShown(
        JourneyLogger::NOT_SHOWN_REASON_NO_SUPPORTED_PAYMENT_METHOD);
    client_->OnError(mojom::PaymentErrorReason::NOT_SUPPORTED,
                     GetNotSupportedErrorMessage(
                         spec_ ? spec_->payment_method_identifiers_set()
                               : std::set<std::string>()) +
                         (error_message.empty() ? "" : " " + error_message));
    if (observer_for_testing_)
      observer_for_testing_->OnNotSupportedError();
    OnConnectionTerminated();
  }
}

base::WeakPtr<PaymentRequest> PaymentRequest::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PaymentRequest::OnInitialized(InitializationTask* initialization_task) {
  DCHECK_EQ(spec_.get(), initialization_task);
  DCHECK_EQ(PaymentRequestSpec::UpdateReason::INITIAL_PAYMENT_DETAILS,
            spec_->current_update_reason());
  if (is_show_called_ && state_ && state_->is_get_all_apps_finished() &&
      observer_for_testing_) {
    observer_for_testing_->OnAppListReady(weak_ptr_factory_.GetWeakPtr());
  }
}

bool PaymentRequest::IsInitialized() const {
  return is_initialized_ && client_ && client_.is_bound() &&
         receiver_.is_bound() && state_ && spec_;
}

bool PaymentRequest::IsThisPaymentRequestShowing() const {
  return is_show_called_ && display_handle_ && spec_ && state_;
}

bool PaymentRequest::OnlySingleAppCanProvideAllRequiredInformation() const {
  DCHECK(state()->IsInitialized());
  DCHECK(spec()->IsInitialized());

  if (!spec()->request_shipping() && !spec()->request_payer_name() &&
      !spec()->request_payer_phone() && !spec()->request_payer_email()) {
    return state()->available_apps().size() == 1 &&
           state()->available_apps().at(0)->type() !=
               PaymentApp::Type::AUTOFILL;
  }

  bool an_app_can_provide_all_info = false;
  for (const auto& app : state()->available_apps()) {
    if ((!spec()->request_shipping() || app->HandlesShippingAddress()) &&
        (!spec()->request_payer_name() || app->HandlesPayerName()) &&
        (!spec()->request_payer_phone() || app->HandlesPayerPhone()) &&
        (!spec()->request_payer_email() || app->HandlesPayerEmail())) {
      // There is another available app that can provide all merchant requested
      // information information.
      if (an_app_can_provide_all_info)
        return false;

      an_app_can_provide_all_info = true;
    }
  }
  return an_app_can_provide_all_info;
}

bool PaymentRequest::SatisfiesSkipUIConstraints() {
  // Only allowing URL based payment apps to skip the payment sheet.
  skipped_payment_request_ui_ =
      !spec()->IsSecurePaymentConfirmationRequested() &&
      (spec()->url_payment_method_identifiers().size() > 0 ||
       delegate_->SkipUiForBasicCard()) &&
      base::FeatureList::IsEnabled(features::kWebPaymentsSingleAppUiSkip) &&
      base::FeatureList::IsEnabled(::features::kServiceWorkerPaymentApps) &&
      is_show_user_gesture_ && state()->IsInitialized() &&
      spec()->IsInitialized() &&
      OnlySingleAppCanProvideAllRequiredInformation() &&
      // The available app should be preselectable.
      state()->selected_app() != nullptr;
  if (skipped_payment_request_ui_) {
    DCHECK(state()->IsInitialized() && spec()->IsInitialized());
    journey_logger_.SetEventOccurred(JourneyLogger::EVENT_SKIPPED_SHOW);
  } else if (state()->IsInitialized() && spec()->IsInitialized()) {
    // Set EVENT_SHOWN only after state() and spec() initialization.
    journey_logger_.SetEventOccurred(JourneyLogger::EVENT_SHOWN);
  }
  return skipped_payment_request_ui_;
}

void PaymentRequest::OnPaymentResponseAvailable(
    mojom::PaymentResponsePtr response) {
  DCHECK(!response->method_name.empty());
  DCHECK(!response->stringified_details.empty());

  journey_logger_.SetEventOccurred(
      JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);

  // Log the correct "selected instrument" metric according to its type and
  // the method name in response.
  DCHECK(state_->selected_app());
  JourneyLogger::Event selected_event =
      JourneyLogger::Event::EVENT_SELECTED_OTHER;
  switch (state_->selected_app()->type()) {
    case PaymentApp::Type::AUTOFILL:
      selected_event = JourneyLogger::Event::EVENT_SELECTED_CREDIT_CARD;
      break;
    case PaymentApp::Type::SERVICE_WORKER_APP:
      // Intentionally fall through.
    case PaymentApp::Type::NATIVE_MOBILE_APP: {
      selected_event = IsGooglePaymentMethod(response->method_name)
                           ? JourneyLogger::Event::EVENT_SELECTED_GOOGLE
                           : JourneyLogger::Event::EVENT_SELECTED_OTHER;
      break;
    }
    case PaymentApp::Type::INTERNAL: {
      if (response->method_name == methods::kSecurePaymentConfirmation) {
        selected_event =
            JourneyLogger::Event::EVENT_SELECTED_SECURE_PAYMENT_CONFIRMATION;
      }
      break;
    }
    case PaymentApp::Type::UNDEFINED:
      NOTREACHED();
      break;
  }
  journey_logger_.SetEventOccurred(selected_event);

  // If currently interactive, show the processing spinner. Autofill payment
  // apps request a CVC, so they are always interactive at this point. A payment
  // handler may elect to be non-interactive by not showing a confirmation page
  // to the user.
  if (delegate_->IsInteractive())
    delegate_->ShowProcessingSpinner();

  client_->OnPaymentResponse(std::move(response));
}

void PaymentRequest::OnPaymentResponseError(const std::string& error_message) {
  journey_logger_.SetEventOccurred(
      JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS);
  RecordFirstAbortReason(JourneyLogger::ABORT_REASON_INSTRUMENT_DETAILS_ERROR);

  reject_show_error_message_ = error_message;
  delegate_->ShowErrorMessage();
  // When the user dismisses the error message, UserCancelled() will reject
  // PaymentRequest.show() with |reject_show_error_message_|.
}

void PaymentRequest::OnShippingOptionIdSelected(
    std::string shipping_option_id) {
  client_->OnShippingOptionChange(shipping_option_id);
}

void PaymentRequest::OnShippingAddressSelected(
    mojom::PaymentAddressPtr address) {
  client_->OnShippingAddressChange(RedactShippingAddress(std::move(address)));
}

void PaymentRequest::OnPayerInfoSelected(mojom::PayerDetailPtr payer_info) {
  client_->OnPayerDetailChange(std::move(payer_info));
}

void PaymentRequest::UserCancelled() {
  // If |client_| is not bound, then the object is already being destroyed as
  // a result of a renderer event.
  if (!client_.is_bound())
    return;

  RecordFirstAbortReason(JourneyLogger::ABORT_REASON_ABORTED_BY_USER);

  // This sends an error to the renderer, which informs the API user.
  client_->OnError(mojom::PaymentErrorReason::USER_CANCEL,
                   !reject_show_error_message_.empty()
                       ? reject_show_error_message_
                       : errors::kUserCancelled);

  // We close all bindings and ask to be destroyed.
  client_.reset();
  receiver_.reset();
  payment_handler_host_->Disconnect();
  if (observer_for_testing_)
    observer_for_testing_->OnConnectionTerminated();
  manager_->DestroyRequest(weak_ptr_factory_.GetWeakPtr());
}

void PaymentRequest::DidStartMainFrameNavigationToDifferentDocument(
    bool is_user_initiated) {
  RecordFirstAbortReason(is_user_initiated
                             ? JourneyLogger::ABORT_REASON_USER_NAVIGATION
                             : JourneyLogger::ABORT_REASON_MERCHANT_NAVIGATION);
}

void PaymentRequest::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_EQ(render_frame_host,
            content::RenderFrameHost::FromID(initiator_frame_routing_id_));
  // RenderFrameHost is usually deleted explicitly before PaymentRequest
  // destruction if the user closes the tab or browser window without closing
  // the payment request dialog.
  RecordFirstAbortReason(JourneyLogger::ABORT_REASON_ABORTED_BY_USER);
  // But don't bother sending errors to |client_| because the mojo pipe will be
  // torn down anyways when RenderFrameHost is destroyed. It's not safe to call
  // UserCancelled() here because it is not re-entrant.
  // TODO(crbug.com/1121841) Make UserCancelled re-entrant.
  OnConnectionTerminated();
}

void PaymentRequest::OnConnectionTerminated() {
  // We are here because of a browser-side error, or likely as a result of the
  // disconnect_handler on |receiver_|, which can mean that the renderer
  // has decided to close the pipe for various reasons (see all uses of
  // PaymentRequest::clearResolversAndCloseMojoConnection() in Blink). We close
  // the binding and the dialog, and ask to be deleted.
  client_.reset();
  receiver_.reset();
  payment_handler_host_->Disconnect();
  delegate_->CloseDialog();
  if (observer_for_testing_)
    observer_for_testing_->OnConnectionTerminated();

  RecordFirstAbortReason(JourneyLogger::ABORT_REASON_MOJO_CONNECTION_ERROR);
  manager_->DestroyRequest(weak_ptr_factory_.GetWeakPtr());
}

void PaymentRequest::Pay() {
  journey_logger_.SetEventOccurred(JourneyLogger::EVENT_PAY_CLICKED);
  journey_logger_.RecordCheckoutStep(
      JourneyLogger::CheckoutFunnelStep::kPaymentHandlerInvoked);
  DCHECK(state_->selected_app());
  state_->selected_app()->SetPaymentHandlerHost(
      payment_handler_host_->AsWeakPtr());
  state_->GeneratePaymentResponse();
}

void PaymentRequest::HideIfNecessary() {
  display_handle_.reset();
}

bool PaymentRequest::IsOffTheRecord() const {
  return delegate_->IsOffTheRecord();
}

void PaymentRequest::OnPaymentHandlerOpenWindowCalled() {
  DCHECK(state_->selected_app());
  // UKM for payment app origin should get recorded only when the origin of the
  // invoked payment app is shown to the user.
  journey_logger_.SetPaymentAppUkmSourceId(
      state_->selected_app()->UkmSourceId());
}

content::WebContents* PaymentRequest::web_contents() {
  auto* rfh = content::RenderFrameHost::FromID(initiator_frame_routing_id_);
  return rfh && rfh->IsCurrent()
             ? content::WebContents::FromRenderFrameHost(rfh)
             : nullptr;
}

void PaymentRequest::RecordFirstAbortReason(
    JourneyLogger::AbortReason abort_reason) {
  if (!has_recorded_completion_) {
    has_recorded_completion_ = true;
    journey_logger_.SetAborted(abort_reason);
  }
}

void PaymentRequest::CanMakePaymentCallback(bool can_make_payment) {
  client_->OnCanMakePayment(
      can_make_payment ? mojom::CanMakePaymentQueryResult::CAN_MAKE_PAYMENT
                       : mojom::CanMakePaymentQueryResult::CANNOT_MAKE_PAYMENT);

  journey_logger_.SetCanMakePaymentValue(can_make_payment);

  if (observer_for_testing_)
    observer_for_testing_->OnCanMakePaymentReturned();
}

void PaymentRequest::HasEnrolledInstrumentCallback(
    bool has_enrolled_instrument) {
  auto* rfh = content::RenderFrameHost::FromID(initiator_frame_routing_id_);
  if (!rfh)
    return;

  if (!spec_ || CanMakePaymentQueryFactory::GetInstance()
                    ->GetForContext(rfh->GetBrowserContext())
                    ->CanQuery(top_level_origin_, frame_origin_,
                               spec_->query_for_quota())) {
    RespondToHasEnrolledInstrumentQuery(has_enrolled_instrument,
                                        /*warn_local_development=*/false);
  } else if (UrlUtil::IsLocalDevelopmentUrl(frame_origin_)) {
    RespondToHasEnrolledInstrumentQuery(has_enrolled_instrument,
                                        /*warn_local_development=*/true);
  } else {
    client_->OnHasEnrolledInstrument(
        HasEnrolledInstrumentQueryResult::QUERY_QUOTA_EXCEEDED);
  }

  if (observer_for_testing_)
    observer_for_testing_->OnHasEnrolledInstrumentReturned();
}

void PaymentRequest::RespondToHasEnrolledInstrumentQuery(
    bool has_enrolled_instrument,
    bool warn_local_development) {
  HasEnrolledInstrumentQueryResult positive =
      warn_local_development
          ? HasEnrolledInstrumentQueryResult::WARNING_HAS_ENROLLED_INSTRUMENT
          : HasEnrolledInstrumentQueryResult::HAS_ENROLLED_INSTRUMENT;
  HasEnrolledInstrumentQueryResult negative =
      warn_local_development
          ? HasEnrolledInstrumentQueryResult::WARNING_HAS_NO_ENROLLED_INSTRUMENT
          : HasEnrolledInstrumentQueryResult::HAS_NO_ENROLLED_INSTRUMENT;

  client_->OnHasEnrolledInstrument(has_enrolled_instrument ? positive
                                                           : negative);
  journey_logger_.SetHasEnrolledInstrumentValue(has_enrolled_instrument);
}

void PaymentRequest::OnAbortResult(bool aborted) {
  if (client_.is_bound())
    client_->OnAbort(aborted);

  if (aborted) {
    RecordFirstAbortReason(JourneyLogger::ABORT_REASON_ABORTED_BY_MERCHANT);
    state_->OnAbort();
  }
}

}  // namespace payments
