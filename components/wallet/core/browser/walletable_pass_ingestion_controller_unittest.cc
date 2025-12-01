// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/walletable_pass_ingestion_controller.h"

#include <memory>

#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/hints/mock_optimization_guide_decider.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strike_database/test_inmemory_strike_database.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/wallet/core/browser/walletable_pass_client.h"
#include "components/wallet/core/browser/walletable_pass_ingestion_controller_test_api.h"
#include "components/wallet/core/browser/walletable_permission_utils.h"
#include "components/wallet/core/common/wallet_features.h"
#include "components/wallet/core/common/wallet_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::test::EqualsProto;
using optimization_guide::ModelBasedCapabilityKey::kWalletablePassExtraction;
using optimization_guide::OptimizationGuideDecision::kFalse;
using optimization_guide::OptimizationGuideDecision::kTrue;
using optimization_guide::proto::WALLETABLE_PASS_DETECTION_LOYALTY_ALLOWLIST;
using testing::_;
using testing::Eq;
using testing::Return;
using testing::WithArgs;
using enum optimization_guide::proto::PassCategory;

namespace wallet {
namespace {

class MockWalletablePassClient : public WalletablePassClient {
 public:
  MOCK_METHOD(optimization_guide::OptimizationGuideDecider*,
              GetOptimizationGuideDecider,
              (),
              (override));
  MOCK_METHOD(optimization_guide::RemoteModelExecutor*,
              GetRemoteModelExecutor,
              (),
              (override));
  MOCK_METHOD(
      void,
      ShowWalletablePassConsentBubble,
      (optimization_guide::proto::PassCategory pass_category,
       WalletablePassClient::WalletablePassBubbleResultCallback callback),
      (override));
  MOCK_METHOD(
      void,
      ShowWalletablePassSaveBubble,
      (WalletablePass pass,
       WalletablePassClient::WalletablePassBubbleResultCallback callback),
      (override));
  MOCK_METHOD(strike_database::StrikeDatabaseBase*,
              GetStrikeDatabase,
              (),
              (override));
  MOCK_METHOD(PrefService*, GetPrefService, (), (override));
  MOCK_METHOD(signin::IdentityManager*, GetIdentityManager, (), (override));
  MOCK_METHOD(GeoIpCountryCode, GetGeoIpCountryCode, (), (override));
};

// Mock implementation of WalletablePassIngestionController that provides mocks
// for the pure virtual methods.
class MockWalletablePassIngestionController
    : public WalletablePassIngestionController {
 public:
  explicit MockWalletablePassIngestionController(WalletablePassClient* client)
      : WalletablePassIngestionController(client) {}

  MOCK_METHOD(std::string, GetPageTitle, (), (const, override));
  MOCK_METHOD(void,
              GetAnnotatedPageContent,
              (AnnotatedPageContentCallback),
              (override));
};

class WalletablePassIngestionControllerTest : public testing::Test {
 protected:
  WalletablePassIngestionControllerTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kWalletablePassDetection,
        {{"walletable_supported_country_allowlist", "US"}});
    wallet::prefs::RegisterProfilePrefs(test_pref_service().registry());
    ON_CALL(mock_client_, GetOptimizationGuideDecider())
        .WillByDefault(Return(&mock_decider_));
    ON_CALL(mock_client_, GetRemoteModelExecutor())
        .WillByDefault(Return(&mock_model_executor_));
    ON_CALL(mock_client_, GetStrikeDatabase())
        .WillByDefault(Return(&test_strike_database_));
    ON_CALL(mock_client_, GetPrefService())
        .WillByDefault(Return(&test_pref_service()));
    ON_CALL(mock_client_, GetIdentityManager())
        .WillByDefault(Return(test_identity_environment().identity_manager()));
    ON_CALL(mock_client_, GetGeoIpCountryCode())
        .WillByDefault(Return(GeoIpCountryCode("US")));
    controller_ =
        std::make_unique<MockWalletablePassIngestionController>(&mock_client_);
  }

