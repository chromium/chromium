// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android_app_communication_test_support_chrome_os.h"

#include <utility>

#include "ash/components/arc/pay/arc_payment_app_bridge.h"
#include "ash/components/arc/test/arc_payment_app_bridge_test_support.h"
#include "ash/public/cpp/external_arc/overlay/test/test_arc_overlay_manager.h"
#include "chromeos/components/payments/mojom/payment_app.mojom.h"
#include "chromeos/components/payments/mojom/payment_app_types.mojom.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/native_error_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class ScopedInitializationAsh
    : public AndroidAppCommunicationTestSupport::ScopedInitialization {
 public:
  ScopedInitializationAsh(
      arc::ArcServiceManager* manager,
      chromeos::payments::mojom::PaymentAppInstance* instance)
      : scoped_set_instance_(manager, instance) {}
  ~ScopedInitializationAsh() override = default;

  ScopedInitializationAsh(const ScopedInitializationAsh& other) = delete;
  ScopedInitializationAsh& operator=(const ScopedInitializationAsh& other) =
      delete;

 private:
  arc::ArcPaymentAppBridgeTestSupport::ScopedSetInstance scoped_set_instance_;
};

class AndroidAppCommunicationTestSupportAsh
    : public AndroidAppCommunicationTestSupportChromeOS {
 public:
  AndroidAppCommunicationTestSupportAsh() = default;
  ~AndroidAppCommunicationTestSupportAsh() override = default;

  AndroidAppCommunicationTestSupportAsh(
      const AndroidAppCommunicationTestSupportAsh& other) = delete;
  AndroidAppCommunicationTestSupportAsh& operator=(
      const AndroidAppCommunicationTestSupportAsh& other) = delete;

  std::unique_ptr<ScopedInitialization> CreateScopedInitialization() override {
    return std::make_unique<ScopedInitializationAsh>(support_.manager(),
                                                     support_.instance());
  }

  content::BrowserContext* context() override { return support_.context(); }

  std::string GetNoInstanceExpectedErrorString() override {
    return errors::kUnableToInvokeAndroidPaymentApps;
  }

  MockPaymentAppInstance* instance() override { return support_.instance(); }

 private:
  arc::ArcPaymentAppBridgeTestSupport support_;
  ash::TestArcOverlayManager overlay_manager_;
};

}  // namespace

// Declared in cross-platform file
// //components/payments/content/android_app_communication_test_support.h
// static
std::unique_ptr<AndroidAppCommunicationTestSupport>
AndroidAppCommunicationTestSupport::Create() {
  return std::make_unique<AndroidAppCommunicationTestSupportAsh>();
}

}  // namespace payments
