// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android_payment_app_factory.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "components/payments/content/android_app_communication.h"
#include "components/payments/content/android_app_communication_test_support.h"
#include "components/payments/content/mock_android_app_communication.h"
#include "components/payments/content/mock_payment_app_factory_delegate.h"
#include "components/payments/content/payment_app_factory.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/core/android_app_description.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace payments {
namespace {

using base::test::RunOnceCallback;

// The scaffolding for testing the Android payment app factory.
class AndroidPaymentAppFactoryTest : public testing::Test {
 public:
  AndroidPaymentAppFactoryTest()
      : mock_communication_(
            std::make_unique<MockAndroidAppCommunication>(&context_)),
        factory_(mock_communication_->GetWeakPtr()) {
    auto method_data = mojom::PaymentMethodData::New();
    method_data->supported_method = "https://play.google.com/billing";
    method_data->stringified_data = "{}";
    delegate_ = std::make_unique<MockPaymentAppFactoryDelegate>(
        web_contents_factory_.CreateWebContents(&context_),
        std::move(method_data));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  content::TestWebContentsFactory web_contents_factory_;
  std::unique_ptr<MockPaymentAppFactoryDelegate> delegate_;
  std::unique_ptr<MockAndroidAppCommunication> mock_communication_;
  AndroidPaymentAppFactory factory_;
};

// This is a regression test for crbug.com/1414738. A mismatch in early-return
// checks could result in calling IsReadyToPay with a null RenderFrameHost in a
// loop - however the first call would end up deleting |this| and cause a UAF.
TEST_F(AndroidPaymentAppFactoryTest, NullRenderFrameHost) {
  EXPECT_CALL(*delegate_, GetTwaPackageName)
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>("com.example.app"));

  // In order to reach the problematic code, we need a null RenderFrameHost and
  // also to be in off the record mode.
  EXPECT_CALL(*delegate_, GetInitiatorRenderFrameHost)
      .WillRepeatedly(testing::Return(nullptr));
  delegate_->set_is_off_the_record();

  // Return an app with multiple activities. This causes multiple iterations
  // when looping over `single_activity_apps` - the UAF would trigger on the
  // second iteration.
  EXPECT_CALL(*mock_communication_, GetAppDescriptions)
      .WillRepeatedly(
          [](const std::string& twa_package_name,
             MockAndroidAppCommunication::GetAppDescriptionsCallback callback) {
            std::vector<std::unique_ptr<AndroidAppDescription>> apps;
            apps.emplace_back(std::make_unique<AndroidAppDescription>());
            apps.back()->package = "com.example.app";
            apps.back()->service_names.push_back("com.example.app.Service");
            apps.back()->activities.emplace_back(
                std::make_unique<AndroidActivityDescription>());
            apps.back()->activities.back()->name = "com.example.app.Activity";
            apps.back()->activities.back()->default_payment_method =
                "https://play.google.com/billing";
            apps.back()->activities.emplace_back(
                std::make_unique<AndroidActivityDescription>());
            apps.back()->activities.back()->name = "com.example.app.Activity2";
            apps.back()->activities.back()->default_payment_method =
                "https://play.google.com/billing";
            std::move(callback).Run(std::nullopt, std::move(apps));
          });

  // Now trigger app finding. This should near immediately bail in
  // AppFinder::OnGetAppDescriptions and not make it to IsReadyToPay.
  factory_.Create(delegate_->GetWeakPtr());
}

// This test class uses a deeper integration into the underlying app support,
// and as such some tests may only run on certain platforms (e.g., ChromeOS).
class AndroidPaymentAppFactoryIntegrationTest : public testing::Test {
 public:
  AndroidPaymentAppFactoryIntegrationTest()
      : support_(AndroidAppCommunicationTestSupport::Create()),
        factory_(GetCommunication(support_->context())) {
    auto method_data = mojom::PaymentMethodData::New();
    method_data->supported_method = "https://play.google.com/billing";
    method_data->stringified_data = "{}";
    delegate_ = std::make_unique<MockPaymentAppFactoryDelegate>(
        web_contents_factory_.CreateWebContents(support_->context()),
        std::move(method_data));

    EXPECT_CALL(*delegate_, GetInitiatorRenderFrameHost())
        .WillRepeatedly(testing::Return(
            delegate_->GetWebContents()->GetPrimaryMainFrame()));
  }

