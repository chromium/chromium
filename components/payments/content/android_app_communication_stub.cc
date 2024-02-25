// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android_app_communication.h"

#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "components/payments/core/native_error_strings.h"

namespace payments {
namespace {

class AndroidAppCommunicationStub : public AndroidAppCommunication {
 public:
  explicit AndroidAppCommunicationStub(content::BrowserContext* context)
      : AndroidAppCommunication(context) {}

  ~AndroidAppCommunicationStub() override = default;

  // AndroidAppCommunication implementation.
  void GetAppDescriptions(const std::string& twa_package_name,
                          GetAppDescriptionsCallback callback) override {
    std::move(callback).Run(/*error_message=*/std::nullopt,
                            /*app_descriptions=*/{});
  }

  // AndroidAppCommunication implementation.
  void IsReadyToPay(const std::string& package_name,
                    const std::string& service_name,
                    const std::map<std::string, std::set<std::string>>&
                        stringified_method_data,
                    const GURL& top_level_origin,
                    const GURL& payment_request_origin,
                    const std::string& payment_request_id,
                    IsReadyToPayCallback callback) override {
    std::move(callback).Run(errors::kUnableToInvokeAndroidPaymentApps,
                            /*is_ready_to_pay=*/false);
  }

  // AndroidAppCommunication implementation.
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
    std::move(callback).Run(errors::kUnableToInvokeAndroidPaymentApps,
                            /*is_activity_result_ok=*/false,
                            /*payment_method_identifier=*/"",
                            /*stringified_details=*/"{}");
  }

  void AbortPaymentApp(const base::UnguessableToken& request_token,
                       AbortPaymentAppCallback callback) override {
    std::move(callback).Run(false);
  }

  // AndroidAppCommunication implementation.
  void SetForTesting() override {}

  // AndroidAppCommunication implementation.
  void SetAppForTesting(const std::string& package_name,
                        const std::string& method,
                        const std::string& response) override {}
};

}  // namespace

// Declared in cross-platform header file. See:
// components/payments/content/android_app_communication.h
// static
std::unique_ptr<AndroidAppCommunication> AndroidAppCommunication::Create(
    content::BrowserContext* context) {
  return std::make_unique<AndroidAppCommunicationStub>(context);
}

}  // namespace payments
