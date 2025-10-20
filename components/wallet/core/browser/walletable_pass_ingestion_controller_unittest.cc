// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/walletable_pass_ingestion_controller.h"

#include <memory>

#include "base/test/protobuf_matchers.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/hints/mock_optimization_guide_decider.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/wallet/core/browser/walletable_pass_client.h"
#include "components/wallet/core/browser/walletable_pass_ingestion_controller_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::test::EqualsProto;
using optimization_guide::ModelBasedCapabilityKey::kWalletablePassExtraction;
using optimization_guide::OptimizationGuideDecision::kFalse;
using optimization_guide::OptimizationGuideDecision::kTrue;
using optimization_guide::proto::WALLETABLE_PASS_DETECTION_ALLOWLIST;
using testing::_;
using testing::Return;
using testing::WithArgs;

namespace wallet {
namespace {

class MockWalletablePassClient : public WalletablePassClient {
 public:
  MOCK_METHOD(optimization_guide::OptimizationGuideDecider*,
              GetOptimizationGuideDecider,
              (),
              (override));
  MOCK_METHOD(optimization_guide::OptimizationGuideModelExecutor*,
              GetOptimizationGuideModelExecutor,
              (),
              (override));
  MOCK_METHOD(
      void,
      ShowWalletablePassConsentBubble,
      (WalletablePassClient::WalletablePassBubbleResultCallback callback),
      (override));
  MOCK_METHOD(
      void,
      ShowWalletablePassSaveBubble,
      (const optimization_guide::proto::WalletablePass& pass,
       WalletablePassClient::WalletablePassBubbleResultCallback callback),
      (override));
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
    ON_CALL(mock_client_, GetOptimizationGuideDecider())
        .WillByDefault(Return(&mock_decider_));
    ON_CALL(mock_client_, GetOptimizationGuideModelExecutor())
        .WillByDefault(Return(&mock_model_executor_));
    controller_ =
        std::make_unique<MockWalletablePassIngestionController>(&mock_client_);
  }

  MockWalletablePassIngestionController* controller() {
    return controller_.get();
  }
  optimization_guide::MockOptimizationGuideDecider& mock_decider() {
    return mock_decider_;
  }
  optimization_guide::MockOptimizationGuideModelExecutor&
  mock_model_executor() {
    return mock_model_executor_;
  }
  MockWalletablePassClient& mock_client() { return mock_client_; }

 private:
  testing::NiceMock<optimization_guide::MockOptimizationGuideDecider>
      mock_decider_;
  testing::NiceMock<optimization_guide::MockOptimizationGuideModelExecutor>
      mock_model_executor_;
  testing::NiceMock<MockWalletablePassClient> mock_client_;

  std::unique_ptr<MockWalletablePassIngestionController> controller_;
};

TEST_F(WalletablePassIngestionControllerTest,
       IsEligibleForExtraction_NonHttpUrl_NotEligible) {
  GURL file_url("file:///test.html");
  EXPECT_FALSE(test_api(controller()).IsEligibleForExtraction(file_url));

  GURL ftp_url("ftp://example.com");
  EXPECT_FALSE(test_api(controller()).IsEligibleForExtraction(ftp_url));
}

TEST_F(WalletablePassIngestionControllerTest,
       IsEligibleForExtraction_AllowlistedUrl) {
  GURL https_url("https://example.com");
  EXPECT_CALL(mock_decider(),
              CanApplyOptimization(
                  https_url, WALLETABLE_PASS_DETECTION_ALLOWLIST, nullptr))
      .WillOnce(Return(kTrue));

  EXPECT_TRUE(test_api(controller()).IsEligibleForExtraction(https_url));
}

TEST_F(WalletablePassIngestionControllerTest,
       IsEligibleForExtraction_NotAllowlistedUrl) {
  GURL http_url("http://example.com");
  EXPECT_CALL(mock_decider(),
              CanApplyOptimization(
                  http_url, WALLETABLE_PASS_DETECTION_ALLOWLIST, nullptr))
      .WillOnce(Return(kFalse));

  EXPECT_FALSE(test_api(controller()).IsEligibleForExtraction(http_url));
}

TEST_F(WalletablePassIngestionControllerTest,
       ExtractWalletablePass_CallsModelExecutor) {
  GURL url("https://example.com");
  optimization_guide::proto::AnnotatedPageContent content;
  content.set_tab_id(123);

  EXPECT_CALL(*controller(), GetPageTitle()).WillOnce(Return("title"));
  EXPECT_CALL(mock_model_executor(),
              ExecuteModel(kWalletablePassExtraction, _, _, _));

  test_api(controller()).ExtractWalletablePass(url, content);
}

TEST_F(WalletablePassIngestionControllerTest,
       StartWalletablePassDetectionFlow_NotEligible) {
  GURL url("https://example.com");
  EXPECT_CALL(
      mock_decider(),
      CanApplyOptimization(url, WALLETABLE_PASS_DETECTION_ALLOWLIST, nullptr))
      .WillOnce(Return(kFalse));

  EXPECT_CALL(*controller(), GetAnnotatedPageContent(_)).Times(0);
  test_api(controller()).StartWalletablePassDetectionFlow(url);
}

TEST_F(WalletablePassIngestionControllerTest,
       StartWalletablePassDetectionFlow_Eligible) {
  GURL url("https://example.com");
  EXPECT_CALL(
      mock_decider(),
      CanApplyOptimization(url, WALLETABLE_PASS_DETECTION_ALLOWLIST, nullptr))
      .WillOnce(Return(kTrue));

  // Expect GetAnnotatedPageContent to be called, and simulate a successful
  // response.
  EXPECT_CALL(*controller(), GetAnnotatedPageContent(_))
      .WillOnce(WithArgs<0>(
          [](MockWalletablePassIngestionController::AnnotatedPageContentCallback
                 callback) {
            optimization_guide::proto::AnnotatedPageContent content;
            std::move(callback).Run(std::move(content));
          }));

  // Expect that the model executor is called when the content is retrieved.
  EXPECT_CALL(*controller(), GetPageTitle()).WillOnce(Return("title"));
  EXPECT_CALL(mock_model_executor(),
              ExecuteModel(kWalletablePassExtraction, _, _, _));

  test_api(controller()).StartWalletablePassDetectionFlow(url);
}

TEST_F(WalletablePassIngestionControllerTest,
       ShowSaveBubble_ModelExecutionSucceeds_BubbleShown) {
  GURL url("https://example.com");
  optimization_guide::proto::AnnotatedPageContent content;
  content.set_tab_id(123);

  optimization_guide::proto::WalletablePass walletable_pass;
  walletable_pass.mutable_loyalty_card()->set_member_id("test_pass_id");

  EXPECT_CALL(*controller(), GetPageTitle()).WillOnce(Return("title"));

  EXPECT_CALL(mock_model_executor(),
              ExecuteModel(kWalletablePassExtraction, _, _, _))
      .WillOnce(WithArgs<3>(
          [&walletable_pass](
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            // Create the response object
            optimization_guide::proto::WalletablePassExtractionResponse
                response;
            *response.add_walletable_pass() = walletable_pass;
            auto execution_result =
                optimization_guide::OptimizationGuideModelExecutionResult(
                    optimization_guide::AnyWrapProto(response),
                    /*execution_info=*/nullptr);

            std::move(callback).Run(std::move(execution_result),
                                    /*log_entry=*/nullptr);
          }));

  EXPECT_CALL(mock_client(),
              ShowWalletablePassSaveBubble(EqualsProto(walletable_pass), _));

  test_api(controller()).ExtractWalletablePass(url, content);
}

}  // namespace
}  // namespace wallet