  std::unique_ptr<AndroidAppCommunicationTestSupport> support_;
  content::TestWebContentsFactory web_contents_factory_;
  std::unique_ptr<MockPaymentAppFactoryDelegate> delegate_;
  AndroidPaymentAppFactory factory_;

 private:
  // Returns the Android app communication that can be used in unit tests.
  static base::WeakPtr<AndroidAppCommunication> GetCommunication(
      content::BrowserContext* context) {
    base::WeakPtr<AndroidAppCommunication> communication =
        AndroidAppCommunication::GetForBrowserContext(context);
    communication->SetForTesting();
    return communication;
  }
};

// The payment app factory should return an error if it's unable to invoke
// Android payment apps on a platform that supports such apps, e.g, when ARC is
// disabled on Chrome OS, Lacros cannot connect to payment app instance.
TEST_F(AndroidPaymentAppFactoryIntegrationTest,
       FactoryReturnsErrorWithoutPaymentAppInstance) {
  EXPECT_CALL(*delegate_, GetTwaPackageName)
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>("com.example.app"));
  base::RunLoop runloop;
  EXPECT_CALL(*delegate_, OnDoneCreatingPaymentApps()).WillOnce([&runloop] {
    runloop.Quit();
  });

  EXPECT_CALL(*delegate_, OnPaymentAppCreationError(
                              support_->GetNoInstanceExpectedErrorString(),
                              AppCreationFailureReason::UNKNOWN))
      .Times(support_->AreAndroidAppsSupportedOnThisPlatform() ? 1 : 0);
  EXPECT_CALL(*delegate_, OnPaymentAppCreated(testing::_)).Times(0);
  EXPECT_CALL(*delegate_, GetChromeOSTWAInstanceId()).Times(0);

  support_->ExpectNoListOfPaymentAppsQuery();
  support_->ExpectNoIsReadyToPayQuery();

  factory_.Create(delegate_->GetWeakPtr());
  runloop.Run();
}

// The payment app factory should not return any errors when there're no Android
// payment apps available.
TEST_F(AndroidPaymentAppFactoryIntegrationTest, NoErrorsWhenNoApps) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(*delegate_, GetTwaPackageName)
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>("com.example.app"));
  base::RunLoop runloop;
  EXPECT_CALL(*delegate_, OnDoneCreatingPaymentApps()).WillOnce([&runloop] {
    runloop.Quit();
  });

  EXPECT_CALL(*delegate_, OnPaymentAppCreationError(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*delegate_, OnPaymentAppCreated(testing::_)).Times(0);
  EXPECT_CALL(*delegate_, GetChromeOSTWAInstanceId()).Times(0);

  support_->ExpectQueryListOfPaymentAppsAndRespond({});
  support_->ExpectNoIsReadyToPayQuery();

  factory_.Create(delegate_->GetWeakPtr());
  runloop.Run();
}

// The |arg| is of type std::unique_ptr<PaymentApp>.
MATCHER_P3(PaymentAppMatches, type, package, method, "") {
  return arg->type() == type && arg->GetId() == package &&
         base::Contains(arg->GetAppMethodNames(), method);
}

// The payment app factory should return the TWA payment app when running in TWA
// mode, even when it does not have an IS_READY_TO_PAY service.
TEST_F(AndroidPaymentAppFactoryIntegrationTest,
       FindAppsThatDoNotHaveReadyToPayService) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(*delegate_, GetTwaPackageName)
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>("com.example.app"));
  base::RunLoop runloop;
  EXPECT_CALL(*delegate_, OnDoneCreatingPaymentApps()).WillOnce([&runloop] {
    runloop.Quit();
  });

  EXPECT_CALL(*delegate_, OnPaymentAppCreationError(testing::_, testing::_))
      .Times(0);

  EXPECT_CALL(*delegate_,
              OnPaymentAppCreated(PaymentAppMatches(
                  PaymentApp::Type::NATIVE_MOBILE_APP, "com.example.app",
                  "https://play.google.com/billing")))
      .Times(support_->AreAndroidAppsSupportedOnThisPlatform() ? 1 : 0);
  EXPECT_CALL(*delegate_, GetChromeOSTWAInstanceId())
      .Times(support_->AreAndroidAppsSupportedOnThisPlatform() ? 1 : 0);

  // This app does not have an IS_READY_TO_PAY service.
  std::vector<std::unique_ptr<AndroidAppDescription>> apps;
  apps.emplace_back(std::make_unique<AndroidAppDescription>());
  apps.back()->package = "com.example.app";
  apps.back()->activities.emplace_back(
      std::make_unique<AndroidActivityDescription>());
  apps.back()->activities.back()->name = "com.example.app.Activity";
  apps.back()->activities.back()->default_payment_method =
      "https://play.google.com/billing";
  support_->ExpectQueryListOfPaymentAppsAndRespond(std::move(apps));

  // There is no IS_READY_TO_PAY service to invoke.
  support_->ExpectNoIsReadyToPayQuery();

  factory_.Create(delegate_->GetWeakPtr());
  runloop.Run();
}

