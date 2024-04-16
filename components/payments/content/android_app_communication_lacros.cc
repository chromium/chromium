// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android_app_communication.h"

#include "chromeos/components/payments/mojom/payment_app.mojom.h"
#include "chromeos/components/payments/mojom/payment_app_types.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/payments/content/android_app_communication_helpers.h"
#include "components/payments/core/chrome_os_error_strings.h"
#include "components/payments/core/native_error_strings.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace payments {
namespace {

using chromeos::payments::mojom::PaymentAppInstance;

PaymentAppInstance* GetPaymentAppInstance() {
  auto* service = chromeos::LacrosService::Get();
  if (!service || !service->IsAvailable<PaymentAppInstance>()) {
    LOG(ERROR) << "Lacros service not available.";
    return nullptr;
  }
  return service->GetRemote<PaymentAppInstance>().get();
}

// Invokes the TWA Android app via Ash using Crosapi.
class AndroidAppCommunicationLacros : public AndroidAppCommunication {
 public:
  explicit AndroidAppCommunicationLacros(content::BrowserContext* context)
      : AndroidAppCommunication(context) {}

  ~AndroidAppCommunicationLacros() override = default;

  // AndroidAppCommunication:
  void GetAppDescriptions(const std::string& twa_package_name,
                          GetAppDescriptionsCallback callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (twa_package_name.empty()) {
      // Chrome OS supports Android app payment only through a TWA. An empty
      // `twa_package_name` indicates that Chrome was not launched from a TWA,
      // so there're no payment apps available.
      std::move(callback).Run(/*error_message=*/std::nullopt,
                              /*app_descriptions=*/{});
      return;
    }
    if (!package_name_for_testing_.empty()) {
      std::move(callback).Run(
          /*error_message=*/std::nullopt,
          CreateAppForTesting(package_name_for_testing_, method_for_testing_));
      return;
    }
    PaymentAppInstance* payment_app_instance = GetPaymentAppInstance();
    if (!payment_app_instance) {
      std::move(callback).Run(errors::kUnableToConnectToAsh,
                              /*app_descriptions=*/{});
      return;
    }
    payment_app_instance->IsPaymentImplemented(
        twa_package_name, base::BindOnce(&OnIsImplemented, twa_package_name,
                                         std::move(callback)));
  }

  void IsReadyToPay(const std::string& package_name,
                    const std::string& service_name,
                    const std::map<std::string, std::set<std::string>>&
                        stringified_method_data,
                    const GURL& top_level_origin,
                    const GURL& payment_request_origin,
                    const std::string& payment_request_id,
                    IsReadyToPayCallback callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    PaymentAppInstance* payment_app_instance = GetPaymentAppInstance();
    if (!payment_app_instance) {
      std::move(callback).Run(errors::kUnableToConnectToAsh,
                              /*is_ready_to_pay=*/false);
      return;
    }
    std::optional<std::string> error_message;
    auto parameters = CreatePaymentParameters(
        package_name, service_name, stringified_method_data, top_level_origin,
        payment_request_origin, payment_request_id, &error_message);
    if (!parameters) {
      std::move(callback).Run(error_message, /*is_ready_to_pay=*/false);
      return;
    }
    payment_app_instance->IsReadyToPay(
        std::move(parameters),
        base::BindOnce(&OnIsReadyToPay, std::move(callback)));
  }
  void InvokePaymentApp(
      const std::string& package_name,
      const std::string& activity_name,
      const std::map<std::string, std::set<std::string>>&
          stringified_method_data,
      const GURL& top_level_origin,
      const GURL& payment_request_origin,
      const std::string& payment_request_id,
      const base::UnguessableToken& request_token,
      content::WebContents* web_contents,
      const std::optional<base::UnguessableToken>& twa_instance_identifier,
      InvokePaymentAppCallback callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // TODO(crbug.com/40247053): Ensure the Android Play Billing interface is
    // overlaid on top of the browser window.
    std::optional<std::string> error_message;
    if (package_name_for_testing_ == package_name) {
      std::move(callback).Run(error_message,
                              /*is_activity_result_ok=*/true,
                              method_for_testing_, response_for_testing_);
      return;
    }

    PaymentAppInstance* payment_app_instance = GetPaymentAppInstance();
    if (!payment_app_instance) {
      std::move(callback).Run(errors::kUnableToConnectToAsh,
                              /*is_activity_result_ok=*/false,
                              /*payment_method_identifier=*/"",
                              /*stringified_details=*/kEmptyDictionaryJson);
      return;
    }
    auto parameters = CreatePaymentParameters(
        package_name, activity_name, stringified_method_data, top_level_origin,
        payment_request_origin, payment_request_id, &error_message);
    if (!parameters) {
      std::move(callback).Run(error_message,
                              /*is_activity_result_ok=*/false,
                              /*payment_method_identifier=*/"",
                              /*stringified_details=*/kEmptyDictionaryJson);
      return;
    }
    parameters->request_token = request_token.ToString();
    parameters->twa_instance_identifier = twa_instance_identifier;
    payment_app_instance->InvokePaymentApp(
        std::move(parameters),
        base::BindOnce(&OnPaymentAppResponse, std::move(callback),
                       base::ScopedClosureRunner()));
  }

  void AbortPaymentApp(const base::UnguessableToken& request_token,
                       AbortPaymentAppCallback callback) override {
    PaymentAppInstance* payment_app_instance = GetPaymentAppInstance();
    if (!payment_app_instance) {
      std::move(callback).Run(false);
      return;
    }
    payment_app_instance->AbortPaymentApp(request_token.ToString(),
                                          std::move(callback));
  }

  void SetForTesting() override {}

  void SetAppForTesting(const std::string& package_name,
                        const std::string& method,
                        const std::string& response) override {
    package_name_for_testing_ = package_name;
    method_for_testing_ = method;
    response_for_testing_ = response;
  }

 private:
  std::string package_name_for_testing_;
  std::string method_for_testing_;
  std::string response_for_testing_;
};

}  // namespace

// Declared in cross-platform header file. See:
// //components/payments/content/android_app_communication.h
// static
std::unique_ptr<AndroidAppCommunication> AndroidAppCommunication::Create(
    content::BrowserContext* context) {
  return std::make_unique<AndroidAppCommunicationLacros>(context);
}

}  // namespace payments
