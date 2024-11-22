// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/merchant_trust_service.h"

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
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
using testing::Return;

class MockMerchantTrustService : public MerchantTrustService {
 public:
  explicit MockMerchantTrustService()
      : MerchantTrustService(nullptr, false, nullptr) {}

  MOCK_METHOD(bool, IsOptimizationGuideAllowed, (), (const, override));
};

class MerchantTrustServiceTest : public ::testing::Test {
 public:
  void SetUp() override {
    service_ = std::make_unique<testing::StrictMock<MockMerchantTrustService>>();
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

// Tests with correct proto
TEST_F(MerchantTrustServiceTest, ValidResponse) {
  std::optional<page_info::proto::MerchantTrustSignalsV3> info =
      service()->GetMerchantTrustInfo(GURL("https://foo.com"),
                                      ukm::UkmRecorder::GetNewSourceID());
  EXPECT_TRUE(info.has_value());
  EXPECT_EQ(
      info->page_url(),
      "https://customerreviews.google.com/v/merchant?q=amazon.com&c=AE&v=19");
}

// Tests with optimization guide not allowed
TEST_F(MerchantTrustServiceTest, NoOptimizationGuideNotAllowed) {
  SetOptimizationGuideAllowed(false);
  std::optional<page_info::proto::MerchantTrustSignalsV3> info =
      service()->GetMerchantTrustInfo(GURL("https://foo.com"),
                                      ukm::UkmRecorder::GetNewSourceID());
  EXPECT_FALSE(info.has_value());
}


}  // namespace page_info
