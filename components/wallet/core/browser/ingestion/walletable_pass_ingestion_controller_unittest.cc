// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/ingestion/walletable_pass_ingestion_controller.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
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
#include "components/wallet/core/browser/data_models/data_model_utils.h"
#include "components/wallet/core/browser/metrics/wallet_metrics.h"
#include "components/wallet/core/browser/ingestion/walletable_pass_client.h"
#include "components/wallet/core/browser/ingestion/walletable_pass_ingestion_controller_test_api.h"
#include "components/wallet/core/browser/walletable_permission_utils.h"
#include "components/wallet/core/common/wallet_features.h"
#include "components/wallet/core/common/wallet_prefs.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::test::EqualsProto;
using optimization_guide::ModelBasedCapabilityKey::kWalletablePassExtraction;
using optimization_guide::OptimizationGuideDecision::kFalse;
using optimization_guide::OptimizationGuideDecision::kTrue;
using optimization_guide::proto::
    WALLETABLE_PASS_DETECTION_BOARDING_PASS_ALLOWLIST;
using optimization_guide::proto::WALLETABLE_PASS_DETECTION_EVENT_PASS_ALLOWLIST;
using optimization_guide::proto::WALLETABLE_PASS_DETECTION_LOYALTY_ALLOWLIST;
using optimization_guide::proto::
    WALLETABLE_PASS_DETECTION_TRANSIT_TICKET_ALLOWLIST;
using testing::_;
using testing::Eq;
using testing::Return;
using testing::WithArgs;

namespace wallet {
namespace {

class MockWalletHttpClient : public WalletHttpClient {
 public:
  MockWalletHttpClient() = default;
  MOCK_METHOD(void,
              UpsertPublicPass,
              (Pass pass, WalletHttpClient::UpsertPublicPassCallback callback),
              (override));
  MOCK_METHOD(void,
              UpsertPrivatePass,
              (PrivatePass pass,
               WalletHttpClient::UpsertPrivatePassCallback callback),
              (override));
  MOCK_METHOD(void,
              GetUnmaskedPass,
              (std::string_view pass_id,
               WalletHttpClient::GetUnmaskedPassCallback callback),
              (override));
};

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
      (PassCategory pass_category,
       WalletablePassClient::WalletablePassBubbleResultCallback callback),
      (override));
  MOCK_METHOD(
      void,
      ShowWalletablePassSaveBubble,
      (WalletPass pass,
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
  MOCK_METHOD(void, DetectBarcodes, (BarcodeDetectionCallback), (override));
};

class WalletablePassIngestionControllerTest : public testing::Test {
 protected:
  WalletablePassIngestionControllerTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kWalletablePassDetection,
          {{"walletable_supported_country_allowlist", "US"}}},
         {features::kWalletablePassSave, {}}},
        {});
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

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  WalletPass CreateLoyaltyCard(
      const std::string& member_id = "test_member_id") {
    WalletPass walletable_pass;
    LoyaltyCard loyalty_card;
    loyalty_card.member_id = member_id;
    walletable_pass.pass_data = std::move(loyalty_card);
    return walletable_pass;
  }

  void ExpectSaveBubbleOnClient(
      const WalletPass& expected_pass,
      WalletablePassClient::WalletablePassBubbleResultCallback* out_callback) {
    EXPECT_CALL(mock_client(),
                ShowWalletablePassSaveBubble(Eq(expected_pass), _))
        .WillOnce(WithArgs<1>(
            [out_callback](
                WalletablePassClient::WalletablePassBubbleResultCallback
                    callback) { *out_callback = std::move(callback); }));
  }

  void ExpectConsentBubbleOnClient(
      PassCategory expected_category,
      WalletablePassClient::WalletablePassBubbleResultCallback* out_callback) {
    EXPECT_CALL(mock_client(),
                ShowWalletablePassConsentBubble(Eq(expected_category), _))
        .WillOnce(WithArgs<1>(
            [out_callback](
                WalletablePassClient::WalletablePassBubbleResultCallback
                    callback) { *out_callback = std::move(callback); }));
  }

  void ExpectAndRunExtractWalletablePass(
      PassCategory pass_category,
      base::OnceCallback<void(
          optimization_guide::OptimizationGuideModelExecutionResultCallback)>
          model_executor_action,
      metrics::WalletablePassServerExtractionFunnelEvents expected_event) {
    GURL url("https://example.com");
    optimization_guide::proto::AnnotatedPageContent content;

    EXPECT_CALL(*controller(), GetPageTitle()).WillRepeatedly(Return("title"));

    EXPECT_CALL(mock_model_executor(),
                ExecuteModel(kWalletablePassExtraction, _, _, _))
        .WillOnce(WithArgs<3>(
            [model_executor_action = std::move(model_executor_action)](
                optimization_guide::
                    OptimizationGuideModelExecutionResultCallback
                        callback) mutable {
              std::move(model_executor_action).Run(std::move(callback));
            }));
    test_api(controller()).ExtractWalletablePass(url, pass_category, content);
    histogram_tester_.ExpectUniqueSample(
        base::StrCat({"Wallet.WalletablePass.ServerExtraction.Funnel.",
                      PassCategoryToString(pass_category)}),
        expected_event, 1);
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
  base::HistogramTester histogram_tester_;

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
       GetPassCategoryForURL_LoyaltyCardAllowlistedUrl) {
  GURL https_url("https://example.com");
  EXPECT_CALL(
      mock_decider(),
      CanApplyOptimization(
          https_url, WALLETABLE_PASS_DETECTION_LOYALTY_ALLOWLIST, nullptr))
      .WillOnce(Return(kTrue));

  EXPECT_EQ(test_api(controller()).GetPassCategoryForURL(https_url),
            PassCategory::kLoyaltyCard);
}

