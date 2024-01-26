// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android_app_communication_helpers.h"

#include "chromeos/components/payments/mojom/payment_app.mojom.h"
#include "chromeos/components/payments/mojom/payment_app_types.mojom.h"
#include "components/payments/core/chrome_os_error_strings.h"
#include "components/payments/core/method_strings.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace payments {

const char kEmptyDictionaryJson[] = "{}";

void OnIsImplemented(
    const std::string& twa_package_name,
    AndroidAppCommunication::GetAppDescriptionsCallback callback,
    chromeos::payments::mojom::IsPaymentImplementedResultPtr response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!twa_package_name.empty());

  if (response.is_null()) {
    std::move(callback).Run(errors::kEmptyResponse, /*app_descriptions=*/{});
    return;
  }

  if (response->is_error()) {
    std::move(callback).Run(response->get_error(), /*app_descriptions=*/{});
    return;
  }

  if (!response->is_valid()) {
    std::move(callback).Run(errors::kInvalidResponse, /*app_descriptions=*/{});
    return;
  }

  if (response->get_valid()->activity_names.empty()) {
    // If a TWA does not implement PAY intent in any of its activities, then
    // |activity_names| is empty, which is not an error.
    std::move(callback).Run(/*error_message=*/std::nullopt,
                            /*app_descriptions=*/{});
    return;
  }

  if (response->get_valid()->activity_names.size() != 1U) {
    std::move(callback).Run(errors::kMoreThanOneActivity,
                            /*app_descriptions=*/{});
    return;
  }

  auto activity = std::make_unique<AndroidActivityDescription>();
  activity->name = response->get_valid()->activity_names.front();

  // The only available payment method identifier in a Chrome OS TWA at this
  // time.
  activity->default_payment_method = methods::kGooglePlayBilling;

  auto app = std::make_unique<AndroidAppDescription>();
  app->package = twa_package_name;
  app->activities.emplace_back(std::move(activity));
  app->service_names = response->get_valid()->service_names;

  std::vector<std::unique_ptr<AndroidAppDescription>> app_descriptions;
  app_descriptions.emplace_back(std::move(app));

  std::move(callback).Run(/*error_message=*/std::nullopt,
                          std::move(app_descriptions));
}

void OnIsReadyToPay(AndroidAppCommunication::IsReadyToPayCallback callback,
                    chromeos::payments::mojom::IsReadyToPayResultPtr response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (response.is_null()) {
    std::move(callback).Run(errors::kEmptyResponse, /*is_ready_to_pay=*/false);
    return;
  }

  if (response->is_error()) {
    std::move(callback).Run(response->get_error(), /*is_ready_to_pay=*/false);
    return;
  }

  if (!response->is_response()) {
    std::move(callback).Run(errors::kInvalidResponse,
                            /*is_ready_to_pay=*/false);
    return;
  }

  std::move(callback).Run(/*error_message=*/std::nullopt,
                          response->get_response());
}

void OnPaymentAppResponse(
    AndroidAppCommunication::InvokePaymentAppCallback callback,
    base::ScopedClosureRunner overlay_state,
    chromeos::payments::mojom::InvokePaymentAppResultPtr response) {
  if (overlay_state) {
    // Dismiss and prevent any further overlays
    overlay_state.RunAndReset();
  }

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (response.is_null()) {
    std::move(callback).Run(errors::kEmptyResponse,
                            /*is_activity_result_ok=*/false,
                            /*payment_method_identifier=*/"",
                            /*stringified_details=*/kEmptyDictionaryJson);
    return;
  }

  if (response->is_error()) {
    std::move(callback).Run(response->get_error(),
                            /*is_activity_result_ok=*/false,
                            /*payment_method_identifier=*/"",
                            /*stringified_details=*/kEmptyDictionaryJson);
    return;
  }

  if (!response->is_valid()) {
    std::move(callback).Run(errors::kInvalidResponse,
                            /*is_activity_result_ok=*/false,
                            /*payment_method_identifier=*/"",
                            /*stringified_details=*/kEmptyDictionaryJson);
    return;
  }

  // Chrome OS TWA currently supports only methods::kGooglePlayBilling payment
  // method identifier.
  std::move(callback).Run(
      /*error_message=*/std::nullopt,
      response->get_valid()->is_activity_result_ok,
      /*payment_method_identifier=*/methods::kGooglePlayBilling,
      response->get_valid()->stringified_details);
}

chromeos::payments::mojom::PaymentParametersPtr CreatePaymentParameters(
    const std::string& package_name,
    const std::string& activity_or_service_name,
    const std::map<std::string, std::set<std::string>>& stringified_method_data,
    const GURL& top_level_origin,
    const GURL& payment_request_origin,
    const std::string& payment_request_id,
    std::optional<std::string>* error_message) {
  // Chrome OS TWA supports only kGooglePlayBilling payment method identifier
  // at this time.
  auto supported_method_iterator =
      stringified_method_data.find(methods::kGooglePlayBilling);
  if (supported_method_iterator == stringified_method_data.end()) {
    return nullptr;
  }

  // Chrome OS TWA supports only one set of payment method specific data.
  if (supported_method_iterator->second.size() > 1) {
    *error_message = errors::kMoreThanOneMethodData;
    return nullptr;
  }

  auto parameters = chromeos::payments::mojom::PaymentParameters::New();
  parameters->stringified_method_data =
      supported_method_iterator->second.empty()
          ? kEmptyDictionaryJson
          : *supported_method_iterator->second.begin();
  parameters->package_name = package_name;
  parameters->activity_or_service_name = activity_or_service_name;
  parameters->top_level_origin = top_level_origin.spec();
  parameters->payment_request_origin = payment_request_origin.spec();
  parameters->payment_request_id = payment_request_id;

  return parameters;
}

std::vector<std::unique_ptr<AndroidAppDescription>> CreateAppForTesting(
    const std::string& package_name,
    const std::string& method_name) {
  auto activity = std::make_unique<AndroidActivityDescription>();
  activity->name = package_name + ".Activity";
  activity->default_payment_method = method_name;
  auto app = std::make_unique<AndroidAppDescription>();
  app->package = package_name;
  app->activities.emplace_back(std::move(activity));
  std::vector<std::unique_ptr<AndroidAppDescription>> app_descriptions;
  app_descriptions.emplace_back(std::move(app));
  return app_descriptions;
}

}  // namespace payments
