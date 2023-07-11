// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android_app_communication_test_support.h"

#include <utility>

#include "components/payments/core/native_error_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"

namespace payments {
namespace {

class AndroidAppCommunicationTestSupportStub
    : public AndroidAppCommunicationTestSupport {
 public:
  AndroidAppCommunicationTestSupportStub() = default;
  ~AndroidAppCommunicationTestSupportStub() override = default;

  AndroidAppCommunicationTestSupportStub(
      const AndroidAppCommunicationTestSupportStub& other) = delete;
  AndroidAppCommunicationTestSupportStub& operator=(
      const AndroidAppCommunicationTestSupportStub& other) = delete;

  bool AreAndroidAppsSupportedOnThisPlatform() const override { return false; }

  std::unique_ptr<ScopedInitialization> CreateScopedInitialization() override {
    return std::make_unique<ScopedInitialization>();
  }

  void ExpectNoListOfPaymentAppsQuery() override {}

  void ExpectNoIsReadyToPayQuery() override {}

  void ExpectNoPaymentAppInvoke() override {}

  void ExpectQueryListOfPaymentAppsAndRespond(
      std::vector<std::unique_ptr<AndroidAppDescription>> apps) override {}

  void ExpectQueryIsReadyToPayAndRespond(bool is_ready_to_pay) override {}

  void ExpectInvokePaymentAppAndRespond(
      bool is_activity_result_ok,
      const std::string& payment_method_identifier,
      const std::string& stringified_details) override {}

  void ExpectInvokeAndAbortPaymentApp() override {}

  void ExpectNoAbortPaymentApp() override {}

  content::BrowserContext* context() override { return &context_; }

  std::string GetNoInstanceExpectedErrorString() override {
    return errors::kUnableToInvokeAndroidPaymentApps;
  }

 private:
  content::BrowserTaskEnvironment environment_;
  content::TestBrowserContext context_;
};

}  // namespace

// Declared in cross-platform file
// //components/payments/content/android_app_communication_test_support.h
// static
std::unique_ptr<AndroidAppCommunicationTestSupport>
AndroidAppCommunicationTestSupport::Create() {
  return std::make_unique<AndroidAppCommunicationTestSupportStub>();
}

}  // namespace payments
