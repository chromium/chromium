// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android_app_communication.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "components/payments/content/android_app_communication_test_support.h"
#include "components/payments/core/android_app_description.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace payments {
namespace {

std::vector<std::unique_ptr<AndroidAppDescription>> createApp(
    const std::vector<std::string>& activity_names,
    const std::string& default_payment_method,
    const std::vector<std::string>& service_names) {
  auto app = std::make_unique<AndroidAppDescription>();

  for (const auto& activity_name : activity_names) {
    auto activity = std::make_unique<AndroidActivityDescription>();
    activity->name = activity_name;
    activity->default_payment_method = default_payment_method;
    app->activities.emplace_back(std::move(activity));
  }

  app->package = "com.example.app";
  app->service_names = service_names;

  std::vector<std::unique_ptr<AndroidAppDescription>> apps;
  apps.emplace_back(std::move(app));

  return apps;
}

class AndroidAppCommunicationTest : public testing::Test {
 public:
  AndroidAppCommunicationTest()
      : support_(AndroidAppCommunicationTestSupport::Create()),
        web_contents_(
            web_contents_factory_.CreateWebContents(support_->context())) {}
  ~AndroidAppCommunicationTest() override = default;

  AndroidAppCommunicationTest(const AndroidAppCommunicationTest& other) =
      delete;
  AndroidAppCommunicationTest& operator=(
      const AndroidAppCommunicationTest& other) = delete;

  std::unique_ptr<AndroidAppCommunicationTestSupport> support_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  std::optional<base::UnguessableToken> twa_instance_identifier_ =
      base::UnguessableToken::Create();
};

TEST_F(AndroidAppCommunicationTest, OneInstancePerBrowserContext) {
  auto communication_one =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  auto communication_two =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  EXPECT_EQ(communication_one.get(), communication_two.get());
}

TEST_F(AndroidAppCommunicationTest, NoPaymentInstanceForGetAppDescriptions) {
  // Intentionally do not set an instance.

  support_->ExpectNoListOfPaymentAppsQuery();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  base::test::TestFuture<const std::optional<std::string>&,
                         std::vector<std::unique_ptr<AndroidAppDescription>>>
      future;
  communication->GetAppDescriptions("com.example.app", future.GetCallback());
  auto error = future.Get<std::optional<std::string>>();
  const auto& apps =
      future.Get<std::vector<std::unique_ptr<AndroidAppDescription>>>();
  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(support_->GetNoInstanceExpectedErrorString(), error.value());
  } else {
    EXPECT_FALSE(error.has_value());
  }

  EXPECT_TRUE(apps.empty());
}

TEST_F(AndroidAppCommunicationTest, NoAppDescriptions) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryListOfPaymentAppsAndRespond({});

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  base::test::TestFuture<const std::optional<std::string>&,
                         std::vector<std::unique_ptr<AndroidAppDescription>>>
      future;
  communication->GetAppDescriptions("com.example.app", future.GetCallback());
  auto error = future.Get<std::optional<std::string>>();
  const auto& apps =
      future.Get<std::vector<std::unique_ptr<AndroidAppDescription>>>();
  EXPECT_FALSE(error.has_value());
  EXPECT_TRUE(apps.empty());
}

TEST_F(AndroidAppCommunicationTest, TwoActivitiesInPackage) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryListOfPaymentAppsAndRespond(
      createApp({"com.example.app.ActivityOne", "com.example.app.ActivityTwo"},
                "https://play.google.com/billing", {}));

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  base::test::TestFuture<const std::optional<std::string>&,
                         std::vector<std::unique_ptr<AndroidAppDescription>>>
      future;
  communication->GetAppDescriptions("com.example.app", future.GetCallback());
  auto error = future.Get<std::optional<std::string>>();
  const auto& apps =
      future.Get<std::vector<std::unique_ptr<AndroidAppDescription>>>();
  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(
        "Found more than one PAY activity in the Trusted Web Activity, but at "
        "most one activity is supported.",
        error.value());
  } else {
    EXPECT_FALSE(error.has_value());
  }
  EXPECT_TRUE(apps.empty());
}

