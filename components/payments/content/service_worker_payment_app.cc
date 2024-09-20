// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/service_worker_payment_app.h"

#include <limits>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/payments/content/payment_event_response_util.h"
#include "components/payments/content/payment_handler_host.h"
#include "components/payments/content/payment_request_converter.h"
#include "components/payments/core/features.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/pre_purchase_query.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/payment_app_provider_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "ui/gfx/image/image_skia.h"
#include "url/origin.h"

namespace payments {

// Service worker payment app provides icon through bitmap, so set 0 as invalid
// resource Id.
ServiceWorkerPaymentApp::ServiceWorkerPaymentApp(
    content::WebContents* web_contents,
    const GURL& top_origin,
    const GURL& frame_origin,
    base::WeakPtr<PaymentRequestSpec> spec,
    std::unique_ptr<content::StoredPaymentApp> stored_payment_app_info,
    bool is_incognito,
    const base::RepeatingClosure& show_processing_spinner)
    : PaymentApp(0, PaymentApp::Type::SERVICE_WORKER_APP),
      top_origin_(top_origin),
      frame_origin_(frame_origin),
      spec_(spec),
      stored_payment_app_info_(std::move(stored_payment_app_info)),
      delegate_(nullptr),
      is_incognito_(is_incognito),
      show_processing_spinner_(show_processing_spinner),
      can_make_payment_result_(false),
      has_enrolled_instrument_result_(false),
      needs_installation_(false),
      web_contents_(web_contents->GetWeakPtr()) {
  DCHECK(web_contents);
  DCHECK(top_origin_.is_valid());
  DCHECK(frame_origin_.is_valid());

  app_method_names_.insert(stored_payment_app_info_->enabled_methods.begin(),
                           stored_payment_app_info_->enabled_methods.end());
}

// Service worker payment app provides icon through bitmap, so set 0 as invalid
// resource Id.
ServiceWorkerPaymentApp::ServiceWorkerPaymentApp(
    content::WebContents* web_contents,
    const GURL& top_origin,
    const GURL& frame_origin,
    base::WeakPtr<PaymentRequestSpec> spec,
    std::unique_ptr<WebAppInstallationInfo> installable_payment_app_info,
    const std::string& enabled_method,
    bool is_incognito,
    const base::RepeatingClosure& show_processing_spinner)
    : PaymentApp(0, PaymentApp::Type::SERVICE_WORKER_APP),
      top_origin_(top_origin),
      frame_origin_(frame_origin),
      spec_(spec),
      delegate_(nullptr),
      is_incognito_(is_incognito),
      show_processing_spinner_(show_processing_spinner),
      can_make_payment_result_(false),
      has_enrolled_instrument_result_(false),
      needs_installation_(true),
      installable_web_app_info_(std::move(installable_payment_app_info)),
      installable_enabled_method_(enabled_method),
      web_contents_(web_contents->GetWeakPtr()) {
  DCHECK(web_contents);
  DCHECK(top_origin_.is_valid());
  DCHECK(frame_origin_.is_valid());

  app_method_names_.insert(installable_enabled_method_);
}

ServiceWorkerPaymentApp::~ServiceWorkerPaymentApp() {
  if (delegate_) {
    // If there's a payment in progress, abort it before destroying this so that
    // it can update its internal state. Since the PaymentRequest will be
    // destroyed, pass an empty callback to the payment app.
    AbortPaymentApp(/*abort_callback=*/base::DoNothing());
  }
}

void ServiceWorkerPaymentApp::ValidateCanMakePayment(
    ValidateCanMakePaymentCallback callback) {
  if (!spec_)
    return;

  // Returns true for payment app that needs installation.
  if (needs_installation_) {
    OnCanMakePaymentEventSkipped(std::move(callback));
    return;
  }

  // Returns true if we are in incognito (avoiding sending the event to the
  // payment handler).
  if (is_incognito_) {
    OnCanMakePaymentEventSkipped(std::move(callback));
    return;
  }

  // Do not send CanMakePayment event to payment apps that have not been
  // explicitly verified.
  if (!stored_payment_app_info_->has_explicitly_verified_methods) {
    OnCanMakePaymentEventSkipped(std::move(callback));
    return;
  }

  mojom::CanMakePaymentEventDataPtr event_data =
      CreateCanMakePaymentEventData();
  if (event_data.is_null()) {
    // This could only happen if this app only supports non-url based payment
    // methods of the payment request, then return true and do not send
    // CanMakePaymentEvent to the payment app.
    OnCanMakePaymentEventSkipped(std::move(callback));
    return;
  }

  auto* payment_app_provider = GetPaymentAppProvider();
  if (!payment_app_provider)
    return;

  base::UmaHistogramEnumeration("PaymentRequest.PrePurchaseQuery",
                                PrePurchaseQuery::kServiceWorkerEvent);
  payment_app_provider->CanMakePayment(
      stored_payment_app_info_->registration_id,
      url::Origin::Create(stored_payment_app_info_->scope),
      *spec_->details().id, std::move(event_data),
      base::BindOnce(&ServiceWorkerPaymentApp::OnCanMakePaymentEventResponded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

mojom::CanMakePaymentEventDataPtr
ServiceWorkerPaymentApp::CreateCanMakePaymentEventData() {
  if (!spec_)
    return nullptr;

  std::set<std::string> requested_url_methods;
  for (const auto& method : spec_->payment_method_identifiers_set()) {
    GURL url_method(method);
    if (url_method.is_valid()) {
      requested_url_methods.insert(method);
    }
  }
  std::set<std::string> supported_url_methods =
      base::STLSetIntersection<std::set<std::string>>(requested_url_methods,
                                                      GetAppMethodNames());
  // Only fire CanMakePaymentEvent if this app supports non-url based payment
  // methods of the payment request.
  if (supported_url_methods.empty())
    return nullptr;

  mojom::CanMakePaymentEventDataPtr event_data =
      mojom::CanMakePaymentEventData::New();

  event_data->top_origin = top_origin_;
  event_data->payment_request_origin = frame_origin_;

  DCHECK(spec_->details().modifiers);
  for (const auto& modifier : *spec_->details().modifiers) {
    if (base::Contains(supported_url_methods,
                       modifier->method_data->supported_method)) {
      event_data->modifiers.emplace_back(modifier.Clone());
    }
  }

  for (const auto& data : spec_->method_data()) {
    if (base::Contains(supported_url_methods, data->supported_method)) {
      event_data->method_data.push_back(data.Clone());
    }
  }

  return event_data;
}

void ServiceWorkerPaymentApp::OnCanMakePaymentEventSkipped(
    ValidateCanMakePaymentCallback callback) {
  // |can_make_payment| is true as long as there is a matching payment handler.
  can_make_payment_result_ = true;
  has_enrolled_instrument_result_ = false;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerPaymentApp::CallValidateCanMakePaymentCallback,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerPaymentApp::OnCanMakePaymentEventResponded(
    ValidateCanMakePaymentCallback callback,
    mojom::CanMakePaymentResponsePtr response) {
  // |can_make_payment| is true as long as there is a matching payment handler.
  can_make_payment_result_ = true;
  has_enrolled_instrument_result_ = response->can_make_payment;
  base::UmaHistogramBoolean("PaymentRequest.EventResponse.CanMakePayment",
                            response->can_make_payment);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerPaymentApp::CallValidateCanMakePaymentCallback,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerPaymentApp::CallValidateCanMakePaymentCallback(
    ValidateCanMakePaymentCallback callback) {
  std::move(callback).Run(weak_ptr_factory_.GetWeakPtr());
}

void ServiceWorkerPaymentApp::InvokePaymentApp(
    base::WeakPtr<Delegate> delegate) {
  delegate_ = delegate;
  auto* payment_app_provider = GetPaymentAppProvider();
  if (!payment_app_provider)
    return;

  if (needs_installation_) {
    payment_app_provider->InstallAndInvokePaymentApp(
        CreatePaymentRequestEventData(), installable_web_app_info_->name,
        installable_web_app_info_->icon == nullptr
            ? SkBitmap()
            : *(installable_web_app_info_->icon),
        GURL(installable_web_app_info_->sw_js_url),
        GURL(installable_web_app_info_->sw_scope),
        installable_web_app_info_->sw_use_cache, installable_enabled_method_,
        installable_web_app_info_->supported_delegations,
        base::BindOnce(
            &ServiceWorkerPaymentApp::OnPaymentAppIdentity,
            weak_ptr_factory_.GetWeakPtr(),
            url::Origin::Create(GURL(installable_web_app_info_->sw_scope))),
        base::BindOnce(&ServiceWorkerPaymentApp::OnPaymentAppResponse,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    url::Origin sw_origin =
        url::Origin::Create(stored_payment_app_info_->scope);
    OnPaymentAppIdentity(sw_origin, stored_payment_app_info_->registration_id);
    payment_app_provider->InvokePaymentApp(
        stored_payment_app_info_->registration_id, sw_origin,
        CreatePaymentRequestEventData(),
        base::BindOnce(&ServiceWorkerPaymentApp::OnPaymentAppResponse,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  show_processing_spinner_.Run();
}

void ServiceWorkerPaymentApp::OnPaymentAppWindowClosed() {
  delegate_ = nullptr;
  auto* payment_app_provider = GetPaymentAppProvider();
  if (!payment_app_provider)
    return;
  payment_app_provider->OnClosingOpenedWindow(
      mojom::PaymentEventResponseType::PAYMENT_HANDLER_WINDOW_CLOSING);
}

mojom::PaymentRequestEventDataPtr
ServiceWorkerPaymentApp::CreatePaymentRequestEventData() {
  mojom::PaymentRequestEventDataPtr event_data =
      mojom::PaymentRequestEventData::New();

  if (!spec_)
    return event_data;

  event_data->top_origin = top_origin_;
  event_data->payment_request_origin = frame_origin_;

  if (spec_->details().id.has_value())
    event_data->payment_request_id = spec_->details().id.value();

  event_data->total = spec_->details().total->amount.Clone();

  DCHECK(spec_->details().modifiers);
  for (const auto& modifier : *spec_->details().modifiers) {
    if (base::Contains(GetAppMethodNames(),
                       modifier->method_data->supported_method)) {
      event_data->modifiers.emplace_back(modifier.Clone());
    }
  }

  for (const auto& data : spec_->method_data()) {
    if (base::Contains(GetAppMethodNames(), data->supported_method)) {
      event_data->method_data.push_back(data.Clone());
    }
  }

  if (HandlesPayerName()) {
    DCHECK(spec_->request_payer_name());

    if (!event_data->payment_options)
      event_data->payment_options = mojom::PaymentOptions::New();

    event_data->payment_options->request_payer_name = true;
  }

  if (HandlesPayerEmail()) {
    DCHECK(spec_->request_payer_email());

    if (!event_data->payment_options)
      event_data->payment_options = mojom::PaymentOptions::New();

    event_data->payment_options->request_payer_email = true;
  }

  if (HandlesPayerPhone()) {
    DCHECK(spec_->request_payer_phone());

    if (!event_data->payment_options)
      event_data->payment_options = mojom::PaymentOptions::New();

    event_data->payment_options->request_payer_phone = true;
  }

  if (HandlesShippingAddress()) {
    DCHECK(spec_->request_shipping());

    if (!event_data->payment_options)
      event_data->payment_options = mojom::PaymentOptions::New();

    event_data->payment_options->request_shipping = true;
    event_data->payment_options->shipping_type =
        mojom::PaymentShippingType::SHIPPING;
    if (spec_->shipping_type() == PaymentShippingType::DELIVERY) {
      event_data->payment_options->shipping_type =
          mojom::PaymentShippingType::DELIVERY;
    } else if (spec_->shipping_type() == PaymentShippingType::PICKUP) {
      event_data->payment_options->shipping_type =
          mojom::PaymentShippingType::PICKUP;
    }

    event_data->shipping_options =
        std::vector<mojom::PaymentShippingOptionPtr>();
    for (const auto& option : spec_->GetShippingOptions()) {
      event_data->shipping_options->push_back(option.Clone());
    }
  }

  event_data->payment_handler_host = std::move(payment_handler_host_remote_);

  return event_data;
}

void ServiceWorkerPaymentApp::OnPaymentAppResponse(
    mojom::PaymentHandlerResponsePtr response) {
  if (!delegate_)
    return;

  if (response->response_type ==
      mojom::PaymentEventResponseType::PAYMENT_EVENT_SUCCESS) {
    DCHECK(!response->method_name.empty());
    DCHECK(!response->stringified_details.empty());
    delegate_->OnInstrumentDetailsReady(
        response->method_name, response->stringified_details,
        PayerData(response->payer_name.value_or(""),
                  response->payer_email.value_or(""),
                  response->payer_phone.value_or(""),
                  std::move(response->shipping_address),
                  response->shipping_option.value_or("")));
  } else {
    DCHECK(response->method_name.empty());
    DCHECK(response->stringified_details.empty());
    DCHECK(response->payer_name.value_or("").empty());
    DCHECK(response->payer_email.value_or("").empty());
    DCHECK(response->payer_phone.value_or("").empty());
    DCHECK(!response->shipping_address);
    DCHECK(response->shipping_option.value_or("").empty());
    delegate_->OnInstrumentDetailsError(std::string(
        ConvertPaymentEventResponseTypeToErrorString(response->response_type)));
  }

  delegate_ = nullptr;
}

bool ServiceWorkerPaymentApp::IsCompleteForPayment() const {
  return true;
}

bool ServiceWorkerPaymentApp::CanPreselect() const {
  // Do not preselect the payment app when the name and/or icon is missing.
  return !GetLabel().empty() && icon_bitmap() && !icon_bitmap()->drawsNothing();
}

std::u16string ServiceWorkerPaymentApp::GetMissingInfoLabel() const {
  NOTREACHED_IN_MIGRATION();
  return std::u16string();
}

bool ServiceWorkerPaymentApp::HasEnrolledInstrument() const {
  // This app should not be used when can_make_payment_result_ is false, so this
  // interface should not be invoked.
  DCHECK(can_make_payment_result_);
  return has_enrolled_instrument_result_;
}

void ServiceWorkerPaymentApp::RecordUse() {
  NOTIMPLEMENTED();
}

bool ServiceWorkerPaymentApp::NeedsInstallation() const {
  return needs_installation_;
}

std::string ServiceWorkerPaymentApp::GetId() const {
  return needs_installation_ ? installable_web_app_info_->sw_scope
                             : stored_payment_app_info_->scope.spec();
}

std::u16string ServiceWorkerPaymentApp::GetLabel() const {
  return base::UTF8ToUTF16(needs_installation_
                               ? installable_web_app_info_->name
                               : stored_payment_app_info_->name);
}

std::u16string ServiceWorkerPaymentApp::GetSublabel() const {
  if (needs_installation_) {
    DCHECK(GURL(installable_web_app_info_->sw_scope).is_valid());
    return base::UTF8ToUTF16(
        url::Origin::Create(GURL(installable_web_app_info_->sw_scope)).host());
  }
  return base::UTF8ToUTF16(
      url::Origin::Create(stored_payment_app_info_->scope).host());
}

bool ServiceWorkerPaymentApp::IsValidForModifier(
    const std::string& method) const {
  // Payment app that needs installation only supports url based payment
  // methods.
  if (needs_installation_)
    return installable_enabled_method_ == method;

  bool is_valid = false;
  IsValidForPaymentMethodIdentifier(method, &is_valid);
  return is_valid;
}

base::WeakPtr<PaymentApp> ServiceWorkerPaymentApp::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

const SkBitmap* ServiceWorkerPaymentApp::icon_bitmap() const {
  return needs_installation_ ? installable_web_app_info_->icon.get()
                             : stored_payment_app_info_->icon.get();
}

std::set<std::string>
ServiceWorkerPaymentApp::GetApplicationIdentifiersThatHideThisApp() const {
  if (needs_installation_) {
    return std::set<std::string>(
        installable_web_app_info_->preferred_app_ids.begin(),
        installable_web_app_info_->preferred_app_ids.end());
  }

  std::set<std::string> result;
  if (!stored_payment_app_info_->prefer_related_applications)
    return result;

  for (const auto& related : stored_payment_app_info_->related_applications) {
    result.insert(related.id);
  }

  return result;
}

bool ServiceWorkerPaymentApp::HandlesShippingAddress() const {
  if (!spec_ || !spec_->request_shipping())
    return false;

  return needs_installation_
             ? installable_web_app_info_->supported_delegations.shipping_address
             : stored_payment_app_info_->supported_delegations.shipping_address;
}

bool ServiceWorkerPaymentApp::HandlesPayerName() const {
  if (!spec_ || !spec_->request_payer_name())
    return false;

  return needs_installation_
             ? installable_web_app_info_->supported_delegations.payer_name
             : stored_payment_app_info_->supported_delegations.payer_name;
}

bool ServiceWorkerPaymentApp::HandlesPayerEmail() const {
  if (!spec_ || !spec_->request_payer_email())
    return false;

  return needs_installation_
             ? installable_web_app_info_->supported_delegations.payer_email
             : stored_payment_app_info_->supported_delegations.payer_email;
}

bool ServiceWorkerPaymentApp::HandlesPayerPhone() const {
  if (!spec_ || !spec_->request_payer_phone())
    return false;

  return needs_installation_
             ? installable_web_app_info_->supported_delegations.payer_phone
             : stored_payment_app_info_->supported_delegations.payer_phone;
}

void ServiceWorkerPaymentApp::OnPaymentAppIdentity(const url::Origin& origin,
                                                   int64_t registration_id) {
  registration_id_ = registration_id;
  if (payment_handler_host_) {
    payment_handler_host_->set_sw_origin_for_logs(origin);
    payment_handler_host_->set_registration_id_for_logs(registration_id_);
  }
}

ukm::SourceId ServiceWorkerPaymentApp::UkmSourceId() {
  if (ukm_source_id_ == ukm::kInvalidSourceId) {
    GURL sw_scope = needs_installation_
                        ? GURL(installable_web_app_info_->sw_scope)
                        : stored_payment_app_info_->scope;
    // At this point we know that the payment handler window is open for this
    // app since this getter is called for the invoked app inside the
    // PaymentRequest::OnPaymentHandlerOpenWindowCalled function.
    ukm_source_id_ =
        content::PaymentAppProviderUtil::GetSourceIdForPaymentAppFromScope(
            sw_scope.DeprecatedGetOriginAsURL());
  }
  return ukm_source_id_;
}

void ServiceWorkerPaymentApp::SetPaymentHandlerHost(
    base::WeakPtr<PaymentHandlerHost> payment_handler_host) {
  payment_handler_host_ = payment_handler_host;
  payment_handler_host_remote_ = payment_handler_host_->Bind();
}

bool ServiceWorkerPaymentApp::IsWaitingForPaymentDetailsUpdate() const {
  return payment_handler_host_ &&
         payment_handler_host_->is_waiting_for_payment_details_update();
}

void ServiceWorkerPaymentApp::UpdateWith(
    mojom::PaymentRequestDetailsUpdatePtr details_update) {
  if (payment_handler_host_)
    payment_handler_host_->UpdateWith(std::move(details_update));
}

void ServiceWorkerPaymentApp::OnPaymentDetailsNotUpdated() {
  if (payment_handler_host_)
    payment_handler_host_->OnPaymentDetailsNotUpdated();
}

void ServiceWorkerPaymentApp::AbortPaymentApp(
    base::OnceCallback<void(bool)> abort_callback) {
  auto* payment_app_provider = GetPaymentAppProvider();
  if (!spec_ || !payment_app_provider)
    return;

  payment_app_provider->AbortPayment(
      registration_id_,
      stored_payment_app_info_
          ? url::Origin::Create(stored_payment_app_info_->scope)
          : url::Origin::Create(GURL(installable_web_app_info_->sw_scope)),
      *spec_->details().id, std::move(abort_callback));
}

content::PaymentAppProvider* ServiceWorkerPaymentApp::GetPaymentAppProvider() {
  return (!web_contents_)
             ? nullptr
             : content::PaymentAppProvider::GetOrCreateForWebContents(
                   web_contents_.get());
}

}  // namespace payments
