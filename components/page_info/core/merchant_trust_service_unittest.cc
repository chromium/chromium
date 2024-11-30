// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/merchant_trust_service.h"

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/proto/merchant_trust_metadata.pb.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_info {
using testing::_;
using testing::Invoke;
using testing::Return;

using DecisionWithMetadata = MerchantTrustService::DecisionAndMetadata;
using optimization_guide::AnyWrapProto;
using optimization_guide::OptimizationGuideDecision;
using optimization_guide::OptimizationMetadata;

namespace {
proto::MerchantTrustSignalsV3 CreateValidProto() {
  proto::MerchantTrustSignalsV3 proto;
  proto.set_star_rating(3.8);
  proto.set_count_rating(45);
  proto.set_page_url("https://page_url.com");
  proto.set_overall_summary("Test summary");
  return proto;
}

OptimizationGuideDecision ReturnOptimizationGuideDecisionTrue(
    const GURL& url,
    OptimizationMetadata* metadata) {
  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url("type.googleapis.com/com.foo.Whatever");
  metadata->set_any_metadata(AnyWrapProto(CreateValidProto()));
  return OptimizationGuideDecision::kTrue;
}

OptimizationGuideDecision ReturnOptimizationGuideDecisionUnknown(
    const GURL& url,
    OptimizationMetadata* metadata) {
  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url("type.googleapis.com/com.foo.Whatever");
  metadata->set_any_metadata(AnyWrapProto(CreateValidProto()));
  return OptimizationGuideDecision::kUnknown;
}

}  // namespace

class MockMerchantTrustService : public MerchantTrustService {
 public:
  explicit MockMerchantTrustService()
      : MerchantTrustService(nullptr, false, nullptr) {}

  MOCK_METHOD(bool, IsOptimizationGuideAllowed, (), (const, override));
  MOCK_METHOD(optimization_guide::OptimizationGuideDecision,
              CanApplyOptimization,
              (const GURL&, OptimizationMetadata*),
              (const, override));
};

class MerchantTrustServiceTest : public ::testing::Test {
 public:
  void SetUp() override {
    service_ =
        std::make_unique<testing::StrictMock<MockMerchantTrustService>>();
    SetOptimizationGuideAllowed(true);
  }

  void SetOptimizationGuideAllowed(bool allowed) {
    EXPECT_CALL(*service(), IsOptimizationGuideAllowed())
        .WillRepeatedly(Return(allowed));
  }

  MockMerchantTrustService* service() { return service_.get(); }

 private:
  std::unique_ptr<MockMerchantTrustService> service_;
};

// Tests that proto are returned correctly when optimization guide decision is
// true
TEST_F(MerchantTrustServiceTest, OptimizationGuideDecisionTrue) {
  EXPECT_CALL(*service(), CanApplyOptimization(_, _))
      .WillOnce(Invoke(&ReturnOptimizationGuideDecisionTrue));

  std::optional<page_info::MerchantData> info =
      service()->GetMerchantTrustInfo(GURL("https://foo.com"),
                                      ukm::UkmRecorder::GetNewSourceID());
  EXPECT_TRUE(info.has_value());
  EXPECT_EQ(info->page_url, GURL("https://page_url.com"));
}

// Tests that proto are not returned correctly when optimization guide decision
// is unknown
TEST_F(MerchantTrustServiceTest, OptimizationGuideDecisionUnknown) {
  EXPECT_CALL(*service(), CanApplyOptimization(_, _))
      .WillOnce(Invoke(&ReturnOptimizationGuideDecisionUnknown));

  std::optional<page_info::MerchantData> info =
      service()->GetMerchantTrustInfo(GURL("https://foo.com"),
                                      ukm::UkmRecorder::GetNewSourceID());
  EXPECT_FALSE(info.has_value());
}

// Tests with optimization guide not allowed
TEST_F(MerchantTrustServiceTest, NoOptimizationGuideNotAllowed) {
  SetOptimizationGuideAllowed(false);
  std::optional<page_info::MerchantData> info =
      service()->GetMerchantTrustInfo(GURL("https://foo.com"),
                                      ukm::UkmRecorder::GetNewSourceID());
  EXPECT_FALSE(info.has_value());
}

// Tests that sample data is returned when optimization guide decision is
// unknown and sample data is enabled.
TEST_F(MerchantTrustServiceTest, SampleData) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      kMerchantTrust, {{kMerchantTrustEnabledWithSampleData.name, "true"}});

  EXPECT_CALL(*service(), CanApplyOptimization(_, _))
      .WillOnce(Invoke(&ReturnOptimizationGuideDecisionUnknown));

  std::optional<page_info::MerchantData> info =
      service()->GetMerchantTrustInfo(GURL("https://foo.com"),
                                      ukm::UkmRecorder::GetNewSourceID());
  EXPECT_TRUE(info.has_value());
  EXPECT_EQ(
      info->page_url,
      GURL("https://customerreviews.google.com/v/merchant?q=amazon.com&c=AE&v=19"));
}

}  // namespace page_info