// The payment app factory should return one payment app and should not query
// the IS_READY_TO_PAY service, because of being off the record.
TEST_F(AndroidPaymentAppFactoryIntegrationTest,
       DoNotQueryReadyToPaySericeWhenOffTheRecord) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  // Simulate being off the record.
  delegate_->set_is_off_the_record();

  EXPECT_CALL(*delegate_, GetTwaPackageName)
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>("com.example.app"));
  base::RunLoop runloop;
  EXPECT_CALL(*delegate_, OnDoneCreatingPaymentApps()).WillOnce([&runloop] {
    runloop.Quit();
  });

  EXPECT_CALL(*delegate_, OnPaymentAppCreationError(testing::_, testing::_))
      .Times(0);

  EXPECT_CALL(*delegate_,
              OnPaymentAppCreated(PaymentAppMatches(
                  PaymentApp::Type::NATIVE_MOBILE_APP, "com.example.app",
                  "https://play.google.com/billing")))
      .Times(support_->AreAndroidAppsSupportedOnThisPlatform() ? 1 : 0);

  EXPECT_CALL(*delegate_, GetChromeOSTWAInstanceId())
      .Times(support_->AreAndroidAppsSupportedOnThisPlatform() ? 1 : 0);

  std::vector<std::unique_ptr<AndroidAppDescription>> apps;
  apps.emplace_back(std::make_unique<AndroidAppDescription>());
  apps.back()->package = "com.example.app";
  apps.back()->service_names.push_back("com.example.app.Service");
  apps.back()->activities.emplace_back(
      std::make_unique<AndroidActivityDescription>());
  apps.back()->activities.back()->name = "com.example.app.Activity";
  apps.back()->activities.back()->default_payment_method =
      "https://play.google.com/billing";
  support_->ExpectQueryListOfPaymentAppsAndRespond(std::move(apps));

  // The IS_READY_TO_PAY service should not be invoked when off the record.
  support_->ExpectNoIsReadyToPayQuery();

  factory_.Create(delegate_->GetWeakPtr());
  runloop.Run();
}

// The payment app factory should return the TWA payment app that returns true
// from IS_READY_TO_PAY service when running in TWA mode.
TEST_F(AndroidPaymentAppFactoryIntegrationTest,
       FindTheTwaPaymentAppThatIsReadyToPayInTwaMode) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(*delegate_, GetTwaPackageName)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>("com.twa.app"));
  base::RunLoop runloop;
  EXPECT_CALL(*delegate_, OnDoneCreatingPaymentApps()).WillOnce([&runloop] {
    runloop.Quit();
  });

  EXPECT_CALL(*delegate_, OnPaymentAppCreationError(testing::_, testing::_))
      .Times(0);

  EXPECT_CALL(*delegate_,
              OnPaymentAppCreated(PaymentAppMatches(
                  PaymentApp::Type::NATIVE_MOBILE_APP, "com.twa.app",
                  "https://play.google.com/billing")))
      .Times(support_->AreAndroidAppsSupportedOnThisPlatform() ? 1 : 0);
  EXPECT_CALL(*delegate_, GetChromeOSTWAInstanceId())
      .Times(support_->AreAndroidAppsSupportedOnThisPlatform() ? 1 : 0);

  std::vector<std::unique_ptr<AndroidAppDescription>> apps;
  apps.emplace_back(std::make_unique<AndroidAppDescription>());
  apps.back()->package = "com.twa.app";
  apps.back()->service_names.push_back("com.twa.app.Service");
  apps.back()->activities.emplace_back(
      std::make_unique<AndroidActivityDescription>());
  apps.back()->activities.back()->name = "com.twa.app.Activity";
  apps.back()->activities.back()->default_payment_method =
      "https://play.google.com/billing";
  support_->ExpectQueryListOfPaymentAppsAndRespond(std::move(apps));
  support_->ExpectQueryIsReadyToPayAndRespond(/*is_ready_to_pay=*/true);

  factory_.Create(delegate_->GetWeakPtr());
  runloop.Run();
}