TEST_F(AndroidAppCommunicationTest, TwoServicesInPackage) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryListOfPaymentAppsAndRespond(
      createApp({"com.example.app.Activity"}, "https://play.google.com/billing",
                {"com.example.app.ServiceOne", "com.example.app.ServiceTwo"}));

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  base::test::TestFuture<const std::optional<std::string>&,
                         std::vector<std::unique_ptr<AndroidAppDescription>>>
      future;
  communication->GetAppDescriptions("com.example.app", future.GetCallback());
  auto error = future.Get<std::optional<std::string>>();
  const auto& apps =
      future.Get<std::vector<std::unique_ptr<AndroidAppDescription>>>();
  EXPECT_FALSE(error.has_value());
  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    ASSERT_EQ(1u, apps.size());
    ASSERT_NE(nullptr, apps.front().get());
    EXPECT_EQ("com.example.app", apps.front()->package);

    // The logic for checking for multiple services is cross-platform in
    // android_payment_app_factory.cc, so the platform-specific implementations
    // of android_app_communication.h do not check for this error condition.
    std::vector<std::string> expected_service_names = {
        "com.example.app.ServiceOne", "com.example.app.ServiceTwo"};
    EXPECT_EQ(expected_service_names, apps.front()->service_names);

    ASSERT_EQ(1u, apps.front()->activities.size());
    ASSERT_NE(nullptr, apps.front()->activities.front().get());
    EXPECT_EQ("com.example.app.Activity",
              apps.front()->activities.front()->name);
    EXPECT_EQ("https://play.google.com/billing",
              apps.front()->activities.front()->default_payment_method);
  } else {
    EXPECT_TRUE(apps.empty());
  }
}

TEST_F(AndroidAppCommunicationTest, ActivityAndService) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryListOfPaymentAppsAndRespond(
      createApp({"com.example.app.Activity"}, "https://play.google.com/billing",
                {"com.example.app.Service"}));

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  base::test::TestFuture<const std::optional<std::string>&,
                         std::vector<std::unique_ptr<AndroidAppDescription>>>
      future;
  communication->GetAppDescriptions("com.example.app", future.GetCallback());
  auto error = future.Get<std::optional<std::string>>();
  const auto& apps =
      future.Get<std::vector<std::unique_ptr<AndroidAppDescription>>>();
  EXPECT_FALSE(error.has_value());

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    ASSERT_EQ(1u, apps.size());
    ASSERT_NE(nullptr, apps.front().get());
    EXPECT_EQ("com.example.app", apps.front()->package);
    EXPECT_EQ(std::vector<std::string>{"com.example.app.Service"},
              apps.front()->service_names);
    ASSERT_EQ(1u, apps.front()->activities.size());
    ASSERT_NE(nullptr, apps.front()->activities.front().get());
    EXPECT_EQ("com.example.app.Activity",
              apps.front()->activities.front()->name);
    EXPECT_EQ("https://play.google.com/billing",
              apps.front()->activities.front()->default_payment_method);
  } else {
    EXPECT_TRUE(apps.empty());
  }
}

TEST_F(AndroidAppCommunicationTest, OnlyActivity) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryListOfPaymentAppsAndRespond(createApp(
      {"com.example.app.Activity"}, "https://play.google.com/billing", {}));

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  base::test::TestFuture<const std::optional<std::string>&,
                         std::vector<std::unique_ptr<AndroidAppDescription>>>
      future;
  communication->GetAppDescriptions("com.example.app", future.GetCallback());
  auto error = future.Get<std::optional<std::string>>();
  const auto& apps =
      future.Get<std::vector<std::unique_ptr<AndroidAppDescription>>>();
  EXPECT_FALSE(error.has_value());

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    ASSERT_EQ(1u, apps.size());
    ASSERT_NE(nullptr, apps.front().get());
    EXPECT_EQ("com.example.app", apps.front()->package);
    EXPECT_TRUE(apps.front()->service_names.empty());
    ASSERT_EQ(1u, apps.front()->activities.size());
    ASSERT_NE(nullptr, apps.front()->activities.front().get());
    EXPECT_EQ("com.example.app.Activity",
              apps.front()->activities.front()->name);
    EXPECT_EQ("https://play.google.com/billing",
              apps.front()->activities.front()->default_payment_method);
  } else {
    EXPECT_TRUE(apps.empty());
  }
}