TEST_F(WalletablePassIngestionControllerTest,
       GetPassCategoryForURL_BoardingPassAllowlistedUrl) {
  GURL https_url("https://example.com");
  EXPECT_CALL(
      mock_decider(),
      CanApplyOptimization(
          https_url, WALLETABLE_PASS_DETECTION_LOYALTY_ALLOWLIST, nullptr))
      .WillOnce(Return(kFalse));
  EXPECT_CALL(mock_decider(),
              CanApplyOptimization(
                  https_url, WALLETABLE_PASS_DETECTION_BOARDING_PASS_ALLOWLIST,
                  nullptr))
      .WillOnce(Return(kTrue));

  EXPECT_EQ(test_api(controller()).GetPassCategoryForURL(https_url),
            PassCategory::kBoardingPass);
}

TEST_F(WalletablePassIngestionControllerTest,
       GetPassCategoryForURL_EventPassAllowlistedUrl) {
  GURL https_url("https://example.com");
  EXPECT_CALL(
      mock_decider(),
      CanApplyOptimization(
          https_url, WALLETABLE_PASS_DETECTION_LOYALTY_ALLOWLIST, nullptr))
      .WillOnce(Return(kFalse));
  EXPECT_CALL(mock_decider(),
              CanApplyOptimization(
                  https_url, WALLETABLE_PASS_DETECTION_BOARDING_PASS_ALLOWLIST,
                  nullptr))
      .WillOnce(Return(kFalse));
  EXPECT_CALL(
      mock_decider(),
      CanApplyOptimization(
          https_url, WALLETABLE_PASS_DETECTION_EVENT_PASS_ALLOWLIST, nullptr))
      .WillOnce(Return(kTrue));

  EXPECT_EQ(test_api(controller()).GetPassCategoryForURL(https_url),
            PassCategory::kEventPass);
}

TEST_F(WalletablePassIngestionControllerTest,
       GetPassCategoryForURL_TransitTicketAllowlistedUrl) {
  GURL https_url("https://example.com");
  EXPECT_CALL(
      mock_decider(),
      CanApplyOptimization(
          https_url, WALLETABLE_PASS_DETECTION_LOYALTY_ALLOWLIST, nullptr))
      .WillOnce(Return(kFalse));
  EXPECT_CALL(mock_decider(),
              CanApplyOptimization(
                  https_url, WALLETABLE_PASS_DETECTION_BOARDING_PASS_ALLOWLIST,
                  nullptr))
      .WillOnce(Return(kFalse));
  EXPECT_CALL(
      mock_decider(),
      CanApplyOptimization(
          https_url, WALLETABLE_PASS_DETECTION_EVENT_PASS_ALLOWLIST, nullptr))
      .WillOnce(Return(kFalse));
  EXPECT_CALL(mock_decider(),
              CanApplyOptimization(
                  https_url, WALLETABLE_PASS_DETECTION_TRANSIT_TICKET_ALLOWLIST,
                  nullptr))
      .WillOnce(Return(kTrue));

  EXPECT_EQ(test_api(controller()).GetPassCategoryForURL(https_url),
            PassCategory::kTransitTicket);
}

