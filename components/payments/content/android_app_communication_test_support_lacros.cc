// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/payments/content/android_app_communication_test_support_chrome_os.h"

#include "base/test/scoped_feature_list.h"
#include "chromeos/components/payments/mock_payment_app_instance.h"
#include "chromeos/components/payments/mojom/payment_app.mojom.h"
#include "chromeos/components/payments/mojom/payment_app_types.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "components/payments/content/android_app_communication.h"
#include "components/payments/core/chrome_os_error_strings.h"
#include "components/payments/core/features.h"
#include "components/payments/core/method_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class ScopedInitializationLacros
    : public AndroidAppCommunicationTestSupport::ScopedInitialization {
 public:
  ScopedInitializationLacros(MockPaymentAppInstance* instance) {
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        instance->receiver_.BindNewPipeAndPassRemote());
  }
  ~ScopedInitializationLacros() override = default;

  ScopedInitializationLacros(const ScopedInitializationLacros& other) = delete;
  ScopedInitializationLacros& operator=(
      const ScopedInitializationLacros& other) = delete;

 private:
  chromeos::ScopedLacrosServiceTestHelper scoped_lacros_service_test_helper_;
};

class AndroidAppCommunicationTestSupportLacros
    : public AndroidAppCommunicationTestSupportChromeOS {
 public:
  AndroidAppCommunicationTestSupportLacros() {
    feature_list_.InitAndEnableFeature(features::kAppStoreBilling);
  }
  ~AndroidAppCommunicationTestSupportLacros() override = default;

  AndroidAppCommunicationTestSupportLacros(
      const AndroidAppCommunicationTestSupportLacros& other) = delete;
  AndroidAppCommunicationTestSupportLacros& operator=(
      const AndroidAppCommunicationTestSupportLacros& other) = delete;

  std::unique_ptr<ScopedInitialization> CreateScopedInitialization() override {
    return std::make_unique<ScopedInitializationLacros>(instance());
  }

  content::BrowserContext* context() override { return &context_; }

  std::string GetNoInstanceExpectedErrorString() override {
    return errors::kUnableToConnectToAsh;
  }

  // The mock payment_app.mojom connection.
  MockPaymentAppInstance* instance() override { return &instance_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  MockPaymentAppInstance instance_;
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

// Declared in cross-platform file
// //components/payments/content/android_app_communication_test_support.h
// static
std::unique_ptr<AndroidAppCommunicationTestSupport>
AndroidAppCommunicationTestSupport::Create() {
  return std::make_unique<AndroidAppCommunicationTestSupportLacros>();
}

}  // namespace payments
