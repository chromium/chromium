// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android_payment_app_factory.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "components/autofill/core/browser/payments/internal_authenticator.h"
#include "components/payments/content/android_app_communication.h"
#include "components/payments/content/android_app_communication_test_support.h"
#include "components/payments/content/payment_app_factory.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/core/android_app_description.h"
#include "content/public/browser/web_contents.h"
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

// A mock delegate for payment app factories.
class MockPaymentAppFactoryDelegate : public PaymentAppFactory::Delegate {
 public:
  explicit MockPaymentAppFactoryDelegate(content::BrowserContext* context)
      : web_contents_(web_contents_factory_.CreateWebContents(context)),
        top_origin_("https://top-origin.test"),
        frame_origin_("https://frame-origin.test") {
    SetRequestedPaymentMethod("https://play.google.com/billing");
  }

  ~MockPaymentAppFactoryDelegate() override = default;

  void SetRequestedPaymentMethod(const std::string& method) {
    auto details = mojom::PaymentDetails::New();
    details->id = "id";

    std::vector<mojom::PaymentMethodDataPtr> methods;
    methods.emplace_back(mojom::PaymentMethodData::New());
    methods.back()->supported_method = method;
    methods.back()->stringified_data = "{}";

    spec_ = std::make_unique<PaymentRequestSpec>(
        mojom::PaymentOptions::New(), std::move(details), std::move(methods),
        /*observer=*/nullptr, /*app_locale=*/"en-US");
  }

  void set_is_off_the_record() { is_off_the_record_ = true; }

  // PaymentAppFactory::Delegate implementation:
  content::WebContents* GetWebContents() override { return web_contents_; }
  const GURL& GetTopOrigin() override { return top_origin_; }
  const GURL& GetFrameOrigin() override { return frame_origin_; }
  MOCK_METHOD0(GetFrameSecurityOrigin, const url::Origin&());
  MOCK_CONST_METHOD0(GetInitiatorRenderFrameHost, content::RenderFrameHost*());
  MOCK_CONST_METHOD0(GetMethodData,
                     const std::vector<mojom::PaymentMethodDataPtr>&());
  MOCK_CONST_METHOD0(CreateInternalAuthenticator,
                     std::unique_ptr<autofill::InternalAuthenticator>());
  MOCK_CONST_METHOD0(GetPaymentManifestWebDataService,
                     scoped_refptr<PaymentManifestWebDataService>());
  MOCK_METHOD0(MayCrawlForInstallablePaymentApps, bool());
  bool IsOffTheRecord() const override { return is_off_the_record_; }
  base::WeakPtr<PaymentRequestSpec> GetSpec() const override {
    return spec_->AsWeakPtr();
  }
  MOCK_CONST_METHOD0(GetTwaPackageName, std::string());
  MOCK_METHOD0(ShowProcessingSpinner, void());
  MOCK_METHOD0(GetBillingProfiles,
               const std::vector<autofill::AutofillProfile*>&());
  MOCK_METHOD0(IsRequestedAutofillDataAvailable, bool());
  MOCK_CONST_METHOD0(GetPaymentRequestDelegate,
                     ContentPaymentRequestDelegate*());
  MOCK_METHOD1(OnPaymentAppCreated, void(std::unique_ptr<PaymentApp> app));
  MOCK_METHOD1(OnPaymentAppCreationError,
               void(const std::string& error_message));
  MOCK_CONST_METHOD0(SkipCreatingNativePaymentApps, bool());
  MOCK_METHOD0(OnDoneCreatingPaymentApps, void());
  MOCK_METHOD0(SetCanMakePaymentEvenWithoutApps, void());

  base::WeakPtr<PaymentAppFactory::Delegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  content::TestWebContentsFactory web_contents_factory_;
  content::WebContents* web_contents_;
  GURL top_origin_;
  GURL frame_origin_;
  std::unique_ptr<PaymentRequestSpec> spec_;
  bool is_off_the_record_ = false;
  base::WeakPtrFactory<PaymentAppFactory::Delegate> weak_ptr_factory_{this};
};

