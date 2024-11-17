// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_handler_host.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "components/payments/core/error_strings.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/payment_address.h"
#include "components/payments/core/payments_validators.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_background_services_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace payments {
namespace {

content::DevToolsBackgroundServicesContext* GetDevTools(
    content::WebContents* web_contents,
    const url::Origin& sw_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents)
    return nullptr;

  auto* storage_partition =
      web_contents->GetBrowserContext()->GetStoragePartitionForUrl(
          sw_origin.GetURL(), /*can_create=*/true);
  if (!storage_partition)
    return nullptr;

  auto* dev_tools = storage_partition->GetDevToolsBackgroundServicesContext();
  return dev_tools && dev_tools->IsRecording(
                          content::DevToolsBackgroundService::kPaymentHandler)
             ? dev_tools
             : nullptr;
}

// Generates a PaymentResponse with the given error, then runs the provided
// callback.
void RunCallbackWithError(const std::string& error,
                          ChangePaymentRequestDetailsCallback callback) {
  mojom::PaymentRequestDetailsUpdatePtr response =
      mojom::PaymentRequestDetailsUpdate::New();
  response->error = error;
  std::move(callback).Run(std::move(response));
}

}  // namespace

PaymentHandlerHost::PaymentHandlerHost(content::WebContents* web_contents,
                                       base::WeakPtr<Delegate> delegate)
    : delegate_(delegate) {
  DCHECK(web_contents);
  DCHECK(delegate_);
  web_contents_ = web_contents->GetWeakPtr();
}

PaymentHandlerHost::~PaymentHandlerHost() = default;

mojo::PendingRemote<mojom::PaymentHandlerHost> PaymentHandlerHost::Bind() {
  receiver_.reset();
  mojo::PendingRemote<mojom::PaymentHandlerHost> host =
      receiver_.BindNewPipeAndPassRemote();

  // Connection error handler can be set only after the Bind() call.
  receiver_.set_disconnect_handler(base::BindOnce(
      &PaymentHandlerHost::Disconnect, weak_ptr_factory_.GetWeakPtr()));

  return host;
}

void PaymentHandlerHost::UpdateWith(
    mojom::PaymentRequestDetailsUpdatePtr response) {
  if (!change_payment_request_details_callback_)
    return;

  auto* dev_tools = GetDevTools(web_contents_.get(), sw_origin_for_logs_);
  if (dev_tools) {
    std::map<std::string, std::string> data = {{"Error", response->error}};

    if (response->total) {
      data["Total Currency"] = response->total->currency;
      data["Total Value"] = response->total->value;
    }

    if (response->stringified_payment_method_errors) {
      data["Payment Method Errors"] =
          *response->stringified_payment_method_errors;
    }

    if (response->shipping_address_errors) {
      data["Shipping Address Address Line Error"] =
          response->shipping_address_errors->address_line;
      data["Shipping Address City Error"] =
          response->shipping_address_errors->city;
      data["Shipping Address Country Error"] =
          response->shipping_address_errors->country;
      data["Shipping Address Dependent Locality Error"] =
          response->shipping_address_errors->dependent_locality;
      data["Shipping Address Organization Error"] =
          response->shipping_address_errors->organization;
      data["Shipping Address Phone Error"] =
          response->shipping_address_errors->phone;
      data["Shipping Address Postal Code Error"] =
          response->shipping_address_errors->postal_code;
      data["Shipping Address Recipient Error"] =
          response->shipping_address_errors->recipient;
      data["Shipping Address Region Error"] =
          response->shipping_address_errors->region;
      data["Shipping Address Sorting Code Error"] =
          response->shipping_address_errors->sorting_code;
    }

    if (response->modifiers) {
      for (size_t i = 0; i < response->modifiers->size(); ++i) {
        std::string prefix =
            "Modifier" + (response->modifiers->size() == 1
                              ? ""
                              : " #" + base::NumberToString(i));
        const auto& modifier = response->modifiers->at(i);
        data.emplace(prefix + " Method Name",
                     modifier->method_data->method_name);
        data.emplace(prefix + " Method Data",
                     modifier->method_data->stringified_data.value_or("{}"));
        if (!modifier->total)
          continue;
        data.emplace(prefix + " Total Currency", modifier->total->currency);
        data.emplace(prefix + " Total Value", modifier->total->value);
      }
    }

    if (response->shipping_options) {
      for (size_t i = 0; i < response->shipping_options->size(); ++i) {
        std::string prefix =
            "Shipping Option" + (response->shipping_options->size() == 1
                                     ? ""
                                     : " #" + base::NumberToString(i));
        const auto& option = response->shipping_options->at(i);
        data.emplace(prefix + " Id", option->id);
        data.emplace(prefix + " Label", option->label);
        data.emplace(prefix + " Amount Currency", option->amount->currency);
        data.emplace(prefix + " Amount Value", option->amount->value);
        data.emplace(prefix + " Selected", option->selected ? "true" : "false");
      }
    }

    dev_tools->LogBackgroundServiceEvent(
        registration_id_for_logs_,
        blink::StorageKey::CreateFirstParty(sw_origin_for_logs_),
        content::DevToolsBackgroundService::kPaymentHandler, "Update with",
        /*instance_id=*/payment_request_id_for_logs_, data);
  }

  std::move(change_payment_request_details_callback_).Run(std::move(response));
}