TEST_F(AndroidAppCommunicationTest, OutsideOfTwa) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectNoListOfPaymentAppsQuery();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  base::test::TestFuture<const std::optional<std::string>&,
                         std::vector<std::unique_ptr<AndroidAppDescription>>>
      future;
  communication->GetAppDescriptions(
      /*twa_package_name=*/"",  // Empty string means this is not TWA.
      future.GetCallback());
  auto error = future.Get<std::optional<std::string>>();
  const auto& apps =
      future.Get<std::vector<std::unique_ptr<AndroidAppDescription>>>();

  EXPECT_FALSE(error.has_value());
  EXPECT_TRUE(apps.empty());
}

TEST_F(AndroidAppCommunicationTest, NoPaymentInstanceForIsReadyToPay) {
  // Intentionally do not set an instance.

  support_->ExpectNoIsReadyToPayQuery();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert("{}");

  base::test::TestFuture<const std::optional<std::string>&, bool> future;
  communication->IsReadyToPay("com.example.app", "com.example.app.Service",
                              stringified_method_data,
                              GURL("https://top-level-origin.com"),
                              GURL("https://payment-request-origin.com"),
                              "payment-request-id", future.GetCallback());
  auto error = future.Get<std::optional<std::string>>();
  auto is_ready_to_pay = future.Get<bool>();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(support_->GetNoInstanceExpectedErrorString(), error.value());
  EXPECT_FALSE(is_ready_to_pay);
}

TEST_F(AndroidAppCommunicationTest, TwaIsReadyToPayOnlyWithPlayBilling) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectNoIsReadyToPayQuery();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://example.test"].insert("{}");
  base::test::TestFuture<const std::optional<std::string>&, bool> future;
  base::RunLoop run_loop;
  communication->IsReadyToPay("com.example.app", "com.example.app.Service",
                              stringified_method_data,
                              GURL("https://top-level-origin.com"),
                              GURL("https://payment-request-origin.com"),
                              "payment-request-id", future.GetCallback());
  auto error = future.Get<std::optional<std::string>>();
  auto is_ready_to_pay = future.Get<bool>();
  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error.has_value());
  } else {
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error.value());
  }

  EXPECT_FALSE(is_ready_to_pay);
}

TEST_F(AndroidAppCommunicationTest, MoreThanOnePaymentMethodDataNotReadyToPay) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectNoIsReadyToPayQuery();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert(
      "{\"product_id\": \"1\"}");
  stringified_method_data["https://play.google.com/billing"].insert(
      "{\"product_id\": \"2\"}");

  base::test::TestFuture<const std::optional<std::string>&, bool> future;
  communication->IsReadyToPay("com.example.app", "com.example.app.Service",
                              stringified_method_data,
                              GURL("https://top-level-origin.com"),
                              GURL("https://payment-request-origin.com"),
                              "payment-request-id", future.GetCallback());
  auto error = future.Get<std::optional<std::string>>();
  auto is_ready_to_pay = future.Get<bool>();
  ASSERT_TRUE(error.has_value());

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_EQ("At most one payment method specific data is supported.",
              error.value());
  } else {
    EXPECT_EQ("Unable to invoke Android apps.", error.value());
  }

  EXPECT_FALSE(is_ready_to_pay);
}

TEST_F(AndroidAppCommunicationTest, EmptyMethodDataIsReadyToPay) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryIsReadyToPayAndRespond(true);

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data.insert(std::make_pair(
      "https://play.google.com/billing", std::set<std::string>()));

  base::test::TestFuture<const std::optional<std::string>&, bool> future;
  communication->IsReadyToPay("com.example.app", "com.example.app.Service",
                              stringified_method_data,
                              GURL("https://top-level-origin.com"),
                              GURL("https://payment-request-origin.com"),
                              "payment-request-id", future.GetCallback());
  auto error = future.Get<std::optional<std::string>>();
  auto is_ready_to_pay = future.Get<bool>();
  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error.has_value());
    EXPECT_TRUE(is_ready_to_pay);
  } else {
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error.value());
    EXPECT_FALSE(is_ready_to_pay);
  }
}