// The scaffolding for testing the Android payment app factory.
class AndroidPaymentAppFactoryTest : public testing::Test {
 public:
  AndroidPaymentAppFactoryTest()
      : support_(AndroidAppCommunicationTestSupport::Create()),
        delegate_(support_->context()),
        factory_(GetCommunication(support_->context())) {}

  std::unique_ptr<AndroidAppCommunicationTestSupport> support_;
  MockPaymentAppFactoryDelegate delegate_;
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
// Aneroid payment apps on a platform that supports such apps, e.g, when ARC is
// disabled on Chrome OS.
TEST_F(AndroidPaymentAppFactoryTest, FactoryReturnsErrorWithoutArc) {
  EXPECT_CALL(delegate_, GetTwaPackageName())
      .WillRepeatedly(testing::Return("com.example.app"));
  EXPECT_CALL(delegate_, OnDoneCreatingPaymentApps());

  EXPECT_CALL(delegate_,
              OnPaymentAppCreationError("Unable to invoke Android apps."))
      .Times(support_->AreAndroidAppsSupportedOnThisPlatform() ? 1 : 0);
  EXPECT_CALL(delegate_, OnPaymentAppCreated(testing::_)).Times(0);
  support_->ExpectNoListOfPaymentAppsQuery();
  support_->ExpectNoIsReadyToPayQuery();

  factory_.Create(delegate_.GetWeakPtr());
}

// The payment app factory should not return any errors when there're no Android
// payment apps available.
TEST_F(AndroidPaymentAppFactoryTest, NoErrorsWhenNoApps) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(delegate_, GetTwaPackageName())
      .WillRepeatedly(testing::Return("com.example.app"));
  EXPECT_CALL(delegate_, OnDoneCreatingPaymentApps());

  EXPECT_CALL(delegate_, OnPaymentAppCreationError(testing::_)).Times(0);
  EXPECT_CALL(delegate_, OnPaymentAppCreated(testing::_)).Times(0);
  support_->ExpectQueryListOfPaymentAppsAndRespond({});
  support_->ExpectNoIsReadyToPayQuery();

  factory_.Create(delegate_.GetWeakPtr());
}

// The |arg| is of type std::unique_ptr<PaymentApp>.
MATCHER_P3(PaymentAppMatches, type, package, method, "") {
  return arg->type() == type && arg->GetId() == package &&
         base::Contains(arg->GetAppMethodNames(), method);
}

// The payment app factory should return the TWA payment app when running in TWA
// mode, even when it does not have an IS_READY_TO_PAY service.
TEST_F(AndroidPaymentAppFactoryTest, FindAppsThatDoNotHaveReadyToPayService) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(delegate_, GetTwaPackageName())
      .WillRepeatedly(testing::Return("com.example.app"));
  EXPECT_CALL(delegate_, OnDoneCreatingPaymentApps());

  EXPECT_CALL(delegate_, OnPaymentAppCreationError(testing::_)).Times(0);

  EXPECT_CALL(delegate_,
              OnPaymentAppCreated(PaymentAppMatches(
                  PaymentApp::Type::NATIVE_MOBILE_APP, "com.example.app",
                  "https://play.google.com/billing")))
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

  factory_.Create(delegate_.GetWeakPtr());
}

// The payment app factory should return one payment app and should not query
// the IS_READY_TO_PAY service, because of being off the record.
TEST_F(AndroidPaymentAppFactoryTest,
       DoNotQueryReadyToPaySericeWhenOffTheRecord) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  // Simulate being off the record.
  delegate_.set_is_off_the_record();

  EXPECT_CALL(delegate_, GetTwaPackageName())
      .WillRepeatedly(testing::Return("com.example.app"));
  EXPECT_CALL(delegate_, OnDoneCreatingPaymentApps());

  EXPECT_CALL(delegate_, OnPaymentAppCreationError(testing::_)).Times(0);

  EXPECT_CALL(delegate_,
              OnPaymentAppCreated(PaymentAppMatches(
                  PaymentApp::Type::NATIVE_MOBILE_APP, "com.example.app",
                  "https://play.google.com/billing")))
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

  factory_.Create(delegate_.GetWeakPtr());
}