  MockWalletablePassIngestionController* controller() {
    return controller_.get();
  }
  optimization_guide::MockOptimizationGuideDecider& mock_decider() {
    return mock_decider_;
  }
  optimization_guide::MockRemoteModelExecutor& mock_model_executor() {
    return mock_model_executor_;
  }
  MockWalletablePassClient& mock_client() { return mock_client_; }

  strike_database::TestInMemoryStrikeDatabase& test_strike_database() {
    return test_strike_database_;
  }

  sync_preferences::TestingPrefServiceSyncable& test_pref_service() {
    return test_pref_service_;
  }

  signin::IdentityTestEnvironment& test_identity_environment() {
    return test_identity_environment_;
  }

  WalletablePass CreateLoyaltyCard(
      const std::string& member_id = "test_member_id") {
    WalletablePass walletable_pass;
    LoyaltyCard loyalty_card;
    loyalty_card.member_id = member_id;
    walletable_pass.pass_data = std::move(loyalty_card);
    return walletable_pass;
  }

  void ExpectSaveBubbleOnClient(
      const WalletablePass& expected_pass,
      WalletablePassClient::WalletablePassBubbleResultCallback* out_callback) {
    EXPECT_CALL(mock_client(),
                ShowWalletablePassSaveBubble(Eq(expected_pass), _))
        .WillOnce(WithArgs<1>(
            [out_callback](
                WalletablePassClient::WalletablePassBubbleResultCallback
                    callback) { *out_callback = std::move(callback); }));
  }