TEST_F(AndroidAppCommunicationTest, NotReadyToPay) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryIsReadyToPayAndRespond(false);

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert("{}");

  base::test::TestFuture<const std::optional<std::string>&, bool> future;
  communication->IsReadyToPay("com.example.app", "com.example.app.Service",
                              stringified_method_data,
                              GURL("https://top-level-origin.com"),
                              GURL("https://payment-request-origin.com"),
                              "payment-request-id", future.GetCallback());
  auto error = future.Get<std::optional<std::string>>();
  auto is_ready_to_pay = future.Get<bool>();
  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error.has_value());
  } else {
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error.value());
  }

  EXPECT_FALSE(is_ready_to_pay);
}

TEST_F(AndroidAppCommunicationTest, ReadyToPay) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectQueryIsReadyToPayAndRespond(true);

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert("{}");

  base::test::TestFuture<const std::optional<std::string>&, bool> future;
  communication->IsReadyToPay("com.example.app", "com.example.app.Service",
                              stringified_method_data,
                              GURL("https://top-level-origin.com"),
                              GURL("https://payment-request-origin.com"),
                              "payment-request-id", future.GetCallback());
  auto error = future.Get<std::optional<std::string>>();
  auto is_ready_to_pay = future.Get<bool>();
  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error.has_value());
    EXPECT_TRUE(is_ready_to_pay);
  } else {
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error.value());
    EXPECT_FALSE(is_ready_to_pay);
  }
}

TEST_F(AndroidAppCommunicationTest, NoPaymentInstanceForInvokePaymentApp) {
  // Intentionally do not set an instance.

  support_->ExpectNoPaymentAppInvoke();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert("{}");

  base::test::TestFuture<const std::optional<std::string>&, bool,
                         const std::string&, const std::string&>
      future;
  communication->InvokePaymentApp(
      "com.example.app", "com.example.app.Activity", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::UnguessableToken::Create(), web_contents_, twa_instance_identifier_,
      future.GetCallback());
  auto error = future.Get<0>();
  const auto& is_activity_result_ok = future.Get<1>();
  const auto& payment_method_identifier = future.Get<2>();
  const auto& stringified_details = future.Get<3>();

  EXPECT_EQ(support_->GetNoInstanceExpectedErrorString(), error.value());
  EXPECT_FALSE(is_activity_result_ok);
  EXPECT_TRUE(payment_method_identifier.empty());
  EXPECT_EQ("{}", stringified_details);
}

TEST_F(AndroidAppCommunicationTest, TwaPaymentOnlyWithPlayBilling) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectNoPaymentAppInvoke();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://example.test"].insert("{}");

  base::test::TestFuture<const std::optional<std::string>&, bool,
                         const std::string&, const std::string&>
      future;
  communication->InvokePaymentApp(
      "com.example.app", "com.example.app.Activity", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::UnguessableToken::Create(), web_contents_, twa_instance_identifier_,
      future.GetCallback());
  auto error = future.Get<0>();
  const auto& is_activity_result_ok = future.Get<1>();
  const auto& payment_method_identifier = future.Get<2>();
  const auto& stringified_details = future.Get<3>();

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error.has_value());
    EXPECT_FALSE(is_activity_result_ok);
    EXPECT_TRUE(payment_method_identifier.empty());
    EXPECT_EQ("{}", stringified_details);
  } else {
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error.value());
  }
}

TEST_F(AndroidAppCommunicationTest, NoPaymentWithMoreThanOnePaymentMethodData) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectNoPaymentAppInvoke();

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert(
      "{\"product_id\": \"1\"}");
  stringified_method_data["https://play.google.com/billing"].insert(
      "{\"product_id\": \"2\"}");

  base::test::TestFuture<const std::optional<std::string>&, bool,
                         const std::string&, const std::string&>
      future;
  communication->InvokePaymentApp(
      "com.example.app", "com.example.app.Activity", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::UnguessableToken::Create(), web_contents_, twa_instance_identifier_,
      future.GetCallback());
  auto error = future.Get<0>();
  const auto& is_activity_result_ok = future.Get<1>();
  const auto& payment_method_identifier = future.Get<2>();
  const auto& stringified_details = future.Get<3>();

  EXPECT_FALSE(is_activity_result_ok);
  EXPECT_EQ("{}", stringified_details);
  EXPECT_TRUE(payment_method_identifier.empty());
  ASSERT_TRUE(error.has_value());

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_EQ("At most one payment method specific data is supported.",
              error.value());
  } else {
    EXPECT_EQ("Unable to invoke Android apps.", error.value());
  }
}