TEST_F(WalletablePassIngestionControllerTest,
       GetPassCategoryForURL_NotAllowlistedUrl) {
  GURL http_url("http://example.com");
  EXPECT_CALL(
      mock_decider(),
      CanApplyOptimization(
          http_url, WALLETABLE_PASS_DETECTION_LOYALTY_ALLOWLIST, nullptr))
      .WillOnce(Return(kFalse));
  EXPECT_CALL(
      mock_decider(),
      CanApplyOptimization(
          http_url, WALLETABLE_PASS_DETECTION_BOARDING_PASS_ALLOWLIST, nullptr))
      .WillOnce(Return(kFalse));
  EXPECT_CALL(
      mock_decider(),
      CanApplyOptimization(
          http_url, WALLETABLE_PASS_DETECTION_EVENT_PASS_ALLOWLIST, nullptr))
      .WillOnce(Return(kFalse));
  EXPECT_CALL(mock_decider(),
              CanApplyOptimization(
                  http_url, WALLETABLE_PASS_DETECTION_TRANSIT_TICKET_ALLOWLIST,
                  nullptr))
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
  expected_request.set_pass_category(
      optimization_guide::proto::PASS_CATEGORY_LOYALTY_CARD);
  expected_request.mutable_page_context()->set_url(url.spec());
  expected_request.mutable_page_context()->set_title("title");
  *expected_request.mutable_page_context()->mutable_annotated_page_content() =
      content;

  EXPECT_CALL(*controller(), GetPageTitle()).WillOnce(Return("title"));
  EXPECT_CALL(mock_model_executor(),
              ExecuteModel(kWalletablePassExtraction,
                           EqualsProto(expected_request), _, _));

  test_api(controller())
      .ExtractWalletablePass(url, PassCategory::kLoyaltyCard, content);
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
  ExpectConsentBubbleOnClient(PassCategory::kLoyaltyCard, &consent_callback);

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

  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.OptIn.Funnel.LoyaltyCard",
      metrics::WalletablePassOptInFunnelEvents::kConsentBubbleWasShown, 1);
  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.OptIn.Funnel.LoyaltyCard",
      metrics::WalletablePassOptInFunnelEvents::kConsentBubbleWasAccepted, 1);
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
      GeoIpCountryCode("US"), true);

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

  histogram_tester().ExpectUniqueSample(
      "Wallet.WalletablePass.OptIn.Funnel.LoyaltyCard",
      metrics::WalletablePassOptInFunnelEvents::kUserAlreadyOptedIn, 1);
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
  EXPECT_CALL(
      mock_decider(),
      CanApplyOptimization(
          url, WALLETABLE_PASS_DETECTION_BOARDING_PASS_ALLOWLIST, nullptr))
      .WillOnce(Return(kFalse));
  EXPECT_CALL(mock_decider(),
              CanApplyOptimization(
                  url, WALLETABLE_PASS_DETECTION_EVENT_PASS_ALLOWLIST, nullptr))
      .WillOnce(Return(kFalse));
  EXPECT_CALL(
      mock_decider(),
      CanApplyOptimization(
          url, WALLETABLE_PASS_DETECTION_TRANSIT_TICKET_ALLOWLIST, nullptr))
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
       StartWalletablePassDetectionFlow_Eligible_GetAnnotatedPageContentFails) {
  test_identity_environment().MakePrimaryAccountAvailable(
      "test@gmail.com", signin::ConsentLevel::kSignin);
  GURL url("https://example.com");
  EXPECT_CALL(mock_decider(),
              CanApplyOptimization(
                  url, WALLETABLE_PASS_DETECTION_LOYALTY_ALLOWLIST, nullptr))
      .WillOnce(Return(kTrue));

  // Expect ShowWalletablePassConsentBubble to be called.
  WalletablePassClient::WalletablePassBubbleResultCallback consent_callback;
  ExpectConsentBubbleOnClient(PassCategory::kLoyaltyCard, &consent_callback);

  test_api(controller()).StartWalletablePassDetectionFlow(url);
  ASSERT_TRUE(consent_callback);

  // Expect GetAnnotatedPageContent to be called, and simulate a failure
  // response.
  EXPECT_CALL(*controller(), GetAnnotatedPageContent)
      .WillOnce(WithArgs<0>(
          [](MockWalletablePassIngestionController::AnnotatedPageContentCallback
                 callback) { std::move(callback).Run(std::nullopt); }));

  // Expect that the model executor is NOT called.
  EXPECT_CALL(mock_model_executor(), ExecuteModel).Times(0);

  // Simulate accepting the consent bubble.
  std::move(consent_callback)
      .Run(WalletablePassClient::WalletablePassBubbleResult::kAccepted);

  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.OptIn.Funnel.LoyaltyCard",
      metrics::WalletablePassOptInFunnelEvents::kConsentBubbleWasShown, 1);
  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.OptIn.Funnel.LoyaltyCard",
      metrics::WalletablePassOptInFunnelEvents::kConsentBubbleWasAccepted, 1);
  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.ServerExtraction.Funnel.LoyaltyCard",
      metrics::WalletablePassServerExtractionFunnelEvents::
          kGetAnnotatedPageContentFailed,
      1);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowConsentBubble_Accepted_GetsPageContent) {
  test_identity_environment().MakePrimaryAccountAvailable(
      "test@gmail.com", signin::ConsentLevel::kSignin);
  GURL url("https://example.com");

  // Expect ShowWalletablePassConsentBubble to be called.
  WalletablePassClient::WalletablePassBubbleResultCallback consent_callback;
  ExpectConsentBubbleOnClient(PassCategory::kLoyaltyCard, &consent_callback);

  test_api(controller()).ShowConsentBubble(url, PassCategory::kLoyaltyCard);
  ASSERT_TRUE(consent_callback);

  // Expect GetAnnotatedPageContent to be called when consent is accepted.
  EXPECT_CALL(*controller(), GetAnnotatedPageContent);

  // Simulate accepting the consent bubble.
  std::move(consent_callback)
      .Run(WalletablePassClient::WalletablePassBubbleResult::kAccepted);

  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.OptIn.Funnel.LoyaltyCard",
      metrics::WalletablePassOptInFunnelEvents::kConsentBubbleWasShown, 1);
  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.OptIn.Funnel.LoyaltyCard",
      metrics::WalletablePassOptInFunnelEvents::kConsentBubbleWasAccepted, 1);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowConsentBubble_StrikesExceed_BubbleNotShown) {
  GURL url("https://example.com");
  test_strike_database().SetStrikeData("WalletablePassConsent__shared_id", 2);

  EXPECT_CALL(mock_client(), ShowWalletablePassConsentBubble(_, _)).Times(0);

  test_api(controller()).ShowConsentBubble(url, PassCategory::kLoyaltyCard);

  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.OptIn.Funnel.LoyaltyCard",
      metrics::WalletablePassOptInFunnelEvents::
          kConsentBubbleWasBlockedByStrike,
      1);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowConsentBubble_Accepted_ClearsStrikes) {
  GURL url("https://example.com");
  test_strike_database().SetStrikeData("WalletablePassConsent__shared_id", 1);

  WalletablePassClient::WalletablePassBubbleResultCallback consent_callback;
  ExpectConsentBubbleOnClient(PassCategory::kLoyaltyCard, &consent_callback);

  test_api(controller()).ShowConsentBubble(url, PassCategory::kLoyaltyCard);
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
  ExpectConsentBubbleOnClient(PassCategory::kLoyaltyCard, &consent_callback);

  test_api(controller()).ShowConsentBubble(url, PassCategory::kLoyaltyCard);
  ASSERT_TRUE(consent_callback);

  std::move(consent_callback)
      .Run(WalletablePassClient::WalletablePassBubbleResult::kDeclined);

  EXPECT_EQ(
      test_strike_database().GetStrikes("WalletablePassConsent__shared_id"), 2);

  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.OptIn.Funnel.LoyaltyCard",
      metrics::WalletablePassOptInFunnelEvents::kConsentBubbleWasShown, 1);
  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.OptIn.Funnel.LoyaltyCard",
      metrics::WalletablePassOptInFunnelEvents::kConsentBubbleWasRejected, 1);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowConsentBubble_Closed_AddsMaxStrikes) {
  GURL url("https://example.com");
  test_strike_database().SetStrikeData("WalletablePassConsent__shared_id", 0);

  WalletablePassClient::WalletablePassBubbleResultCallback consent_callback;
  ExpectConsentBubbleOnClient(PassCategory::kLoyaltyCard, &consent_callback);

  test_api(controller()).ShowConsentBubble(url, PassCategory::kLoyaltyCard);
  ASSERT_TRUE(consent_callback);

  std::move(consent_callback)
      .Run(WalletablePassClient::WalletablePassBubbleResult::kClosed);

  EXPECT_EQ(
      test_strike_database().GetStrikes("WalletablePassConsent__shared_id"), 2);

  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.OptIn.Funnel.LoyaltyCard",
      metrics::WalletablePassOptInFunnelEvents::kConsentBubbleWasShown, 1);
  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.OptIn.Funnel.LoyaltyCard",
      metrics::WalletablePassOptInFunnelEvents::kConsentBubbleWasClosed, 1);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowConsentBubble_LostFocus_AddsOneStrike) {
  GURL url("https://example.com");
  test_strike_database().SetStrikeData("WalletablePassConsent__shared_id", 0);

  WalletablePassClient::WalletablePassBubbleResultCallback consent_callback;
  ExpectConsentBubbleOnClient(PassCategory::kLoyaltyCard, &consent_callback);

  test_api(controller()).ShowConsentBubble(url, PassCategory::kLoyaltyCard);
  ASSERT_TRUE(consent_callback);

  std::move(consent_callback)
      .Run(WalletablePassClient::WalletablePassBubbleResult::kLostFocus);

  EXPECT_EQ(
      test_strike_database().GetStrikes("WalletablePassConsent__shared_id"), 1);

  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.OptIn.Funnel.LoyaltyCard",
      metrics::WalletablePassOptInFunnelEvents::kConsentBubbleWasShown, 1);
  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.OptIn.Funnel.LoyaltyCard",
      metrics::WalletablePassOptInFunnelEvents::kConsentBubbleLostFocus, 1);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowConsentBubble_Unknown_AddsOneStrike) {
  GURL url("https://example.com");
  test_strike_database().SetStrikeData("WalletablePassConsent__shared_id", 0);

  WalletablePassClient::WalletablePassBubbleResultCallback consent_callback;
  ExpectConsentBubbleOnClient(PassCategory::kLoyaltyCard, &consent_callback);

  test_api(controller()).ShowConsentBubble(url, PassCategory::kLoyaltyCard);
  ASSERT_TRUE(consent_callback);

  std::move(consent_callback)
      .Run(WalletablePassClient::WalletablePassBubbleResult::kUnknown);

  EXPECT_EQ(
      test_strike_database().GetStrikes("WalletablePassConsent__shared_id"), 1);

  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.OptIn.Funnel.LoyaltyCard",
      metrics::WalletablePassOptInFunnelEvents::kConsentBubbleWasShown, 1);
  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.OptIn.Funnel.LoyaltyCard",
      metrics::WalletablePassOptInFunnelEvents::
          kConsentBubbleClosedUnknownReason,
      1);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowConsentBubble_Declined_NoAction) {
  GURL url("https://example.com");

  // Expect ShowWalletablePassConsentBubble to be called.
  WalletablePassClient::WalletablePassBubbleResultCallback consent_callback;
  ExpectConsentBubbleOnClient(PassCategory::kLoyaltyCard, &consent_callback);

  test_api(controller()).ShowConsentBubble(url, PassCategory::kLoyaltyCard);
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

  test_api(controller()).MaybeStartExtraction(url, PassCategory::kLoyaltyCard);

  histogram_tester().ExpectUniqueSample(
      "Wallet.WalletablePass.ServerExtraction.Funnel.LoyaltyCard",
      metrics::WalletablePassServerExtractionFunnelEvents::
          kExtractionBlockedBySaveStrike,
      1);
}

