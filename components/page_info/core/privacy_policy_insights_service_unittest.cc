// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/privacy_policy_insights_service.h"

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/proto/privacy_policy_annotation_metadata.pb.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_info {
using testing::_;
using testing::Return;

class MockPrivacyPolicyInsightsService : public PrivacyPolicyInsightsService {
 public:
  MockPrivacyPolicyInsightsService()
      : PrivacyPolicyInsightsService(nullptr, false, nullptr) {}

  MOCK_METHOD(bool, IsOptimizationGuideAllowed, (), (const, override));
};

class PrivacyPolicyInsightsServiceTest : public ::testing::Test {
 public:
  void SetUp() override {
    service_ = std::make_unique<
        testing::StrictMock<MockPrivacyPolicyInsightsService>>();
    SetOptimizationGuideAllowed(true);
  }

  void SetOptimizationGuideAllowed(bool allowed) {
    EXPECT_CALL(*service(), IsOptimizationGuideAllowed())
        .WillRepeatedly(Return(allowed));
  }

  MockPrivacyPolicyInsightsService* service() { return service_.get(); }

 private:
  std::unique_ptr<MockPrivacyPolicyInsightsService> service_;
};

// Tests with correct proto
TEST_F(PrivacyPolicyInsightsServiceTest, ValidResponse) {
  std::optional<page_info::proto::PrivacyPolicyAnnotation> annotation =
      service()->GetPrivacyPolicyAnnotation(
          GURL("https://www.foo.com/policies/privacy/"),
          ukm::UkmRecorder::GetNewSourceID());
  EXPECT_TRUE(annotation.has_value());
  EXPECT_TRUE(annotation->is_privacy_policy());
  EXPECT_EQ(annotation->canonical_url(),
            "https://www.foo.com/policies/privacy/");
}

// Tests with optimization guide not allowed
TEST_F(PrivacyPolicyInsightsServiceTest, OptimizationGuideNotAllowed) {
  SetOptimizationGuideAllowed(false);
  std::optional<page_info::proto::PrivacyPolicyAnnotation> annotation =
      service()->GetPrivacyPolicyAnnotation(GURL("https://foo.com"),
                                            ukm::UkmRecorder::GetNewSourceID());
  EXPECT_FALSE(annotation.has_value());
}

// Tests with invalid URL for KeyedHints
TEST_F(PrivacyPolicyInsightsServiceTest, InvalidUrlForKeyedHints) {
  std::optional<page_info::proto::PrivacyPolicyAnnotation> annotation =
      service()->GetPrivacyPolicyAnnotation(
          GURL("https://username:password@www.example.com/"),
          ukm::UkmRecorder::GetNewSourceID());
  EXPECT_FALSE(annotation.has_value());
}

}  // namespace page_info