// The payment app factory should return the TWA payment app that returns true
// from IS_READY_TO_PAY service when running in TWA mode.
TEST_F(AndroidPaymentAppFactoryTest,
       FindTheTwaPaymentAppThatIsReadyToPayInTwaMode) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(delegate_, GetTwaPackageName())
      .WillRepeatedly(testing::Return("com.twa.app"));
  EXPECT_CALL(delegate_, OnDoneCreatingPaymentApps());

  EXPECT_CALL(delegate_, OnPaymentAppCreationError(testing::_)).Times(0);

  EXPECT_CALL(delegate_, OnPaymentAppCreated(PaymentAppMatches(
                             PaymentApp::Type::NATIVE_MOBILE_APP, "com.twa.app",
                             "https://play.google.com/billing")))
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

  factory_.Create(delegate_.GetWeakPtr());
}

// The payment app factory should return no payment apps when IS_READY_TO_PAY
// service returns false.
TEST_F(AndroidPaymentAppFactoryTest, IgnoreAppsThatAreNotReadyToPay) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(delegate_, GetTwaPackageName())
      .WillRepeatedly(testing::Return("com.example.app"));
  EXPECT_CALL(delegate_, OnDoneCreatingPaymentApps());
  EXPECT_CALL(delegate_, OnPaymentAppCreationError(testing::_)).Times(0);
  EXPECT_CALL(delegate_, OnPaymentAppCreated(testing::_)).Times(0);

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

  factory_.Create(delegate_.GetWeakPtr());
}

// The payment app factory should return the correct TWA payment app out of two
// installed payment apps, when running in TWA mode.
TEST_F(AndroidPaymentAppFactoryTest, FindTheCorrectTwaAppInTwaMode) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(delegate_, GetTwaPackageName())
      .WillRepeatedly(testing::Return("com.correct-twa.app"));
  EXPECT_CALL(delegate_, OnDoneCreatingPaymentApps());

  EXPECT_CALL(delegate_, OnPaymentAppCreationError(testing::_)).Times(0);

  EXPECT_CALL(delegate_,
              OnPaymentAppCreated(PaymentAppMatches(
                  PaymentApp::Type::NATIVE_MOBILE_APP, "com.correct-twa.app",
                  "https://play.google.com/billing")))
      .Times(support_->AreAndroidAppsSupportedOnThisPlatform() ? 1 : 0);
  EXPECT_CALL(delegate_,
              OnPaymentAppCreated(PaymentAppMatches(
                  PaymentApp::Type::NATIVE_MOBILE_APP, "com.different.app",
                  "https://play.google.com/billing")))
      .Times(0);

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

  factory_.Create(delegate_.GetWeakPtr());
}

// The payment app factory does not return non-TWA payment apps when running in
// TWA mode.
TEST_F(AndroidPaymentAppFactoryTest, IgnoreNonTwaAppsInTwaMode) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(delegate_, GetTwaPackageName())
      .WillRepeatedly(testing::Return("com.twa.app"));
  EXPECT_CALL(delegate_, OnDoneCreatingPaymentApps());
  EXPECT_CALL(delegate_, OnPaymentAppCreationError(testing::_)).Times(0);
  EXPECT_CALL(delegate_, OnPaymentAppCreated(testing::_)).Times(0);

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

  factory_.Create(delegate_.GetWeakPtr());
}

// The payment app factory does not return any payment apps when not running
// inside of TWA.
TEST_F(AndroidPaymentAppFactoryTest, DoNotLookForAppsWhenOutsideOfTwaMode) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(delegate_, GetTwaPackageName())
      .WillRepeatedly(testing::Return(""));
  EXPECT_CALL(delegate_, OnDoneCreatingPaymentApps());
  EXPECT_CALL(delegate_, OnPaymentAppCreationError(testing::_)).Times(0);
  EXPECT_CALL(delegate_, OnPaymentAppCreated(testing::_)).Times(0);
  support_->ExpectNoListOfPaymentAppsQuery();

  factory_.Create(delegate_.GetWeakPtr());
}