TEST_F(WalletablePassIngestionControllerTest,
       MaybeStartExtraction_NoStrikes_ExtractionStarted) {
  GURL url("https://example.com");
  EXPECT_CALL(*controller(), GetAnnotatedPageContent);
  test_api(controller()).MaybeStartExtraction(url, PassCategory::kLoyaltyCard);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowSaveBubble_Accept_ClearsStrikes) {
  GURL url("https://example.com");
  WalletPass walletable_pass = CreateLoyaltyCard();
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
  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.Save.Funnel.LoyaltyCard",
      metrics::WalletablePassSaveFunnelEvents::kSaveBubbleWasShown, 1);
  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.Save.Funnel.LoyaltyCard",
      metrics::WalletablePassSaveFunnelEvents::kSaveBubbleWasAccepted, 1);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowSaveBubble_Reject_AddsStrikes) {
  GURL url("https://example.com");
  WalletPass walletable_pass = CreateLoyaltyCard();
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
  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.Save.Funnel.LoyaltyCard",
      metrics::WalletablePassSaveFunnelEvents::kSaveBubbleWasShown, 1);
  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.Save.Funnel.LoyaltyCard",
      metrics::WalletablePassSaveFunnelEvents::kSaveBubbleWasRejected, 1);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowSaveBubble_UnintendedClose_StrikesUnchanged) {
  GURL url("https://example.com");
  WalletPass walletable_pass = CreateLoyaltyCard();
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
  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.Save.Funnel.LoyaltyCard",
      metrics::WalletablePassSaveFunnelEvents::kSaveBubbleWasShown, 1);
  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.Save.Funnel.LoyaltyCard",
      metrics::WalletablePassSaveFunnelEvents::kSaveBubbleLostFocus, 1);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowSaveBubble_Closed_AddsStrikes) {
  GURL url("https://example.com");
  WalletPass walletable_pass = CreateLoyaltyCard();
  test_strike_database().SetStrikeData(
      "WalletablePassSaveByHost__LoyaltyCard;example.com", 1);

  WalletablePassClient::WalletablePassBubbleResultCallback bubble_callback;
  ExpectSaveBubbleOnClient(walletable_pass, &bubble_callback);

  test_api(controller()).ShowSaveBubble(url, walletable_pass);

  // Simulate closing the bubble.
  std::move(bubble_callback)
      .Run(WalletablePassClient::WalletablePassBubbleResult::kClosed);

  // Verify strikes are added.
  EXPECT_EQ(test_strike_database().GetStrikes(
                "WalletablePassSaveByHost__LoyaltyCard;example.com"),
            2);
  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.Save.Funnel.LoyaltyCard",
      metrics::WalletablePassSaveFunnelEvents::kSaveBubbleWasShown, 1);
  histogram_tester().ExpectBucketCount(
      "Wallet.WalletablePass.Save.Funnel.LoyaltyCard",
      metrics::WalletablePassSaveFunnelEvents::kSaveBubbleWasClosed, 1);
}

