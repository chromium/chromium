// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android_app_communication_test_support_chrome_os.h"

#include <utility>

#include "chromeos/components/payments/mojom/payment_app.mojom.h"
#include "chromeos/components/payments/mojom/payment_app_types.mojom.h"
#include "components/payments/core/method_strings.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace payments {

AndroidAppCommunicationTestSupportChromeOS::
    AndroidAppCommunicationTestSupportChromeOS() = default;
AndroidAppCommunicationTestSupportChromeOS::
    ~AndroidAppCommunicationTestSupportChromeOS() = default;

bool AndroidAppCommunicationTestSupportChromeOS::
    AreAndroidAppsSupportedOnThisPlatform() const {
  return true;
}

void AndroidAppCommunicationTestSupportChromeOS::
    ExpectNoListOfPaymentAppsQuery() {
  EXPECT_CALL(*instance(), IsPaymentImplemented(testing::_, testing::_))
      .Times(0);
}

void AndroidAppCommunicationTestSupportChromeOS::ExpectNoIsReadyToPayQuery() {
  EXPECT_CALL(*instance(), IsReadyToPay(testing::_, testing::_)).Times(0);
}

void AndroidAppCommunicationTestSupportChromeOS::ExpectNoPaymentAppInvoke() {
  EXPECT_CALL(*instance(), InvokePaymentApp(testing::_, testing::_)).Times(0);
}

void AndroidAppCommunicationTestSupportChromeOS::
    ExpectQueryListOfPaymentAppsAndRespond(
        std::vector<std::unique_ptr<AndroidAppDescription>> apps) {
  // Move |apps| into a member variable, so it's still alive by the time the
  // RespondToGetAppDescriptions() method is executed at some point in the
  // future.
  apps_ = std::move(apps);
  EXPECT_CALL(*instance(), IsPaymentImplemented(testing::_, testing::_))
      .WillOnce([&](const std::string& package_name,
                    IsPaymentImplementedCallback callback) {
        RespondToGetAppDescriptions(package_name, std::move(callback));
      });
}

void AndroidAppCommunicationTestSupportChromeOS::
    ExpectQueryIsReadyToPayAndRespond(bool is_ready_to_pay) {
  EXPECT_CALL(*instance(), IsReadyToPay(testing::_, testing::_))
      .WillOnce([is_ready_to_pay](
                    chromeos::payments::mojom::PaymentParametersPtr parameters,
                    IsReadyToPayCallback callback) {
        std::move(callback).Run(
            chromeos::payments::mojom::IsReadyToPayResult::NewResponse(
                is_ready_to_pay));
      });
}

void AndroidAppCommunicationTestSupportChromeOS::
    ExpectInvokePaymentAppAndRespond(
        bool is_activity_result_ok,
        const std::string& payment_method_identifier,
        const std::string& stringified_details) {
  EXPECT_CALL(*instance(), InvokePaymentApp(testing::_, testing::_))
      .WillOnce([is_activity_result_ok, stringified_details](
                    chromeos::payments::mojom::PaymentParametersPtr parameters,
                    InvokePaymentAppCallback callback) {
        // Chrome OS supports only kGooglePlayBilling payment method
        // identifier at this time, so the |payment_method_identifier|
        // parameter is ignored here.
        auto valid =
            chromeos::payments::mojom::InvokePaymentAppValidResult::New();
        valid->is_activity_result_ok = is_activity_result_ok;
        valid->stringified_details = stringified_details;
        std::move(callback).Run(
            chromeos::payments::mojom::InvokePaymentAppResult::NewValid(
                std::move(valid)));
      });
}

void AndroidAppCommunicationTestSupportChromeOS::
    ExpectInvokeAndAbortPaymentApp() {
  EXPECT_CALL(*instance(), InvokePaymentApp(testing::_, testing::_))
      .WillOnce(
          [this](chromeos::payments::mojom::PaymentParametersPtr parameters,
                 InvokePaymentAppCallback callback) {
            pending_invoke_callback_ = std::move(callback);
          });

  EXPECT_CALL(*instance(), AbortPaymentApp(testing::_, testing::_))
      .WillOnce([this](const std::string& request_token,
                       AbortPaymentAppCallback callback) {
        if (!pending_invoke_callback_.is_null()) {
          std::move(pending_invoke_callback_)
              .Run(chromeos::payments::mojom::InvokePaymentAppResult::NewError(
                  "Payment was aborted."));
        }
        std::move(callback).Run(true);
      });
}

void AndroidAppCommunicationTestSupportChromeOS::ExpectNoAbortPaymentApp() {
  EXPECT_CALL(*instance(), AbortPaymentApp(testing::_, testing::_)).Times(0);
}

void AndroidAppCommunicationTestSupportChromeOS::RespondToGetAppDescriptions(
    const std::string& package_name,
    IsPaymentImplementedCallback callback) {
  auto valid =
      chromeos::payments::mojom::IsPaymentImplementedValidResult::New();
  for (const auto& app : apps_) {
    if (app->package == package_name) {
      for (const auto& activity : app->activities) {
        // Chrome OS supports only kGooglePlayBilling method at this time.
        if (activity->default_payment_method == methods::kGooglePlayBilling) {
          valid->activity_names.push_back(activity->name);
        }
      }

      valid->service_names = app->service_names;

      // Chrome OS supports only one payment app in Android subsystem at this
      // time, i.e., the TWA that invoked Chrome.
      break;
    }
  }

  std::move(callback).Run(
      chromeos::payments::mojom::IsPaymentImplementedResult::NewValid(
          std::move(valid)));
}

MockPaymentAppInstance* AndroidAppCommunicationTestSupportChromeOS::instance() {
  return nullptr;
}

}  // namespace payments
