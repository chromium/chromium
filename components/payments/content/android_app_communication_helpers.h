// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_APP_COMMUNICATION_HELPERS_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_APP_COMMUNICATION_HELPERS_H_

#include <map>
#include <optional>
#include <set>
#include <string>

#include "base/functional/callback_helpers.h"
#include "chromeos/components/payments/mojom/payment_app_types.mojom-forward.h"
#include "components/payments/content/android_app_communication.h"

class GURL;

namespace payments {

// Empty Json formatted string, used for error responses.
extern const char kEmptyDictionaryJson[];

// Callback that processes the response from
// PaymentAppInstance::IsPaymentImplemented request.
void OnIsImplemented(
    const std::string& twa_package_name,
    AndroidAppCommunication::GetAppDescriptionsCallback callback,
    chromeos::payments::mojom::IsPaymentImplementedResultPtr response);

// Callback that processes the response from PaymentAppInstance::IsReadyToPay
// request.
void OnIsReadyToPay(AndroidAppCommunication::IsReadyToPayCallback callback,
                    chromeos::payments::mojom::IsReadyToPayResultPtr response);

// Callback that processes the response from
// PaymentAppInstance::InvokePaymentApp request.
void OnPaymentAppResponse(
    AndroidAppCommunication::InvokePaymentAppCallback callback,
    base::ScopedClosureRunner overlay_state,
    chromeos::payments::mojom::InvokePaymentAppResultPtr response);

// Creates the payment parameters for payment requests to the
// PaymentAppInstance.
chromeos::payments::mojom::PaymentParametersPtr CreatePaymentParameters(
    const std::string& package_name,
    const std::string& activity_or_service_name,
    const std::map<std::string, std::set<std::string>>& stringified_method_data,
    const GURL& top_level_origin,
    const GURL& payment_request_origin,
    const std::string& payment_request_id,
    std::optional<std::string>* error_message);

// Create fake app descriptions for AndroidAppCommunication::GetAppDescriptions
// interface to support tests.
std::vector<std::unique_ptr<AndroidAppDescription>> CreateAppForTesting(
    const std::string& package_name,
    const std::string& method_name);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_APP_COMMUNICATION_HELPERS_H_