TEST_F(WalletablePassIngestionControllerTest,
       ExtractWalletablePass_ModelExecutionFailed) {
  ExpectAndRunExtractWalletablePass(
      PassCategory::kLoyaltyCard,
      base::BindOnce([](optimization_guide::
                            OptimizationGuideModelExecutionResultCallback
                                callback) {
        std::move(callback).Run(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::unexpected(
                    optimization_guide::OptimizationGuideModelExecutionError::
                        FromModelExecutionError(
                            optimization_guide::
                                OptimizationGuideModelExecutionError::
                                    ModelExecutionError::kGenericFailure)),
                nullptr),
            nullptr);
      }),
      metrics::WalletablePassServerExtractionFunnelEvents::
          kModelExecutionFailed);
}

TEST_F(WalletablePassIngestionControllerTest,
       ExtractWalletablePass_ResponseCannotBeParsed) {
  ExpectAndRunExtractWalletablePass(
      PassCategory::kLoyaltyCard,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::proto::Any any;
            any.set_type_url("type.googleapis.com/somerandomtype");
            any.set_value("garbage");
            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    std::move(any), nullptr),
                nullptr);
          }),
      metrics::WalletablePassServerExtractionFunnelEvents::
          kResponseCannotBeParsed);
}

TEST_F(WalletablePassIngestionControllerTest,
       ExtractWalletablePass_NoPassExtracted) {
  ExpectAndRunExtractWalletablePass(
      PassCategory::kLoyaltyCard,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::proto::WalletablePassExtractionResponse
                response;
            optimization_guide::proto::Any any;
            any.set_type_url(
                "type.googleapis.com/"
                "optimization_guide.proto.WalletablePassExtractionResponse");
            response.SerializeToString(any.mutable_value());
            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    std::move(any), nullptr),
                nullptr);
          }),
      metrics::WalletablePassServerExtractionFunnelEvents::kNoPassExtracted);
}

TEST_F(WalletablePassIngestionControllerTest,
       ExtractWalletablePass_InvalidPassType) {
  ExpectAndRunExtractWalletablePass(
      PassCategory::kLoyaltyCard,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::proto::WalletablePassExtractionResponse
                response;
            response.add_walletable_pass();
            optimization_guide::proto::Any any;
            any.set_type_url(
                "type.googleapis.com/"
                "optimization_guide.proto.WalletablePassExtractionResponse");
            response.SerializeToString(any.mutable_value());
            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    std::move(any), nullptr),
                nullptr);
          }),
      metrics::WalletablePassServerExtractionFunnelEvents::kInvalidPassType);
}