// The Android payment app factory works only with TWA specific payment methods.
TEST_F(AndroidPaymentAppFactoryTest, DoNotLookForAppsForNonTwaMethod) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  // "https://example.test" is not a TWA specific payment method.
  delegate_.SetRequestedPaymentMethod("https://example.test");

  EXPECT_CALL(delegate_, GetTwaPackageName())
      .WillRepeatedly(testing::Return("com.example.app"));
  EXPECT_CALL(delegate_, OnDoneCreatingPaymentApps());
  EXPECT_CALL(delegate_, OnPaymentAppCreationError(testing::_)).Times(0);
  EXPECT_CALL(delegate_, OnPaymentAppCreated(testing::_)).Times(0);
  support_->ExpectNoListOfPaymentAppsQuery();
  support_->ExpectNoIsReadyToPayQuery();

  factory_.Create(delegate_.GetWeakPtr());
}

// If the TWA supports a non-TWA-specific payment method, then it should be
// ignored.
TEST_F(AndroidPaymentAppFactoryTest, IgnoreNonTwaMethodInTheTwa) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(delegate_, GetTwaPackageName())
      .WillRepeatedly(testing::Return("com.twa.app"));
  EXPECT_CALL(delegate_, OnDoneCreatingPaymentApps());
  EXPECT_CALL(delegate_, OnPaymentAppCreationError(testing::_)).Times(0);
  EXPECT_CALL(delegate_, OnPaymentAppCreated(testing::_)).Times(0);

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

  factory_.Create(delegate_.GetWeakPtr());
}

// If the TWA supports both a TWA-specific and a non-TWA-specific payment
// method, then only the TWA-specific payment method activity should be
// returned.
TEST_F(AndroidPaymentAppFactoryTest,
       FindOnlyActivitiesWithTwaSpecificMethodName) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(delegate_, GetTwaPackageName())
      .WillRepeatedly(testing::Return("com.twa.app"));
  EXPECT_CALL(delegate_, OnDoneCreatingPaymentApps());
  EXPECT_CALL(delegate_, OnPaymentAppCreationError(testing::_)).Times(0);
  EXPECT_CALL(delegate_, OnPaymentAppCreated(PaymentAppMatches(
                             PaymentApp::Type::NATIVE_MOBILE_APP, "com.twa.app",
                             "https://play.google.com/billing")))
      .Times(support_->AreAndroidAppsSupportedOnThisPlatform() ? 1 : 0);
  EXPECT_CALL(delegate_, OnPaymentAppCreated(PaymentAppMatches(
                             PaymentApp::Type::NATIVE_MOBILE_APP, "com.twa.app",
                             "https://example.test")))
      .Times(0);

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

  factory_.Create(delegate_.GetWeakPtr());
}

// At most one IS_READY_TO_PAY service is allowed in an Android payment app.
TEST_F(AndroidPaymentAppFactoryTest, ReturnErrorWhenMoreThanOneServiceInApp) {
  // Enable invoking Android payment apps on those platforms that support it.
  auto scoped_initialization_ = support_->CreateScopedInitialization();

  EXPECT_CALL(delegate_, GetTwaPackageName())
      .WillRepeatedly(testing::Return("com.example.app"));
  EXPECT_CALL(delegate_, OnDoneCreatingPaymentApps());

  EXPECT_CALL(delegate_, OnPaymentAppCreationError(
                             "Found more than one IS_READY_TO_PAY service, but "
                             "at most one service is supported."))
      .Times(support_->AreAndroidAppsSupportedOnThisPlatform() ? 1 : 0);

  EXPECT_CALL(delegate_, OnPaymentAppCreated(testing::_)).Times(0);

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

  factory_.Create(delegate_.GetWeakPtr());
}

}  // namespace
}  // namespace payments