TEST_F(AndroidAppCommunicationTest, PaymentWithEmptyMethodData) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectInvokePaymentAppAndRespond(
      /*is_activity_result_ok=*/true,
      /*payment_method_identifier=*/"https://play.google.com/billing",
      /*stringified_details*/ "{\"status\": \"ok\"}");

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data.insert(std::make_pair(
      "https://play.google.com/billing", std::set<std::string>()));

  base::test::TestFuture<const std::optional<std::string>&, bool,
                         const std::string&, const std::string&>
      future;
  communication->InvokePaymentApp(
      "com.example.app", "com.example.app.Activity", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::UnguessableToken::Create(), web_contents_, twa_instance_identifier_,
      future.GetCallback());
  auto error = future.Get<0>();
  const auto& is_activity_result_ok = future.Get<1>();
  const auto& payment_method_identifier = future.Get<2>();
  const auto& stringified_details = future.Get<3>();

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error.has_value());
    EXPECT_TRUE(is_activity_result_ok);
    EXPECT_EQ("https://play.google.com/billing", payment_method_identifier);
    EXPECT_EQ("{\"status\": \"ok\"}", stringified_details);
  } else {
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error.value());
  }
}

TEST_F(AndroidAppCommunicationTest, UserCancelInvokePaymentApp) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectInvokePaymentAppAndRespond(
      /*is_activity_result_ok=*/false,
      /*payment_method_identifier=*/"https://play.google.com/billing",
      /*stringified_details*/ "{}");

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert("{}");

  base::test::TestFuture<const std::optional<std::string>&, bool,
                         const std::string&, const std::string&>
      future;
  communication->InvokePaymentApp(
      "com.example.app", "com.example.app.Activity", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::UnguessableToken::Create(), web_contents_, twa_instance_identifier_,
      future.GetCallback());
  auto error = future.Get<0>();
  const auto& is_activity_result_ok = future.Get<1>();
  const auto& payment_method_identifier = future.Get<2>();
  const auto& stringified_details = future.Get<3>();

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error.has_value());
    EXPECT_FALSE(is_activity_result_ok);
    EXPECT_EQ("https://play.google.com/billing", payment_method_identifier);
    EXPECT_EQ("{}", stringified_details);
  } else {
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error.value());
  }
}

TEST_F(AndroidAppCommunicationTest, UserConfirmInvokePaymentApp) {
  auto scoped_initialization = support_->CreateScopedInitialization();

  support_->ExpectInvokePaymentAppAndRespond(
      /*is_activity_result_ok=*/true,
      /*payment_method_identifier=*/"https://play.google.com/billing",
      /*stringified_details*/ "{\"status\": \"ok\"}");

  auto communication =
      AndroidAppCommunication::GetForBrowserContext(support_->context());
  communication->SetForTesting();

  std::map<std::string, std::set<std::string>> stringified_method_data;
  stringified_method_data["https://play.google.com/billing"].insert("{}");

  base::test::TestFuture<const std::optional<std::string>&, bool,
                         const std::string&, const std::string&>
      future;
  communication->InvokePaymentApp(
      "com.example.app", "com.example.app.Activity", stringified_method_data,
      GURL("https://top-level-origin.com"),
      GURL("https://payment-request-origin.com"), "payment-request-id",
      base::UnguessableToken::Create(), web_contents_, twa_instance_identifier_,
      future.GetCallback());
  auto error = future.Get<0>();
  const auto& is_activity_result_ok = future.Get<1>();
  const auto& payment_method_identifier = future.Get<2>();
  const auto& stringified_details = future.Get<3>();

  if (support_->AreAndroidAppsSupportedOnThisPlatform()) {
    EXPECT_FALSE(error.has_value());
    EXPECT_TRUE(is_activity_result_ok);
    EXPECT_EQ("https://play.google.com/billing", payment_method_identifier);
    EXPECT_EQ("{\"status\": \"ok\"}", stringified_details);
  } else {
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ("Unable to invoke Android apps.", error.value());
  }
}

}  // namespace
}  // namespace payments