TEST_F(WalletablePassIngestionControllerTest,
       ExtractWalletablePass_ExtractionSucceeded) {
  GURL url("https://example.com");
  optimization_guide::proto::AnnotatedPageContent content;

  EXPECT_CALL(*controller(), GetPageTitle()).WillRepeatedly(Return("title"));

  EXPECT_CALL(mock_model_executor(),
              ExecuteModel(kWalletablePassExtraction, _, _, _))
      .WillOnce(WithArgs<3>(
          [](optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::proto::WalletablePassExtractionResponse
                response;
            auto* pass = response.add_walletable_pass();
            pass->mutable_loyalty_card()->set_plan_name("Program Name");
            pass->mutable_loyalty_card()->set_issuer_name("Issuer Name");
            pass->mutable_loyalty_card()->set_member_id("Member ID");
            optimization_guide::proto::Any any;
            any.set_type_url(
                "type.googleapis.com/"
                "optimization_guide.proto.WalletablePassExtractionResponse");
            response.SerializeToString(any.mutable_value());
            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    std::move(any), nullptr),
                nullptr);
          }));
  WalletablePassClient::WalletablePassBubbleResultCallback save_bubble_callback;
  WalletPass expected_pass = CreateLoyaltyCard("Member ID");
  // Set additional fields to match the response from the model executor.
  auto& loyalty_card = std::get<LoyaltyCard>(expected_pass.pass_data);
  loyalty_card.plan_name = "Program Name";
  loyalty_card.issuer_name = "Issuer Name";

  ExpectSaveBubbleOnClient(expected_pass, &save_bubble_callback);
  test_api(controller())
      .ExtractWalletablePass(url, PassCategory::kLoyaltyCard, content);
  histogram_tester().ExpectUniqueSample(
      "Wallet.WalletablePass.ServerExtraction.Funnel.LoyaltyCard",
      metrics::WalletablePassServerExtractionFunnelEvents::kExtractionSucceeded,
      1);
}

TEST_F(WalletablePassIngestionControllerTest,
       MaybeStartExtraction_StaleCallback_Ignored) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");

  WalletablePassIngestionController::AnnotatedPageContentCallback callback1;
  WalletablePassIngestionController::AnnotatedPageContentCallback callback2;

  // We expect GetAnnotatedPageContent to be called twice.
  // Capture the callbacks.
  EXPECT_CALL(*controller(), GetAnnotatedPageContent(_))
      .WillOnce(
          [&callback1](
              WalletablePassIngestionController::AnnotatedPageContentCallback
                  cb) { callback1 = std::move(cb); })
      .WillOnce(
          [&callback2](
              WalletablePassIngestionController::AnnotatedPageContentCallback
                  cb) { callback2 = std::move(cb); });

  // We expect ExecuteModel to be called ONLY ONCE (for the second callback).
  // Verify that the request uses `url2`.
  optimization_guide::proto::WalletablePassExtractionRequest expected_request;
  expected_request.set_pass_category(
      optimization_guide::proto::PASS_CATEGORY_LOYALTY_CARD);
  expected_request.mutable_page_context()->set_url(url2.spec());
  expected_request.mutable_page_context()->set_title("title");
  // The callback provides an empty content, so we expect it in the request.
  *expected_request.mutable_page_context()->mutable_annotated_page_content() =
      optimization_guide::proto::AnnotatedPageContent();

  EXPECT_CALL(mock_model_executor(),
              ExecuteModel(kWalletablePassExtraction,
                           EqualsProto(expected_request), _, _));
  // GetPageTitle is called by ExtractWalletablePass.
  EXPECT_CALL(*controller(), GetPageTitle()).WillOnce(Return("title"));

  // Start 1st extraction
  test_api(controller()).MaybeStartExtraction(url1, PassCategory::kLoyaltyCard);

  // Start 2nd extraction (invalidates the first one)
  test_api(controller()).MaybeStartExtraction(url2, PassCategory::kLoyaltyCard);

  ASSERT_TRUE(callback1);
  ASSERT_TRUE(callback2);

  optimization_guide::proto::AnnotatedPageContent content;

  // Run stale callback 1. Should be ignored (no ExecuteModel).
  std::move(callback1).Run(content);

  // Run fresh callback 2. Should trigger ExecuteModel.
  std::move(callback2).Run(content);
}

TEST_F(WalletablePassIngestionControllerTest,
       MaybeStartExtraction_BoardingPass_DetectsBarcodesAndSaves) {
  GURL url("https://example.com");

  // Expect DetectBarcodes to be called.
  WalletablePassIngestionController::BarcodeDetectionCallback barcode_callback;
  EXPECT_CALL(*controller(), DetectBarcodes(_))
      .WillOnce(
          [&barcode_callback](
              WalletablePassIngestionController::BarcodeDetectionCallback cb) {
            barcode_callback = std::move(cb);
          });

  // Expect GetAnnotatedPageContent NOT to be called.
  EXPECT_CALL(*controller(), GetAnnotatedPageContent).Times(0);

  test_api(controller()).MaybeStartExtraction(url, PassCategory::kBoardingPass);

  ASSERT_TRUE(barcode_callback);

  // Prepare a barcode.
  WalletBarcode barcode;
  // Use a valid sample string for BCBP.
  barcode.raw_value =
      "M1DESMARAIS/LUC       EABC123 YYZORDAC 0834 326J001A0025 100";
  barcode.format = WalletBarcodeFormat::PDF417;

  // Expect ShowSaveBubble.
  EXPECT_CALL(mock_client(), ShowWalletablePassSaveBubble(_, _));

  std::vector<WalletBarcode> barcodes = {barcode};
  std::move(barcode_callback).Run(std::move(barcodes));
}