// The payment app factory should return no payment apps when IS_READY_TO_PAY
// service returns false.
TEST_F(AndroidPaymentAppFactoryIntegrationTest,
       IgnoreAppsThatAreNotReadyToPay) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(*delegate_, GetTwaPackageName)
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>("com.example.app"));
  base::RunLoop runloop;
  EXPECT_CALL(*delegate_, OnDoneCreatingPaymentApps()).WillOnce([&runloop] {
    runloop.Quit();
  });
  EXPECT_CALL(*delegate_, OnPaymentAppCreationError(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*delegate_, OnPaymentAppCreated(testing::_)).Times(0);
  EXPECT_CALL(*delegate_, GetChromeOSTWAInstanceId()).Times(0);

  std::vector<std::unique_ptr<AndroidAppDescription>> apps;
  apps.emplace_back(std::make_unique<AndroidAppDescription>());
  apps.back()->package = "com.example.app";
  apps.back()->service_names.push_back("com.example.app.Service");
  apps.back()->activities.emplace_back(
      std::make_unique<AndroidActivityDescription>());
  apps.back()->activities.back()->name = "com.example.app.Activity";
  apps.back()->activities.back()->default_payment_method =
      "https://play.google.com/billing";
  support_->ExpectQueryListOfPaymentAppsAndRespond(std::move(apps));
  support_->ExpectQueryIsReadyToPayAndRespond(/*is_ready_to_pay=*/false);

  factory_.Create(delegate_->GetWeakPtr());
  runloop.Run();
}

// The payment app factory should return the correct TWA payment app out of two
// installed payment apps, when running in TWA mode.
TEST_F(AndroidPaymentAppFactoryIntegrationTest, FindTheCorrectTwaAppInTwaMode) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(*delegate_, GetTwaPackageName)
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>("com.correct-twa.app"));
  base::RunLoop runloop;
  EXPECT_CALL(*delegate_, OnDoneCreatingPaymentApps()).WillOnce([&runloop] {
    runloop.Quit();
  });

  EXPECT_CALL(*delegate_, OnPaymentAppCreationError(testing::_, testing::_))
      .Times(0);

  EXPECT_CALL(*delegate_,
              OnPaymentAppCreated(PaymentAppMatches(
                  PaymentApp::Type::NATIVE_MOBILE_APP, "com.correct-twa.app",
                  "https://play.google.com/billing")))
      .Times(support_->AreAndroidAppsSupportedOnThisPlatform() ? 1 : 0);
  EXPECT_CALL(*delegate_,
              OnPaymentAppCreated(PaymentAppMatches(
                  PaymentApp::Type::NATIVE_MOBILE_APP, "com.different.app",
                  "https://play.google.com/billing")))
      .Times(0);
  EXPECT_CALL(*delegate_, GetChromeOSTWAInstanceId())
      .Times(support_->AreAndroidAppsSupportedOnThisPlatform() ? 1 : 0);

  std::vector<std::unique_ptr<AndroidAppDescription>> apps;
  apps.emplace_back(std::make_unique<AndroidAppDescription>());
  apps.back()->package = "com.correct-twa.app";
  apps.back()->service_names.push_back("com.correct-twa.app.Service");
  apps.back()->activities.emplace_back(
      std::make_unique<AndroidActivityDescription>());
  apps.back()->activities.back()->name = "com.correct-twa.app.Activity";
  apps.back()->activities.back()->default_payment_method =
      "https://play.google.com/billing";

  apps.emplace_back(std::make_unique<AndroidAppDescription>());
  apps.back()->package = "com.different.app";
  apps.back()->service_names.push_back("com.different.app.Service");
  apps.back()->activities.emplace_back(
      std::make_unique<AndroidActivityDescription>());
  apps.back()->activities.back()->name = "com.different.app.Activity";
  apps.back()->activities.back()->default_payment_method =
      "https://play.google.com/billing";

  support_->ExpectQueryListOfPaymentAppsAndRespond(std::move(apps));
  support_->ExpectQueryIsReadyToPayAndRespond(/*is_ready_to_pay=*/true);

  factory_.Create(delegate_->GetWeakPtr());
  runloop.Run();
}

