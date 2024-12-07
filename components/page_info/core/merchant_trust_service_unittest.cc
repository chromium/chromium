// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/merchant_trust_service.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/commerce/core/proto/merchant_trust.pb.h"
#include "components/optimization_guide/core/mock_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/page_info/core/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_info {
using testing::_;
using testing::An;
using testing::Invoke;
using testing::Return;

using DecisionWithMetadata = MerchantTrustService::DecisionAndMetadata;
using optimization_guide::AnyWrapProto;
using optimization_guide::MockOptimizationGuideDecider;
using optimization_guide::OptimizationGuideDecision;
using optimization_guide::OptimizationGuideDecisionCallback;
using optimization_guide::OptimizationMetadata;
using optimization_guide::proto::OptimizationType;

namespace {
const char kTestSummary[] = "This is a test summary.";

commerce::MerchantTrustSignalsV2 CreateValidProto() {
  commerce::MerchantTrustSignalsV2 proto;
  proto.set_merchant_star_rating(3.8);
  proto.set_merchant_count_rating(45);
  proto.set_merchant_details_page_url("https://page_url.com");
  proto.set_reviews_summary(kTestSummary);
  return proto;
}

OptimizationMetadata BuildMerchantTrustResponse() {
  OptimizationMetadata meta;
  meta.set_any_metadata(AnyWrapProto(CreateValidProto()));
  return meta;
}

}  // namespace

class MockMerchantTrustService : public MerchantTrustService {
 public:
  explicit MockMerchantTrustService(
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
      : MerchantTrustService(optimization_guide_decider,
                             /*is_off_the_record=*/false,
                             nullptr) {}

  MOCK_METHOD(bool, IsOptimizationGuideAllowed, (), (const, override));
};

class MerchantTrustServiceTest : public ::testing::Test {
 public:
  void SetUp() override {
    service_ = std::make_unique<MockMerchantTrustService>(&opt_guide());
    SetOptimizationGuideAllowed(true);
    SetResponse(GURL("https://foo.com"), OptimizationGuideDecision::kUnknown,
                BuildMerchantTrustResponse());
  }

  // Setup optimization guide to return the given decision and metadata for the
  // given URL and optimization type.
  void SetResponse(const GURL& url,
                   OptimizationGuideDecision decision,
                   const OptimizationMetadata& metadata) {
    ON_CALL(
        opt_guide(),
        CanApplyOptimization(
            _, _, An<optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillByDefault(
            Invoke([decision, metadata](
                       const GURL& url, OptimizationType optimization_type,
                       optimization_guide::OptimizationGuideDecisionCallback
                           callback) {
              std::move(callback).Run(decision, metadata);
            }));
  }

  void SetOptimizationGuideAllowed(bool allowed) {
    EXPECT_CALL(*service(), IsOptimizationGuideAllowed())
        .WillRepeatedly(Return(allowed));
  }

  MockMerchantTrustService* service() { return service_.get(); }
  optimization_guide::MockOptimizationGuideDecider& opt_guide() {
    return opt_guide_;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::NiceMock<optimization_guide::MockOptimizationGuideDecider>
      opt_guide_;
  std::unique_ptr<MockMerchantTrustService> service_;
};

// Tests that proto are returned correctly when optimization guide decision is
// true
TEST_F(MerchantTrustServiceTest, OptimizationGuideDecisionTrue) {
  SetResponse(GURL("https://foo.com"), OptimizationGuideDecision::kTrue,
              BuildMerchantTrustResponse());

  base::RunLoop run_loop;
  service()->GetMerchantTrustInfo(
      GURL("https://foo.com"),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             std::optional<page_info::MerchantData> info) {
            ASSERT_TRUE(info.has_value());
            ASSERT_EQ(info->page_url, GURL("https://page_url.com"));
            ASSERT_EQ(info->reviews_summary, kTestSummary);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

// Tests that proto are not returned correctly when optimization guide decision
// is unknown
TEST_F(MerchantTrustServiceTest, OptimizationGuideDecisionUnknown) {
  SetResponse(GURL("https://foo.com"), OptimizationGuideDecision::kUnknown,
              BuildMerchantTrustResponse());

  base::RunLoop run_loop;
  service()->GetMerchantTrustInfo(
      GURL("https://foo.com"),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             std::optional<page_info::MerchantData> info) {
            ASSERT_FALSE(info.has_value());
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

// Tests with optimization guide not allowed
TEST_F(MerchantTrustServiceTest, NoOptimizationGuideNotAllowed) {
  SetOptimizationGuideAllowed(false);
  base::RunLoop run_loop;
  service()->GetMerchantTrustInfo(
      GURL("https://foo.com"),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             std::optional<page_info::MerchantData> info) {
            ASSERT_FALSE(info.has_value());
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

// Tests that sample data is returned when optimization guide decision is
// unknown and sample data is enabled.
TEST_F(MerchantTrustServiceTest, SampleData) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      kMerchantTrust, {{kMerchantTrustEnabledWithSampleData.name, "true"}});

  SetResponse(GURL("https://foo.com"), OptimizationGuideDecision::kUnknown,
              BuildMerchantTrustResponse());

  base::RunLoop run_loop;
  service()->GetMerchantTrustInfo(
      GURL("https://foo.com"),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             std::optional<page_info::MerchantData> info) {
            ASSERT_TRUE(info.has_value());
            ASSERT_EQ(info->page_url,
                      GURL("https://customerreviews.google.com/v/"
                           "merchant?q=amazon.com&c=AE&v=19"));
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

}  // namespace page_info