TEST_F(WalletablePassIngestionControllerTest,
       MaybeStartExtraction_LoyaltyCard_MergesBarcode) {
  GURL url("https://example.com");

  WalletablePassIngestionController::BarcodeDetectionCallback barcode_callback;
  WalletablePassIngestionController::AnnotatedPageContentCallback
      content_callback;

  EXPECT_CALL(*controller(), DetectBarcodes(_))
      .WillOnce(
          [&barcode_callback](
              WalletablePassIngestionController::BarcodeDetectionCallback cb) {
            barcode_callback = std::move(cb);
          });

  EXPECT_CALL(*controller(), GetAnnotatedPageContent(_))
      .WillOnce(
          [&content_callback](
              WalletablePassIngestionController::AnnotatedPageContentCallback
                  cb) { content_callback = std::move(cb); });

  test_api(controller()).MaybeStartExtraction(url, PassCategory::kLoyaltyCard);

  ASSERT_TRUE(barcode_callback);
  ASSERT_TRUE(content_callback);

  // Prepare barcode.
  WalletBarcode barcode;
  barcode.raw_value = "123456789";
  barcode.format = WalletBarcodeFormat::QR_CODE;
  std::vector<WalletBarcode> barcodes = {barcode};

  // Run barcode detection callback.
  std::move(barcode_callback).Run(std::move(barcodes));

  // Prepare content and execute model.
  optimization_guide::proto::AnnotatedPageContent content;
  content.set_tab_id(123);

  // Set up model response.
  optimization_guide::proto::WalletablePassExtractionResponse response;
  auto* pass = response.add_walletable_pass();
  pass->mutable_loyalty_card()->set_member_id("test_member_id");

  EXPECT_CALL(*controller(), GetPageTitle()).WillOnce(Return("title"));
  EXPECT_CALL(mock_model_executor(),
              ExecuteModel(kWalletablePassExtraction, _, _, _))
      .WillOnce(WithArgs<3>(
          [&response](
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    base::ok(optimization_guide::AnyWrapProto(response)),
                    nullptr),
                nullptr);
          }));

  // Expect ShowSaveBubble with merged barcode.
  WalletPass expected_pass = CreateLoyaltyCard("test_member_id");
  std::get<LoyaltyCard>(expected_pass.pass_data).barcode = barcode;

  WalletablePassClient::WalletablePassBubbleResultCallback bubble_callback;
  ExpectSaveBubbleOnClient(expected_pass, &bubble_callback);

  // Run content callback.
  std::move(content_callback).Run(content);
}

TEST_F(WalletablePassIngestionControllerTest,
       MaybeStartExtraction_LoyaltyCard_NoBarcode_SavesWithoutBarcode) {
  GURL url("https://example.com");

  WalletablePassIngestionController::BarcodeDetectionCallback barcode_callback;
  WalletablePassIngestionController::AnnotatedPageContentCallback
      content_callback;

  EXPECT_CALL(*controller(), DetectBarcodes(_))
      .WillOnce(
          [&barcode_callback](
              WalletablePassIngestionController::BarcodeDetectionCallback cb) {
            barcode_callback = std::move(cb);
          });

  EXPECT_CALL(*controller(), GetAnnotatedPageContent(_))
      .WillOnce(
          [&content_callback](
              WalletablePassIngestionController::AnnotatedPageContentCallback
                  cb) { content_callback = std::move(cb); });

  test_api(controller()).MaybeStartExtraction(url, PassCategory::kLoyaltyCard);

  // Return empty barcodes.
  std::move(barcode_callback).Run(std::vector<WalletBarcode>());

  // Prepare content and execute model.
  optimization_guide::proto::AnnotatedPageContent content;
  content.set_tab_id(123);

  // Set up model response.
  optimization_guide::proto::WalletablePassExtractionResponse response;
  auto* pass = response.add_walletable_pass();
  pass->mutable_loyalty_card()->set_member_id("test_member_id");

  EXPECT_CALL(*controller(), GetPageTitle()).WillOnce(Return("title"));
  EXPECT_CALL(mock_model_executor(),
              ExecuteModel(kWalletablePassExtraction, _, _, _))
      .WillOnce(WithArgs<3>(
          [&response](
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    base::ok(optimization_guide::AnyWrapProto(response)),
                    nullptr),
                nullptr);
          }));

  // Expect ShowSaveBubble with pass (no barcode).
  WalletPass expected_pass = CreateLoyaltyCard("test_member_id");
  // barcode remains empty/default.

  WalletablePassClient::WalletablePassBubbleResultCallback bubble_callback;
  ExpectSaveBubbleOnClient(expected_pass, &bubble_callback);

  // Run content callback.
  std::move(content_callback).Run(content);
}

