// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/walletable_pass_service.h"

#include "components/optimization_guide/core/hints/mock_optimization_guide_decider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::Eq;
using testing::Matcher;
using testing::Return;

namespace wallet {
namespace {

class WalletablePassServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    service_ = std::make_unique<WalletablePassService>(&mock_decider_);
  }

  testing::NiceMock<optimization_guide::MockOptimizationGuideDecider>
      mock_decider_;
  std::unique_ptr<WalletablePassService> service_;
};

TEST_F(WalletablePassServiceTest,
       Constructor_RegistersOptimizationTypes_Success) {
  EXPECT_CALL(
      mock_decider_,
      RegisterOptimizationTypes(testing::ElementsAre(
          optimization_guide::proto::WALLETABLE_PASS_DETECTION_ALLOWLIST)));

  service_ = std::make_unique<WalletablePassService>(&mock_decider_);
}

TEST_F(WalletablePassServiceTest,
       IsEligibleForExtraction_NonHttpUrl_NotEligible) {
  GURL file_url("file:///test.html");
  EXPECT_FALSE(service_->IsEligibleForExtraction(file_url));

  GURL ftp_url("ftp://example.com");
  EXPECT_FALSE(service_->IsEligibleForExtraction(ftp_url));
}

TEST_F(WalletablePassServiceTest, IsEligibleForExtraction_AllowlistedUrl) {
  GURL https_url("https://example.com");
  EXPECT_CALL(
      mock_decider_,
      CanApplyOptimization(
          Eq(https_url),
          Eq(optimization_guide::proto::WALLETABLE_PASS_DETECTION_ALLOWLIST),
          Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .WillOnce(Return(optimization_guide::OptimizationGuideDecision::kTrue));

  EXPECT_TRUE(service_->IsEligibleForExtraction(https_url));
}

TEST_F(WalletablePassServiceTest, IsEligibleForExtraction_NotAllowlistedUrl) {
  GURL http_url("http://example.com");
  EXPECT_CALL(
      mock_decider_,
      CanApplyOptimization(
          Eq(http_url),
          Eq(optimization_guide::proto::WALLETABLE_PASS_DETECTION_ALLOWLIST),
          Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .WillOnce(Return(optimization_guide::OptimizationGuideDecision::kFalse));

  EXPECT_FALSE(service_->IsEligibleForExtraction(http_url));
}

}  // namespace
}  // namespace wallet