void PaymentHandlerHost::OnPaymentDetailsNotUpdated() {
  if (!change_payment_request_details_callback_)
    return;

  std::move(change_payment_request_details_callback_)
      .Run(mojom::PaymentRequestDetailsUpdate::New());
}

void PaymentHandlerHost::Disconnect() {
  receiver_.reset();
}

base::WeakPtr<PaymentHandlerHost> PaymentHandlerHost::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PaymentHandlerHost::ChangePaymentMethod(
    mojom::PaymentHandlerMethodDataPtr method_data,
    ChangePaymentRequestDetailsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!method_data) {
    RunCallbackWithError(errors::kMethodDataRequired, std::move(callback));
    return;
  }

  if (method_data->method_name.empty()) {
    RunCallbackWithError(errors::kMethodNameRequired, std::move(callback));
    return;
  }

  const std::string stringified_data =
      method_data->stringified_data.value_or("{}");
  if (!delegate_->ChangePaymentMethod(method_data->method_name,
                                      stringified_data)) {
    RunCallbackWithError(errors::kInvalidState, std::move(callback));
    return;
  }

  auto* dev_tools = GetDevTools(web_contents_.get(), sw_origin_for_logs_);
  if (dev_tools) {
    dev_tools->LogBackgroundServiceEvent(
        registration_id_for_logs_,
        blink::StorageKey::CreateFirstParty(sw_origin_for_logs_),
        content::DevToolsBackgroundService::kPaymentHandler,
        "Change payment method",
        /*instance_id=*/payment_request_id_for_logs_,
        {{"Method Name", method_data->method_name},
         {"Method Data", stringified_data}});
  }

  change_payment_request_details_callback_ = std::move(callback);
}

void PaymentHandlerHost::ChangeShippingOption(
    const std::string& shipping_option_id,
    ChangePaymentRequestDetailsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (shipping_option_id.empty()) {
    RunCallbackWithError(errors::kShippingOptionIdRequired,
                         std::move(callback));
    return;
  }

  if (!delegate_->ChangeShippingOption(shipping_option_id)) {
    RunCallbackWithError(errors::kInvalidState, std::move(callback));
    return;
  }

  auto* dev_tools = GetDevTools(web_contents_.get(), sw_origin_for_logs_);
  if (dev_tools) {
    dev_tools->LogBackgroundServiceEvent(
        registration_id_for_logs_,
        blink::StorageKey::CreateFirstParty(sw_origin_for_logs_),
        content::DevToolsBackgroundService::kPaymentHandler,
        "Change shipping option",
        /*instance_id=*/payment_request_id_for_logs_,
        {{"Shipping Option Id", shipping_option_id}});
  }

  change_payment_request_details_callback_ = std::move(callback);
}

void PaymentHandlerHost::ChangeShippingAddress(
    mojom::PaymentAddressPtr shipping_address,
    ChangePaymentRequestDetailsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!shipping_address || !PaymentsValidators::IsValidCountryCodeFormat(
                               shipping_address->country, nullptr)) {
    RunCallbackWithError(errors::kShippingAddressInvalid, std::move(callback));
    return;
  }

  if (!delegate_->ChangeShippingAddress(shipping_address.Clone())) {
    RunCallbackWithError(errors::kInvalidState, std::move(callback));
    return;
  }

  auto* dev_tools = GetDevTools(web_contents_.get(), sw_origin_for_logs_);
  if (dev_tools) {
    std::map<std::string, std::string> shipping_address_map;
    shipping_address_map.emplace("Country", shipping_address->country);
    for (size_t i = 0; i < shipping_address->address_line.size(); ++i) {
      std::string key =
          "Address Line" + (shipping_address->address_line.size() == 1
                                ? ""
                                : " #" + base::NumberToString(i));
      shipping_address_map.emplace(key, shipping_address->address_line[i]);
    }

    shipping_address_map.emplace("Region", shipping_address->region);
    shipping_address_map.emplace("City", shipping_address->city);
    shipping_address_map.emplace("Dependent Locality",
                                 shipping_address->dependent_locality);
    shipping_address_map.emplace("Postal Code", shipping_address->postal_code);
    shipping_address_map.emplace("Sorting Code",
                                 shipping_address->sorting_code);
    shipping_address_map.emplace("Organization",
                                 shipping_address->organization);
    shipping_address_map.emplace("Recipient", shipping_address->recipient);
    shipping_address_map.emplace("Phone", shipping_address->phone);

    dev_tools->LogBackgroundServiceEvent(
        registration_id_for_logs_,
        blink::StorageKey::CreateFirstParty(sw_origin_for_logs_),
        content::DevToolsBackgroundService::kPaymentHandler,
        "Change shipping address",
        /*instance_id=*/payment_request_id_for_logs_, shipping_address_map);
  }

  change_payment_request_details_callback_ = std::move(callback);
}

}  // namespace payments