TEST_F(WalletablePassIngestionControllerTest,
       MaybeStartExtraction_LoyaltyCard_ModelFails_NoSave) {
  GURL url("https://example.com");

  WalletablePassIngestionController::BarcodeDetectionCallback barcode_callback;
  WalletablePassIngestionController::AnnotatedPageContentCallback
      content_callback;

  EXPECT_CALL(*controller(), DetectBarcodes(_))
      .WillOnce(
          [&barcode_callback](
              WalletablePassIngestionController::BarcodeDetectionCallback cb) {
            barcode_callback = std::move(cb);
          });

  EXPECT_CALL(*controller(), GetAnnotatedPageContent(_))
      .WillOnce(
          [&content_callback](
              WalletablePassIngestionController::AnnotatedPageContentCallback
                  cb) { content_callback = std::move(cb); });

  test_api(controller()).MaybeStartExtraction(url, PassCategory::kLoyaltyCard);

  WalletBarcode barcode;
  barcode.raw_value = "123";
  std::move(barcode_callback).Run(std::vector<WalletBarcode>{barcode});

  // Prepare content.
  optimization_guide::proto::AnnotatedPageContent content;

  EXPECT_CALL(*controller(), GetPageTitle()).WillOnce(Return("title"));
  // Model execution fails (e.g. returns empty response or error).
  EXPECT_CALL(mock_model_executor(),
              ExecuteModel(kWalletablePassExtraction, _, _, _))
      .WillOnce(WithArgs<3>([](optimization_guide::
                                   OptimizationGuideModelExecutionResultCallback
                                       callback) {
        // Run with null response to simulate failure.
        std::move(callback).Run(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::unexpected(
                    optimization_guide::OptimizationGuideModelExecutionError::
                        FromModelExecutionError(
                            optimization_guide::
                                OptimizationGuideModelExecutionError::
                                    ModelExecutionError::kGenericFailure)),
                nullptr),
            nullptr);
      }));

  // Expect NO ShowSaveBubble.
  EXPECT_CALL(mock_client(), ShowWalletablePassSaveBubble(_, _)).Times(0);

  std::move(content_callback).Run(content);
}

TEST_F(WalletablePassIngestionControllerTest,
       MaybeStartExtraction_LoyaltyCard_AnnotationFails_NoSave) {
  GURL url("https://example.com");

  WalletablePassIngestionController::BarcodeDetectionCallback barcode_callback;
  WalletablePassIngestionController::AnnotatedPageContentCallback
      content_callback;

  EXPECT_CALL(*controller(), DetectBarcodes(_))
      .WillOnce(
          [&barcode_callback](
              WalletablePassIngestionController::BarcodeDetectionCallback cb) {
            barcode_callback = std::move(cb);
          });

  EXPECT_CALL(*controller(), GetAnnotatedPageContent(_))
      .WillOnce(
          [&content_callback](
              WalletablePassIngestionController::AnnotatedPageContentCallback
                  cb) { content_callback = std::move(cb); });

  test_api(controller()).MaybeStartExtraction(url, PassCategory::kLoyaltyCard);

  // Return some barcodes.
  WalletBarcode barcode;
  barcode.raw_value = "123";
  std::move(barcode_callback).Run(std::vector<WalletBarcode>{barcode});

  // Fail annotation.
  std::move(content_callback).Run(std::nullopt);

  // Expect NO ShowSaveBubble.
  EXPECT_CALL(mock_client(), ShowWalletablePassSaveBubble(_, _)).Times(0);
}

TEST_F(WalletablePassIngestionControllerTest,
       MaybeStartExtraction_StaleBarcodeCallback_Ignored) {
  GURL url("https://example.com");

  WalletablePassIngestionController::BarcodeDetectionCallback callback1;
  WalletablePassIngestionController::BarcodeDetectionCallback callback2;

  // We expect DetectBarcodes to be called twice.
  EXPECT_CALL(*controller(), DetectBarcodes(_))
      .WillOnce(
          [&callback1](
              WalletablePassIngestionController::BarcodeDetectionCallback cb) {
            callback1 = std::move(cb);
          })
      .WillOnce(
          [&callback2](
              WalletablePassIngestionController::BarcodeDetectionCallback cb) {
            callback2 = std::move(cb);
          });

  // Start 1st extraction
  test_api(controller()).MaybeStartExtraction(url, PassCategory::kBoardingPass);

  // Start 2nd extraction (invalidates the first one)
  test_api(controller()).MaybeStartExtraction(url, PassCategory::kBoardingPass);

  ASSERT_TRUE(callback1);
  ASSERT_TRUE(callback2);

  // Prepare a stale barcode.
  WalletBarcode barcode1;
  barcode1.raw_value = "stale_barcode";
  barcode1.format = WalletBarcodeFormat::PDF417;
  std::vector<WalletBarcode> barcodes1 = {barcode1};

  // Prepare a fresh barcode.
  WalletBarcode barcode2;
  barcode2.raw_value =
      "M1DESMARAIS/LUC       EABC123 YYZORDAC 0834 326J001A0025 100";
  barcode2.format = WalletBarcodeFormat::PDF417;
  std::vector<WalletBarcode> barcodes2 = {barcode2};

  // Expect ShowSaveBubble to be called ONLY ONCE (for the second callback),
  // and verify it contains barcode2.
  WalletablePassClient::WalletablePassBubbleResultCallback bubble_callback;
  EXPECT_CALL(mock_client(), ShowWalletablePassSaveBubble(_, _))
      .WillOnce(WithArgs<0, 1>(
          [&barcode2, &bubble_callback](
              WalletPass pass,
              WalletablePassClient::WalletablePassBubbleResultCallback
                  callback) {
            bubble_callback = std::move(callback);
            ASSERT_TRUE(std::holds_alternative<BoardingPass>(pass.pass_data));
            const auto& boarding_pass = std::get<BoardingPass>(pass.pass_data);
            ASSERT_TRUE(boarding_pass.barcode.has_value());
            EXPECT_EQ(boarding_pass.barcode->raw_value, barcode2.raw_value);
          }));

  // Run stale callback 1 with stale barcode. Should be ignored.
  std::move(callback1).Run(std::move(barcodes1));

  // Run fresh callback 2 with fresh barcode. Should trigger ShowSaveBubble.
  std::move(callback2).Run(std::move(barcodes2));
}

}  // namespace
}  // namespace wallet