// The payment app factory does not return non-TWA payment apps when running in
// TWA mode.
TEST_F(AndroidPaymentAppFactoryIntegrationTest, IgnoreNonTwaAppsInTwaMode) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(*delegate_, GetTwaPackageName)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>("com.twa.app"));
  base::RunLoop runloop;
  EXPECT_CALL(*delegate_, OnDoneCreatingPaymentApps()).WillOnce([&runloop] {
    runloop.Quit();
  });
  EXPECT_CALL(*delegate_, OnPaymentAppCreationError(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*delegate_, OnPaymentAppCreated(testing::_)).Times(0);
  EXPECT_CALL(*delegate_, GetChromeOSTWAInstanceId()).Times(0);

  std::vector<std::unique_ptr<AndroidAppDescription>> apps;
  apps.emplace_back(std::make_unique<AndroidAppDescription>());
  apps.back()->package = "com.non-twa.app";
  apps.back()->service_names.push_back("com.non-twa.app.Service");
  apps.back()->activities.emplace_back(
      std::make_unique<AndroidActivityDescription>());
  apps.back()->activities.back()->name = "com.non-twa.app.Activity";
  apps.back()->activities.back()->default_payment_method =
      "https://play.google.com/billing";
  support_->ExpectQueryListOfPaymentAppsAndRespond(std::move(apps));
  support_->ExpectNoIsReadyToPayQuery();

  factory_.Create(delegate_->GetWeakPtr());
  runloop.Run();
}

// The payment app factory does not return any payment apps when not running
// inside of TWA.
TEST_F(AndroidPaymentAppFactoryIntegrationTest,
       DoNotLookForAppsWhenOutsideOfTwaMode) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(*delegate_, GetTwaPackageName)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(""));
  base::RunLoop runloop;
  EXPECT_CALL(*delegate_, OnDoneCreatingPaymentApps()).WillOnce([&runloop] {
    runloop.Quit();
  });
  EXPECT_CALL(*delegate_, OnPaymentAppCreationError(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*delegate_, OnPaymentAppCreated(testing::_)).Times(0);
  EXPECT_CALL(*delegate_, GetChromeOSTWAInstanceId()).Times(0);

  support_->ExpectNoListOfPaymentAppsQuery();

  factory_.Create(delegate_->GetWeakPtr());
  runloop.Run();
}

// The Android payment app factory works only with TWA specific payment methods.
TEST_F(AndroidPaymentAppFactoryIntegrationTest,
       DoNotLookForAppsForNonTwaMethod) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  // "https://example.test" is not a TWA specific payment method.
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "https://example.test";
  method_data->stringified_data = "{}";
  delegate_->SetRequestedPaymentMethod(std::move(method_data));

  EXPECT_CALL(*delegate_, GetTwaPackageName)
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>("com.example.app"));
  base::RunLoop runloop;
  EXPECT_CALL(*delegate_, OnDoneCreatingPaymentApps()).WillOnce([&runloop] {
    runloop.Quit();
  });
  EXPECT_CALL(*delegate_, OnPaymentAppCreationError(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*delegate_, OnPaymentAppCreated(testing::_)).Times(0);
  EXPECT_CALL(*delegate_, GetChromeOSTWAInstanceId()).Times(0);

  support_->ExpectNoListOfPaymentAppsQuery();
  support_->ExpectNoIsReadyToPayQuery();

  factory_.Create(delegate_->GetWeakPtr());
  runloop.Run();
}

// If the TWA supports a non-TWA-specific payment method, then it should be
// ignored.
TEST_F(AndroidPaymentAppFactoryIntegrationTest, IgnoreNonTwaMethodInTheTwa) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(*delegate_, GetTwaPackageName)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>("com.twa.app"));
  base::RunLoop runloop;
  EXPECT_CALL(*delegate_, OnDoneCreatingPaymentApps()).WillOnce([&runloop] {
    runloop.Quit();
  });
  EXPECT_CALL(*delegate_, OnPaymentAppCreationError(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*delegate_, OnPaymentAppCreated(testing::_)).Times(0);
  EXPECT_CALL(*delegate_, GetChromeOSTWAInstanceId()).Times(0);

  std::vector<std::unique_ptr<AndroidAppDescription>> apps;
  apps.emplace_back(std::make_unique<AndroidAppDescription>());
  apps.back()->package = "com.twa.app";
  apps.back()->service_names.push_back("com.twa.app.Service");
  apps.back()->activities.emplace_back(
      std::make_unique<AndroidActivityDescription>());
  apps.back()->activities.back()->name = "com.twa.app.Activity";
  apps.back()->activities.back()->default_payment_method =
      "https://example.test";
  support_->ExpectQueryListOfPaymentAppsAndRespond(std::move(apps));
  support_->ExpectNoIsReadyToPayQuery();

  factory_.Create(delegate_->GetWeakPtr());
  runloop.Run();
}

