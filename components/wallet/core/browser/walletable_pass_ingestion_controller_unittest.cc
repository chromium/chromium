// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/walletable_pass_ingestion_controller.h"

#include <memory>

#include "components/optimization_guide/core/hints/mock_optimization_guide_decider.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/wallet/core/browser/walletable_pass_client.h"
#include "components/wallet/core/browser/walletable_pass_ingestion_controller_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using optimization_guide::ModelBasedCapabilityKey::kWalletablePassExtraction;
using optimization_guide::OptimizationGuideDecision::kFalse;
using optimization_guide::OptimizationGuideDecision::kTrue;
using optimization_guide::proto::WALLETABLE_PASS_DETECTION_ALLOWLIST;
using testing::_;
using testing::Return;

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
        std::make_unique<WalletablePassIngestionController>(&mock_client_);
  }

  WalletablePassIngestionController* controller() { return controller_.get(); }
  optimization_guide::MockOptimizationGuideDecider& mock_decider() {
    return mock_decider_;
  }
  optimization_guide::MockOptimizationGuideModelExecutor&
  mock_model_executor() {
    return mock_model_executor_;
  }

 private:
  testing::NiceMock<optimization_guide::MockOptimizationGuideDecider>
      mock_decider_;
  testing::NiceMock<optimization_guide::MockOptimizationGuideModelExecutor>
      mock_model_executor_;
  testing::NiceMock<MockWalletablePassClient> mock_client_;

  std::unique_ptr<WalletablePassIngestionController> controller_;
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
  std::string url = "https://example.com";
  optimization_guide::proto::AnnotatedPageContent content;
  content.set_tab_id(123);

  EXPECT_CALL(mock_model_executor(),
              ExecuteModel(kWalletablePassExtraction, _, _, _));

  test_api(controller()).ExtractWalletablePass(url, content);
}

}  // namespace
}  // namespace wallet
