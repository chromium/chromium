// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_APP_COMMUNICATION_TEST_SUPPORT_CHROME_OS_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_APP_COMMUNICATION_TEST_SUPPORT_CHROME_OS_H_

#include "chromeos/components/payments/mock_payment_app_instance.h"
#include "components/payments/content/android_app_communication_test_support.h"

namespace payments {
class AndroidAppCommunicationTestSupportChromeOS
    : public AndroidAppCommunicationTestSupport {
 public:
  using IsPaymentImplementedCallback = base::OnceCallback<void(
      chromeos::payments::mojom::IsPaymentImplementedResultPtr)>;
  using IsReadyToPayCallback = base::OnceCallback<void(
      chromeos::payments::mojom::IsReadyToPayResultPtr)>;
  using InvokePaymentAppCallback = base::OnceCallback<void(
      chromeos::payments::mojom::InvokePaymentAppResultPtr)>;
  using AbortPaymentAppCallback = base::OnceCallback<void(bool)>;

  AndroidAppCommunicationTestSupportChromeOS();
  ~AndroidAppCommunicationTestSupportChromeOS() override;

  AndroidAppCommunicationTestSupportChromeOS(
      const AndroidAppCommunicationTestSupportChromeOS& other) = delete;
  AndroidAppCommunicationTestSupportChromeOS& operator=(
      const AndroidAppCommunicationTestSupportChromeOS& other) = delete;

  bool AreAndroidAppsSupportedOnThisPlatform() const override;

  void ExpectNoListOfPaymentAppsQuery() override;

  void ExpectNoIsReadyToPayQuery() override;

  void ExpectNoPaymentAppInvoke() override;

  void ExpectQueryListOfPaymentAppsAndRespond(
      std::vector<std::unique_ptr<AndroidAppDescription>> apps) override;

  void ExpectQueryIsReadyToPayAndRespond(bool is_ready_to_pay) override;

  void ExpectInvokePaymentAppAndRespond(
      bool is_activity_result_ok,
      const std::string& payment_method_identifier,
      const std::string& stringified_details) override;

  void ExpectInvokeAndAbortPaymentApp() override;

  void ExpectNoAbortPaymentApp() override;

 private:
  void RespondToGetAppDescriptions(const std::string& package_name,
                                   IsPaymentImplementedCallback callback);

  virtual MockPaymentAppInstance* instance();

  std::vector<std::unique_ptr<AndroidAppDescription>> apps_;
  InvokePaymentAppCallback pending_invoke_callback_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_APP_COMMUNICATION_TEST_SUPPORT_CHROME_OS_H_