  void ExpectConsentBubbleOnClient(
      optimization_guide::proto::PassCategory expected_category,
      WalletablePassClient::WalletablePassBubbleResultCallback* out_callback) {
    EXPECT_CALL(mock_client(),
                ShowWalletablePassConsentBubble(Eq(expected_category), _))
        .WillOnce(WithArgs<1>(
            [out_callback](
                WalletablePassClient::WalletablePassBubbleResultCallback
                    callback) { *out_callback = std::move(callback); }));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<optimization_guide::MockOptimizationGuideDecider>
      mock_decider_;
  testing::NiceMock<optimization_guide::MockRemoteModelExecutor>
      mock_model_executor_;
  strike_database::TestInMemoryStrikeDatabase test_strike_database_;
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  signin::IdentityTestEnvironment test_identity_environment_;
  testing::NiceMock<MockWalletablePassClient> mock_client_;

  std::unique_ptr<MockWalletablePassIngestionController> controller_;
};

TEST_F(WalletablePassIngestionControllerTest,
       GetPassCategoryForURL_NonHttpUrl_NotEligible) {
  GURL file_url("file:///test.html");
  EXPECT_EQ(test_api(controller()).GetPassCategoryForURL(file_url),
            std::nullopt);

  GURL ftp_url("ftp://example.com");
  EXPECT_EQ(test_api(controller()).GetPassCategoryForURL(ftp_url),
            std::nullopt);
}

TEST_F(WalletablePassIngestionControllerTest,
       GetPassCategoryForURL_AllowlistedUrl) {
  GURL https_url("https://example.com");
  EXPECT_CALL(
      mock_decider(),
      CanApplyOptimization(
          https_url, WALLETABLE_PASS_DETECTION_LOYALTY_ALLOWLIST, nullptr))
      .WillOnce(Return(kTrue));

  EXPECT_EQ(test_api(controller()).GetPassCategoryForURL(https_url),
            PASS_CATEGORY_LOYALTY_CARD);
}

TEST_F(WalletablePassIngestionControllerTest,
       GetPassCategoryForURL_NotAllowlistedUrl) {
  GURL http_url("http://example.com");
  EXPECT_CALL(
      mock_decider(),
      CanApplyOptimization(
          http_url, WALLETABLE_PASS_DETECTION_LOYALTY_ALLOWLIST, nullptr))
      .WillOnce(Return(kFalse));

  EXPECT_EQ(test_api(controller()).GetPassCategoryForURL(http_url),
            std::nullopt);
}

TEST_F(WalletablePassIngestionControllerTest,
       ExtractWalletablePass_CallsModelExecutor) {
  GURL url("https://example.com");
  optimization_guide::proto::AnnotatedPageContent content;
  content.set_tab_id(123);

  optimization_guide::proto::WalletablePassExtractionRequest expected_request;
  expected_request.set_pass_category(PASS_CATEGORY_LOYALTY_CARD);
  expected_request.mutable_page_context()->set_url(url.spec());
  expected_request.mutable_page_context()->set_title("title");
  *expected_request.mutable_page_context()->mutable_annotated_page_content() =
      content;

  EXPECT_CALL(*controller(), GetPageTitle()).WillOnce(Return("title"));
  EXPECT_CALL(mock_model_executor(),
              ExecuteModel(kWalletablePassExtraction,
                           EqualsProto(expected_request), _, _));

  test_api(controller())
      .ExtractWalletablePass(url, PASS_CATEGORY_LOYALTY_CARD, content);
}

TEST_F(WalletablePassIngestionControllerTest,
       StartWalletablePassDetectionFlow_Eligible) {
  test_identity_environment().MakePrimaryAccountAvailable(
      "test@gmail.com", signin::ConsentLevel::kSignin);
  GURL url("https://example.com");
  EXPECT_CALL(mock_decider(),
              CanApplyOptimization(
                  url, WALLETABLE_PASS_DETECTION_LOYALTY_ALLOWLIST, nullptr))
      .WillOnce(Return(kTrue));

  // Expect ShowWalletablePassConsentBubble to be called.
  WalletablePassClient::WalletablePassBubbleResultCallback consent_callback;
  ExpectConsentBubbleOnClient(PASS_CATEGORY_LOYALTY_CARD, &consent_callback);

  test_api(controller()).StartWalletablePassDetectionFlow(url);
  ASSERT_TRUE(consent_callback);

  // Expect GetAnnotatedPageContent to be called, and simulate a successful
  // response.
  EXPECT_CALL(*controller(), GetAnnotatedPageContent)
      .WillOnce(WithArgs<0>(
          [](MockWalletablePassIngestionController::AnnotatedPageContentCallback
                 callback) {
            optimization_guide::proto::AnnotatedPageContent content;
            std::move(callback).Run(std::move(content));
          }));

  // Expect that the model executor is called when the content is retrieved.
  EXPECT_CALL(*controller(), GetPageTitle).WillOnce(Return("title"));
  EXPECT_CALL(mock_model_executor(),
              ExecuteModel(kWalletablePassExtraction, _, _, _));

  // Simulate accepting the consent bubble.
  std::move(consent_callback)
      .Run(WalletablePassClient::WalletablePassBubbleResult::kAccepted);
}

TEST_F(WalletablePassIngestionControllerTest,
       StartWalletablePassDetectionFlow_Eligible_OptedIn) {
  test_identity_environment().MakePrimaryAccountAvailable(
      "test@gmail.com", signin::ConsentLevel::kSignin);
  GURL url("https://example.com");
  EXPECT_CALL(mock_decider(),
              CanApplyOptimization(
                  url, WALLETABLE_PASS_DETECTION_LOYALTY_ALLOWLIST, nullptr))
      .WillOnce(Return(kTrue));

  // Set OptIn status to true.
  SetWalletablePassDetectionOptInStatus(
      &test_pref_service(), test_identity_environment().identity_manager(),
      true);

  // Expect GetAnnotatedPageContent to be called directly.
  EXPECT_CALL(*controller(), GetAnnotatedPageContent)
      .WillOnce(WithArgs<0>(
          [](MockWalletablePassIngestionController::AnnotatedPageContentCallback
                 callback) {
            optimization_guide::proto::AnnotatedPageContent content;
            std::move(callback).Run(std::move(content));
          }));

  // Expect ShowWalletablePassConsentBubble NOT to be called.
  EXPECT_CALL(mock_client(), ShowWalletablePassConsentBubble).Times(0);

  // Expect model executor call.
  EXPECT_CALL(*controller(), GetPageTitle).WillOnce(Return("title"));
  EXPECT_CALL(mock_model_executor(),
              ExecuteModel(kWalletablePassExtraction, _, _, _));

  test_api(controller()).StartWalletablePassDetectionFlow(url);
}
TEST_F(WalletablePassIngestionControllerTest,
       StartWalletablePassDetectionFlow_NotEligible_UrlNotAllowlisted) {
  test_identity_environment().MakePrimaryAccountAvailable(
      "test@gmail.com", signin::ConsentLevel::kSignin);
  GURL url("https://example.com");
  EXPECT_CALL(mock_decider(),
              CanApplyOptimization(
                  url, WALLETABLE_PASS_DETECTION_LOYALTY_ALLOWLIST, nullptr))
      .WillOnce(Return(kFalse));

  EXPECT_CALL(*controller(), GetAnnotatedPageContent).Times(0);
  test_api(controller()).StartWalletablePassDetectionFlow(url);
}

TEST_F(WalletablePassIngestionControllerTest,
       StartWalletablePassDetectionFlow_NotEligible_NotAllowedCountryCode) {
  test_identity_environment().MakePrimaryAccountAvailable(
      "test@gmail.com", signin::ConsentLevel::kSignin);
  GURL url("https://example.com");

  // Set country code to something not in allowlist (allowlist is US).
  EXPECT_CALL(mock_client(), GetGeoIpCountryCode)
      .WillRepeatedly(Return(GeoIpCountryCode("CA")));

  // Expect no consent bubble and no page content extraction.
  EXPECT_CALL(mock_client(), ShowWalletablePassConsentBubble).Times(0);
  EXPECT_CALL(*controller(), GetAnnotatedPageContent).Times(0);

  test_api(controller()).StartWalletablePassDetectionFlow(url);
}

TEST_F(WalletablePassIngestionControllerTest,
       StartWalletablePassDetectionFlow_NotEligible_NotSignedIn) {
  GURL url("https://example.com");

  EXPECT_CALL(mock_client(), ShowWalletablePassConsentBubble).Times(0);
  EXPECT_CALL(*controller(), GetAnnotatedPageContent).Times(0);

  test_api(controller()).StartWalletablePassDetectionFlow(url);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowConsentBubble_Accepted_GetsPageContent) {
  test_identity_environment().MakePrimaryAccountAvailable(
      "test@gmail.com", signin::ConsentLevel::kSignin);
  GURL url("https://example.com");

  // Expect ShowWalletablePassConsentBubble to be called.
  WalletablePassClient::WalletablePassBubbleResultCallback consent_callback;
  ExpectConsentBubbleOnClient(PASS_CATEGORY_LOYALTY_CARD, &consent_callback);

  test_api(controller()).ShowConsentBubble(url, PASS_CATEGORY_LOYALTY_CARD);
  ASSERT_TRUE(consent_callback);

  // Expect GetAnnotatedPageContent to be called when consent is accepted.
  EXPECT_CALL(*controller(), GetAnnotatedPageContent);

  // Simulate accepting the consent bubble.
  std::move(consent_callback)
      .Run(WalletablePassClient::WalletablePassBubbleResult::kAccepted);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowConsentBubble_StrikesExceed_BubbleNotShown) {
  GURL url("https://example.com");
  test_strike_database().SetStrikeData("WalletablePassConsent__shared_id", 2);

  EXPECT_CALL(mock_client(), ShowWalletablePassConsentBubble(_, _)).Times(0);

  test_api(controller()).ShowConsentBubble(url, PASS_CATEGORY_LOYALTY_CARD);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowConsentBubble_Accepted_ClearsStrikes) {
  GURL url("https://example.com");
  test_strike_database().SetStrikeData("WalletablePassConsent__shared_id", 1);

  WalletablePassClient::WalletablePassBubbleResultCallback consent_callback;
  ExpectConsentBubbleOnClient(PASS_CATEGORY_LOYALTY_CARD, &consent_callback);

  test_api(controller()).ShowConsentBubble(url, PASS_CATEGORY_LOYALTY_CARD);
  ASSERT_TRUE(consent_callback);

  EXPECT_CALL(*controller(), GetAnnotatedPageContent(_));
  std::move(consent_callback)
      .Run(WalletablePassClient::WalletablePassBubbleResult::kAccepted);

  EXPECT_EQ(
      test_strike_database().GetStrikes("WalletablePassConsent__shared_id"), 0);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowConsentBubble_Declined_AddsMaxStrikes) {
  GURL url("https://example.com");
  test_strike_database().SetStrikeData("WalletablePassConsent__shared_id", 0);

  WalletablePassClient::WalletablePassBubbleResultCallback consent_callback;
  ExpectConsentBubbleOnClient(PASS_CATEGORY_LOYALTY_CARD, &consent_callback);

  test_api(controller()).ShowConsentBubble(url, PASS_CATEGORY_LOYALTY_CARD);
  ASSERT_TRUE(consent_callback);

  std::move(consent_callback)
      .Run(WalletablePassClient::WalletablePassBubbleResult::kDeclined);

  EXPECT_EQ(
      test_strike_database().GetStrikes("WalletablePassConsent__shared_id"), 2);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowConsentBubble_Closed_AddsMaxStrikes) {
  GURL url("https://example.com");
  test_strike_database().SetStrikeData("WalletablePassConsent__shared_id", 0);

  WalletablePassClient::WalletablePassBubbleResultCallback consent_callback;
  ExpectConsentBubbleOnClient(PASS_CATEGORY_LOYALTY_CARD, &consent_callback);

  test_api(controller()).ShowConsentBubble(url, PASS_CATEGORY_LOYALTY_CARD);
  ASSERT_TRUE(consent_callback);

  std::move(consent_callback)
      .Run(WalletablePassClient::WalletablePassBubbleResult::kClosed);

  EXPECT_EQ(
      test_strike_database().GetStrikes("WalletablePassConsent__shared_id"), 2);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowConsentBubble_LostFocus_AddsOneStrike) {
  GURL url("https://example.com");
  test_strike_database().SetStrikeData("WalletablePassConsent__shared_id", 0);

  WalletablePassClient::WalletablePassBubbleResultCallback consent_callback;
  ExpectConsentBubbleOnClient(PASS_CATEGORY_LOYALTY_CARD, &consent_callback);

  test_api(controller()).ShowConsentBubble(url, PASS_CATEGORY_LOYALTY_CARD);
  ASSERT_TRUE(consent_callback);

  std::move(consent_callback)
      .Run(WalletablePassClient::WalletablePassBubbleResult::kLostFocus);

  EXPECT_EQ(
      test_strike_database().GetStrikes("WalletablePassConsent__shared_id"), 1);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowConsentBubble_Unknown_AddsOneStrike) {
  GURL url("https://example.com");
  test_strike_database().SetStrikeData("WalletablePassConsent__shared_id", 0);

  WalletablePassClient::WalletablePassBubbleResultCallback consent_callback;
  ExpectConsentBubbleOnClient(PASS_CATEGORY_LOYALTY_CARD, &consent_callback);

  test_api(controller()).ShowConsentBubble(url, PASS_CATEGORY_LOYALTY_CARD);
  ASSERT_TRUE(consent_callback);

  std::move(consent_callback)
      .Run(WalletablePassClient::WalletablePassBubbleResult::kUnknown);

  EXPECT_EQ(
      test_strike_database().GetStrikes("WalletablePassConsent__shared_id"), 1);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowConsentBubble_Declined_NoAction) {
  GURL url("https://example.com");

  // Expect ShowWalletablePassConsentBubble to be called.
  WalletablePassClient::WalletablePassBubbleResultCallback consent_callback;
  ExpectConsentBubbleOnClient(PASS_CATEGORY_LOYALTY_CARD, &consent_callback);

  test_api(controller()).ShowConsentBubble(url, PASS_CATEGORY_LOYALTY_CARD);
  ASSERT_TRUE(consent_callback);

  // Expect GetAnnotatedPageContent NOT to be called when consent is declined.
  EXPECT_CALL(*controller(), GetAnnotatedPageContent).Times(0);

  // Simulate declining the consent bubble.
  std::move(consent_callback)
      .Run(WalletablePassClient::WalletablePassBubbleResult::kDeclined);
}

TEST_F(WalletablePassIngestionControllerTest,
       MaybeStartExtraction_StrikesExceed_ExtractionNotStarted) {
  GURL url("https://example.com");
  test_strike_database().SetStrikeData(
      "WalletablePassSaveByHost__LoyaltyCard;example.com", 3);

  EXPECT_CALL(*controller(), GetAnnotatedPageContent).Times(0);

  test_api(controller()).MaybeStartExtraction(url, PASS_CATEGORY_LOYALTY_CARD);
}

TEST_F(WalletablePassIngestionControllerTest,
       MaybeStartExtraction_NoStrikes_ExtractionStarted) {
  GURL url("https://example.com");
  EXPECT_CALL(*controller(), GetAnnotatedPageContent);
  test_api(controller()).MaybeStartExtraction(url, PASS_CATEGORY_LOYALTY_CARD);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowSaveBubble_Accept_ClearsStrikes) {
  GURL url("https://example.com");
  WalletablePass walletable_pass = CreateLoyaltyCard();
  test_strike_database().SetStrikeData(
      "WalletablePassSaveByHost__LoyaltyCard;example.com", 2);

  WalletablePassClient::WalletablePassBubbleResultCallback bubble_callback;
  ExpectSaveBubbleOnClient(walletable_pass, &bubble_callback);

  test_api(controller()).ShowSaveBubble(url, walletable_pass);

  // Simulate accepting the bubble.
  std::move(bubble_callback)
      .Run(WalletablePassClient::WalletablePassBubbleResult::kAccepted);

  // Verify strikes are cleared.
  EXPECT_EQ(test_strike_database().GetStrikes(
                "WalletablePassSaveByHost__LoyaltyCard;example.com"),
            0);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowSaveBubble_Reject_AddsStrikes) {
  GURL url("https://example.com");
  WalletablePass walletable_pass = CreateLoyaltyCard();
  test_strike_database().SetStrikeData(
      "WalletablePassSaveByHost__LoyaltyCard;example.com", 1);

  WalletablePassClient::WalletablePassBubbleResultCallback bubble_callback;
  ExpectSaveBubbleOnClient(walletable_pass, &bubble_callback);

  test_api(controller()).ShowSaveBubble(url, walletable_pass);

  // Simulate declining the bubble.
  std::move(bubble_callback)
      .Run(WalletablePassClient::WalletablePassBubbleResult::kDeclined);

  // Verify strikes are added.
  EXPECT_EQ(test_strike_database().GetStrikes(
                "WalletablePassSaveByHost__LoyaltyCard;example.com"),
            2);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowSaveBubble_UnintendedClose_StrikesUnchanged) {
  GURL url("https://example.com");
  WalletablePass walletable_pass = CreateLoyaltyCard();
  test_strike_database().SetStrikeData(
      "WalletablePassSaveByHost__LoyaltyCard;example.com", 1);

  WalletablePassClient::WalletablePassBubbleResultCallback bubble_callback;
  ExpectSaveBubbleOnClient(walletable_pass, &bubble_callback);

  test_api(controller()).ShowSaveBubble(url, walletable_pass);

  // Simulate lost focus.
  std::move(bubble_callback)
      .Run(WalletablePassClient::WalletablePassBubbleResult::kLostFocus);

  // Verify strikes are the same.
  EXPECT_EQ(test_strike_database().GetStrikes(
                "WalletablePassSaveByHost__LoyaltyCard;example.com"),
            1);
}

}  // namespace
}  // namespace wallet