// If the TWA supports both a TWA-specific and a non-TWA-specific payment
// method, then only the TWA-specific payment method activity should be
// returned.
TEST_F(AndroidPaymentAppFactoryIntegrationTest,
       FindOnlyActivitiesWithTwaSpecificMethodName) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(*delegate_, GetTwaPackageName)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>("com.twa.app"));
  base::RunLoop runloop;
  EXPECT_CALL(*delegate_, OnDoneCreatingPaymentApps()).WillOnce([&runloop] {
    runloop.Quit();
  });
  EXPECT_CALL(*delegate_, OnPaymentAppCreationError(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*delegate_,
              OnPaymentAppCreated(PaymentAppMatches(
                  PaymentApp::Type::NATIVE_MOBILE_APP, "com.twa.app",
                  "https://play.google.com/billing")))
      .Times(support_->AreAndroidAppsSupportedOnThisPlatform() ? 1 : 0);
  EXPECT_CALL(*delegate_, OnPaymentAppCreated(PaymentAppMatches(
                              PaymentApp::Type::NATIVE_MOBILE_APP,
                              "com.twa.app", "https://example.test")))
      .Times(0);
  EXPECT_CALL(*delegate_, GetChromeOSTWAInstanceId())
      .Times(support_->AreAndroidAppsSupportedOnThisPlatform() ? 1 : 0);

  std::vector<std::unique_ptr<AndroidAppDescription>> apps;
  apps.emplace_back(std::make_unique<AndroidAppDescription>());
  apps.back()->package = "com.twa.app";
  apps.back()->service_names.push_back("com.twa.app.Service");

  apps.back()->activities.emplace_back(
      std::make_unique<AndroidActivityDescription>());
  apps.back()->activities.back()->name = "com.twa.app.ActivityOne";
  apps.back()->activities.back()->default_payment_method =
      "https://play.google.com/billing";

  apps.back()->activities.emplace_back(
      std::make_unique<AndroidActivityDescription>());
  apps.back()->activities.back()->name = "com.twa.app.ActivityTwo";
  apps.back()->activities.back()->default_payment_method =
      "https://example.test";

  support_->ExpectQueryListOfPaymentAppsAndRespond(std::move(apps));
  support_->ExpectQueryIsReadyToPayAndRespond(/*is_ready_to_pay=*/true);

  factory_.Create(delegate_->GetWeakPtr());
  runloop.Run();
}

// At most one IS_READY_TO_PAY service is allowed in an Android payment app.
TEST_F(AndroidPaymentAppFactoryIntegrationTest,
       ReturnErrorWhenMoreThanOneServiceInApp) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(*delegate_, GetTwaPackageName)
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>("com.example.app"));
  base::RunLoop runloop;
  EXPECT_CALL(*delegate_, OnDoneCreatingPaymentApps()).WillOnce([&runloop] {
    runloop.Quit();
  });

  EXPECT_CALL(*delegate_,
              OnPaymentAppCreationError(
                  "Found more than one IS_READY_TO_PAY service, but "
                  "at most one service is supported.",
                  AppCreationFailureReason::UNKNOWN))
      .Times(support_->AreAndroidAppsSupportedOnThisPlatform() ? 1 : 0);

  EXPECT_CALL(*delegate_, OnPaymentAppCreated(testing::_)).Times(0);
  EXPECT_CALL(*delegate_, GetChromeOSTWAInstanceId()).Times(0);

  std::vector<std::unique_ptr<AndroidAppDescription>> apps;
  apps.emplace_back(std::make_unique<AndroidAppDescription>());
  apps.back()->package = "com.example.app";

  // Two IS_READY_TO_PAY services:
  apps.back()->service_names.push_back("com.example.app.ServiceOne");
  apps.back()->service_names.push_back("com.example.app.ServiceTwo");

  apps.back()->activities.emplace_back(
      std::make_unique<AndroidActivityDescription>());
  apps.back()->activities.back()->name = "com.example.app.Activity";
  apps.back()->activities.back()->default_payment_method =
      "https://play.google.com/billing";
  support_->ExpectQueryListOfPaymentAppsAndRespond(std::move(apps));
  support_->ExpectNoIsReadyToPayQuery();

  factory_.Create(delegate_->GetWeakPtr());
  runloop.Run();
}

}  // namespace
}  // namespace payments
