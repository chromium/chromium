// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_manager.h"

#include "base/functional/callback.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {

class MockFacilitatedPaymentsDriver : public FacilitatedPaymentsDriver {
 public:
  explicit MockFacilitatedPaymentsDriver(
      std::unique_ptr<FacilitatedPaymentsManager> manager)
      : FacilitatedPaymentsDriver(std::move(manager)) {}
  ~MockFacilitatedPaymentsDriver() override = default;

  MOCK_METHOD(void,
              TriggerPixCodeDetection,
              (base::OnceCallback<void(bool)>),
              (const override));
};

class MockOptimizationGuideDecider
    : public optimization_guide::OptimizationGuideDecider {
 public:
  MOCK_METHOD(void,
              RegisterOptimizationTypes,
              (const std::vector<optimization_guide::proto::OptimizationType>&),
              (override));
  MOCK_METHOD(void,
              CanApplyOptimization,
              (const GURL&,
               optimization_guide::proto::OptimizationType,
               optimization_guide::OptimizationGuideDecisionCallback),
              (override));
  MOCK_METHOD(optimization_guide::OptimizationGuideDecision,
              CanApplyOptimization,
              (const GURL&,
               optimization_guide::proto::OptimizationType,
               optimization_guide::OptimizationMetadata*),
              (override));
  MOCK_METHOD(
      void,
      CanApplyOptimizationOnDemand,
      (const std::vector<GURL>&,
       const base::flat_set<optimization_guide::proto::OptimizationType>&,
       optimization_guide::proto::RequestContext,
       optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback,
       optimization_guide::proto::RequestContextMetadata*
           request_context_metadata),
      (override));
};

class FacilitatedPaymentsManagerTest : public testing::Test {
 public:
  FacilitatedPaymentsManagerTest() {
    optimization_guide_decider_ =
        std::make_unique<MockOptimizationGuideDecider>();
    driver_ = std::make_unique<MockFacilitatedPaymentsDriver>(nullptr);
    manager_ = std::make_unique<FacilitatedPaymentsManager>(
        driver_.get(), optimization_guide_decider_.get());
  }

 protected:
  std::unique_ptr<MockFacilitatedPaymentsDriver> driver_;
  std::unique_ptr<MockOptimizationGuideDecider> optimization_guide_decider_;
  std::unique_ptr<FacilitatedPaymentsManager> manager_;
};

// Test that the `PIX_PAYMENT_MERCHANT_ALLOWLIST` optimization type is
// registered when RegisterPixOptimizationGuide is called.
TEST_F(FacilitatedPaymentsManagerTest, TestRegisterPixOptimizationGuide) {
  EXPECT_CALL(*optimization_guide_decider_,
              RegisterOptimizationTypes(testing::ElementsAre(
                  optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST)))
      .Times(1);

  manager_->RegisterPixOptimizationGuide();
}

// Test that `ShouldDetectPixCode` returns true for merchant websites not in the
// allowlist.
TEST_F(FacilitatedPaymentsManagerTest, TestShouldDetectPixCode_UrlInAllowlist) {
  GURL url("https://example.com/");
  ON_CALL(*optimization_guide_decider_,
          CanApplyOptimization(
              testing::Eq(url),
              testing::Eq(
                  optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST),
              testing::Matcher<optimization_guide::OptimizationMetadata*>(
                  testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));

  EXPECT_TRUE(manager_->ShouldDetectPixCode(url));
}

// Test that `ShouldDetectPixCode` returns false for merchant websites not in
// the allowlist.
TEST_F(FacilitatedPaymentsManagerTest,
       TestShouldDetectPixCode_UrlNotInAllowlist) {
  GURL url("https://example.com/");
  ON_CALL(*optimization_guide_decider_,
          CanApplyOptimization(
              testing::Eq(url),
              testing::Eq(
                  optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST),
              testing::Matcher<optimization_guide::OptimizationMetadata*>(
                  testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kFalse));

  EXPECT_FALSE(manager_->ShouldDetectPixCode(url));
}

// Test that PIX code detection is triggered for webpages in the allowlist.
TEST_F(FacilitatedPaymentsManagerTest,
       TestDidFinishLoad_UrlInAllowlist_PixCodeDetectionTriggered) {
  GURL url("https://example.com/");
  ON_CALL(*optimization_guide_decider_,
          CanApplyOptimization(
              testing::Eq(url),
              testing::Eq(
                  optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST),
              testing::Matcher<optimization_guide::OptimizationMetadata*>(
                  testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));

  EXPECT_CALL(*driver_, TriggerPixCodeDetection).Times(1);
  manager_->DidFinishLoad(url);
}

// Test that PIX code detection is not triggered for webpages not in the
// allowlist.
TEST_F(FacilitatedPaymentsManagerTest,
       TestDidFinishLoad_UrlNotInAllowlist_PixCodeDetectionNotTriggered) {
  GURL url("https://example.com/");
  ON_CALL(*optimization_guide_decider_,
          CanApplyOptimization(
              testing::Eq(url),
              testing::Eq(
                  optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST),
              testing::Matcher<optimization_guide::OptimizationMetadata*>(
                  testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kFalse));

  EXPECT_CALL(*driver_, TriggerPixCodeDetection).Times(0);
  manager_->DidFinishLoad(url);
}

}  // namespace payments::facilitated
