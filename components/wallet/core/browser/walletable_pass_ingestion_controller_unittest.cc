// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/walletable_pass_ingestion_controller.h"

#include "components/optimization_guide/core/hints/mock_optimization_guide_decider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::Eq;
using testing::Matcher;
using testing::Return;

namespace wallet {
namespace {

class WalletablePassIngestionControllerTest : public testing::Test {
 public:
  WalletablePassIngestionControllerTest() : controller_(&mock_decider_) {}

 protected:
  testing::NiceMock<optimization_guide::MockOptimizationGuideDecider>
      mock_decider_;

  WalletablePassIngestionController controller_;
};

TEST_F(WalletablePassIngestionControllerTest,
       IsEligibleForExtraction_NonHttpUrl_NotEligible) {
  GURL file_url("file:///test.html");
  EXPECT_FALSE(controller_.IsEligibleForExtraction(file_url));

  GURL ftp_url("ftp://example.com");
  EXPECT_FALSE(controller_.IsEligibleForExtraction(ftp_url));
}

TEST_F(WalletablePassIngestionControllerTest,
       IsEligibleForExtraction_AllowlistedUrl) {
  GURL https_url("https://example.com");
  EXPECT_CALL(
      mock_decider_,
      CanApplyOptimization(
          Eq(https_url),
          Eq(optimization_guide::proto::WALLETABLE_PASS_DETECTION_ALLOWLIST),
          Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .WillOnce(Return(optimization_guide::OptimizationGuideDecision::kTrue));

  EXPECT_TRUE(controller_.IsEligibleForExtraction(https_url));
}

TEST_F(WalletablePassIngestionControllerTest,
       IsEligibleForExtraction_NotAllowlistedUrl) {
  GURL http_url("http://example.com");
  EXPECT_CALL(
      mock_decider_,
      CanApplyOptimization(
          Eq(http_url),
          Eq(optimization_guide::proto::WALLETABLE_PASS_DETECTION_ALLOWLIST),
          Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .WillOnce(Return(optimization_guide::OptimizationGuideDecision::kFalse));

  EXPECT_FALSE(controller_.IsEligibleForExtraction(http_url));
}

}  // namespace
}  // namespace wallet
