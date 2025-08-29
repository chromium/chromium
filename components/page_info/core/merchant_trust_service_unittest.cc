// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/merchant_trust_service.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/commerce/core/proto/merchant_trust.pb.h"
#include "components/optimization_guide/core/hints/mock_optimization_guide_decider.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/merchant_trust_validation.h"
#include "components/page_info/core/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_info {
using testing::_;
using testing::An;
using testing::Return;

using DecisionWithMetadata = MerchantTrustService::DecisionAndMetadata;
using optimization_guide::AnyWrapProto;
using optimization_guide::MockOptimizationGuideDecider;
using optimization_guide::OptimizationGuideDecision;
using optimization_guide::OptimizationGuideDecisionCallback;
using optimization_guide::OptimizationMetadata;
using optimization_guide::proto::OptimizationType;
using MerchantTrustStatus = merchant_trust_validation::MerchantTrustStatus;

namespace {
const char kTestSummary[] = "This is a test summary.";

commerce::MerchantTrustSignalsV2 CreateValidProto() {
  commerce::MerchantTrustSignalsV2 proto;
  proto.set_merchant_star_rating(3.8);
  proto.set_merchant_count_rating(45);
  proto.set_merchant_details_page_url("https://page_url.com");
  proto.set_shopper_voice_summary(kTestSummary);
  return proto;
}

OptimizationMetadata BuildMerchantTrustResponse() {
  OptimizationMetadata meta;
  meta.set_any_metadata(AnyWrapProto(CreateValidProto()));
  return meta;
}

}  // namespace

class MockMerchantTrustServiceDelegate : public MerchantTrustService::Delegate {
 public:
  MOCK_METHOD(void, ShowEvaluationSurvey, (), (override));
  MOCK_METHOD(double, GetSiteEngagementScore, (const GURL url), (override));
};

class MockMerchantTrustService : public MerchantTrustService {
 public:
  explicit MockMerchantTrustService(
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      std::unique_ptr<MockMerchantTrustServiceDelegate> delegate,
      PrefService* prefs)
      : MerchantTrustService(std::move(delegate),
                             optimization_guide_decider,
                             /*is_off_the_record=*/false,
                             prefs) {}

  MOCK_METHOD(bool, IsOptimizationGuideAllowed, (), (const, override));
};

class MerchantTrustServiceTest : public ::testing::Test {
 public:
  MerchantTrustServiceTest() {
    page_info::MerchantTrustService::RegisterProfilePrefs(prefs()->registry());
  }

  void SetUp() override {
    auto delegate = std::make_unique<MockMerchantTrustServiceDelegate>();
    delegate_ = delegate.get();
    service_ = std::make_unique<MockMerchantTrustService>(
        &opt_guide(), std::move(delegate), prefs());
    SetOptimizationGuideAllowed(true);
    SetResponse(GURL("https://foo.com"), OptimizationGuideDecision::kUnknown,
                BuildMerchantTrustResponse());
    clock_.SetNow(base::Time::Now());
    service_->SetClockForTesting(&clock_);
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
            [decision, metadata](
                const GURL& url, OptimizationType optimization_type,
                optimization_guide::OptimizationGuideDecisionCallback
                    callback) { std::move(callback).Run(decision, metadata); });
  }

  void SetOptimizationGuideAllowed(bool allowed) {
    EXPECT_CALL(*service(), IsOptimizationGuideAllowed())
        .WillRepeatedly(Return(allowed));
  }

  MockMerchantTrustService* service() { return service_.get(); }
  optimization_guide::MockOptimizationGuideDecider& opt_guide() {
    return opt_guide_;
  }
  MockMerchantTrustServiceDelegate* delegate() { return delegate_; }

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

  base::SimpleTestClock* clock() { return &clock_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::NiceMock<optimization_guide::MockOptimizationGuideDecider>
      opt_guide_;
  std::unique_ptr<MockMerchantTrustService> service_;
  raw_ptr<MockMerchantTrustServiceDelegate> delegate_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  base::SimpleTestClock clock_;
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
                           "merchant?q=amazon.com&c=US&gl=US"));
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

// Tests that if the proto is empty, no data is returned.
TEST_F(MerchantTrustServiceTest, NoResult) {
  base::HistogramTester t;
  OptimizationMetadata metadata;
  metadata.set_any_metadata({});
  SetResponse(GURL("https://foo.com"), OptimizationGuideDecision::kTrue,
              metadata);

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
  t.ExpectUniqueSample("Security.PageInfo.MerchantTrustStatus",
                       MerchantTrustStatus::kNoResult, 1);
}

// Tests that status is recorded as not valid when a proto is missing a field
// and no data is returned.
TEST_F(MerchantTrustServiceTest, InvalidProto) {
  base::HistogramTester t;

  commerce::MerchantTrustSignalsV2 proto = CreateValidProto();
  proto.clear_merchant_details_page_url();
  OptimizationMetadata metadata;
  metadata.set_any_metadata(AnyWrapProto(proto));

  SetResponse(GURL("https://foo.com"), OptimizationGuideDecision::kTrue,
              metadata);

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
  t.ExpectUniqueSample("Security.PageInfo.MerchantTrustStatus",
                       MerchantTrustStatus::kMissingReviewsPageUrl, 1);
}

// Tests that status is recorded as valid with missing reviews summary when a
// proto is missing the reviews summary field and the feature param is enabled.
TEST_F(MerchantTrustServiceTest, ValidProtoMissingReviewsSummary) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      kMerchantTrust, {{kMerchantTrustWithoutSummaryName, "true"}});
  base::HistogramTester t;

  commerce::MerchantTrustSignalsV2 proto = CreateValidProto();
  proto.clear_shopper_voice_summary();
  OptimizationMetadata metadata;
  metadata.set_any_metadata(AnyWrapProto(proto));

  SetResponse(GURL("https://foo.com"), OptimizationGuideDecision::kTrue,
              metadata);

  base::RunLoop run_loop;
  service()->GetMerchantTrustInfo(
      GURL("https://foo.com"),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             std::optional<page_info::MerchantData> info) {
            ASSERT_TRUE(info.has_value());
            ASSERT_EQ(info->page_url, GURL("https://page_url.com"));
            ASSERT_EQ(info->reviews_summary, "");
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
  t.ExpectUniqueSample("Security.PageInfo.MerchantTrustStatus",
                       MerchantTrustStatus::kValidWithMissingReviewsSummary, 1);
}

// Tests that status is recorded as valid with missing reviews summary but not
// returning any data when a proto is missing the reviews summary field and the
// feature param is disabled.
TEST_F(MerchantTrustServiceTest, ValidProtoMissingReviewsSummaryNoData) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      kMerchantTrust, {{kMerchantTrustWithoutSummaryName, "false"}});
  base::HistogramTester t;

  commerce::MerchantTrustSignalsV2 proto = CreateValidProto();
  proto.clear_shopper_voice_summary();
  OptimizationMetadata metadata;
  metadata.set_any_metadata(AnyWrapProto(proto));

  SetResponse(GURL("https://foo.com"), OptimizationGuideDecision::kTrue,
              metadata);

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
  t.ExpectUniqueSample("Security.PageInfo.MerchantTrustStatus",
                       MerchantTrustStatus::kValidWithMissingReviewsSummary, 1);
}
// Tests for control evaluation survey.
TEST_F(MerchantTrustServiceTest, ControlSurvey) {
  base::test::ScopedFeatureList feature_list;
  EXPECT_CALL(*delegate(), ShowEvaluationSurvey()).Times(1);
  feature_list.InitWithFeatureState(kMerchantTrustEvaluationControlSurvey,
                                    true);

  prefs()->SetTime(prefs::kMerchantTrustPageInfoLastOpenTime, clock()->Now());
  clock()->Advance(kMerchantTrustEvaluationControlMinTimeToShowSurvey.Get());
  service()->MaybeShowEvaluationSurvey();
}

TEST_F(MerchantTrustServiceTest, ControlSurveyInteractionTooEarly) {
  base::test::ScopedFeatureList feature_list;
  EXPECT_CALL(*delegate(), ShowEvaluationSurvey()).Times(0);
  feature_list.InitWithFeatureState(kMerchantTrustEvaluationControlSurvey,
                                    true);

  prefs()->SetTime(prefs::kMerchantTrustPageInfoLastOpenTime, clock()->Now());
  clock()->Advance(kMerchantTrustEvaluationControlMinTimeToShowSurvey.Get() -
                   base::Seconds(1));
  service()->MaybeShowEvaluationSurvey();
}

TEST_F(MerchantTrustServiceTest, ControlSurveyInteractionExpired) {
  base::test::ScopedFeatureList feature_list;
  EXPECT_CALL(*delegate(), ShowEvaluationSurvey()).Times(0);
  feature_list.InitWithFeatureState(kMerchantTrustEvaluationControlSurvey,
                                    true);

  prefs()->SetTime(prefs::kMerchantTrustPageInfoLastOpenTime, clock()->Now());
  clock()->Advance(kMerchantTrustEvaluationControlMaxTimeToShowSurvey.Get() +
                   base::Seconds(1));
  service()->MaybeShowEvaluationSurvey();
}

// Test for control evaluation survey disabled.
TEST_F(MerchantTrustServiceTest, ControlSurveyDisabled) {
  base::test::ScopedFeatureList feature_list;
  EXPECT_CALL(*delegate(), ShowEvaluationSurvey()).Times(0);

  feature_list.InitWithFeatureState(kMerchantTrustEvaluationControlSurvey,
                                    false);
  service()->MaybeShowEvaluationSurvey();
}

// Tests for experiment evaluation survey.
TEST_F(MerchantTrustServiceTest, ExperimentSurvey) {
  base::test::ScopedFeatureList feature_list;
  EXPECT_CALL(*delegate(), ShowEvaluationSurvey()).Times(1);
  feature_list.InitWithFeatureState(kMerchantTrustEvaluationExperimentSurvey,
                                    true);

  prefs()->SetTime(prefs::kMerchantTrustUiLastInteractionTime, clock()->Now());
  clock()->Advance(kMerchantTrustEvaluationExperimentMinTimeToShowSurvey.Get());
  service()->MaybeShowEvaluationSurvey();
}

TEST_F(MerchantTrustServiceTest, ExperimentSurveyInteractionTooEarly) {
  base::test::ScopedFeatureList feature_list;
  EXPECT_CALL(*delegate(), ShowEvaluationSurvey()).Times(0);
  feature_list.InitWithFeatureState(kMerchantTrustEvaluationExperimentSurvey,
                                    true);

  prefs()->SetTime(prefs::kMerchantTrustUiLastInteractionTime, clock()->Now());
  clock()->Advance(kMerchantTrustEvaluationExperimentMinTimeToShowSurvey.Get() -
                   base::Seconds(1));
  service()->MaybeShowEvaluationSurvey();
}

TEST_F(MerchantTrustServiceTest, ExperimentSurveyInteractionExpired) {
  base::test::ScopedFeatureList feature_list;
  EXPECT_CALL(*delegate(), ShowEvaluationSurvey()).Times(0);
  feature_list.InitWithFeatureState(kMerchantTrustEvaluationExperimentSurvey,
                                    true);

  prefs()->SetTime(prefs::kMerchantTrustUiLastInteractionTime, clock()->Now());
  clock()->Advance(kMerchantTrustEvaluationExperimentMaxTimeToShowSurvey.Get() +
                   base::Seconds(1));
  service()->MaybeShowEvaluationSurvey();
}

// Test for experiment evaluation survey disabled.
TEST_F(MerchantTrustServiceTest, ExperimentSurveyDisabled) {
  base::test::ScopedFeatureList feature_list;
  EXPECT_CALL(*delegate(), ShowEvaluationSurvey()).Times(0);

  feature_list.InitWithFeatureState(kMerchantTrustEvaluationExperimentSurvey,
                                    false);
  service()->MaybeShowEvaluationSurvey();
}

TEST_F(MerchantTrustServiceTest, RecordMerchantTrustInteractionFamiliarSite) {
  base::HistogramTester t;
  base::UserActionTester user_action_tester;

  EXPECT_CALL(*delegate(), GetSiteEngagementScore(_))
      .WillRepeatedly(Return(kMerchantFamiliarityThreshold));
  service()->RecordMerchantTrustInteraction(
      GURL("https://foo.com"), MerchantTrustInteraction::kPageInfoRowShown);

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "MerchantTrust.PageInfoRowSeen.FamiliarSite"));
  t.ExpectUniqueSample(
      "Security.PageInfo.MerchantTrustInteraction.FamiliarSite",
      MerchantTrustInteraction::kPageInfoRowShown, 1);
  t.ExpectBucketCount(
      "Security.PageInfo.MerchantTrustEngagement.PageInfoRowShown",
      kMerchantFamiliarityThreshold, 1);
}

TEST_F(MerchantTrustServiceTest, RecordMerchantTrustInteractionUnfamiliarSite) {
  base::HistogramTester t;
  base::UserActionTester user_action_tester;

  EXPECT_CALL(*delegate(), GetSiteEngagementScore(_))
      .WillRepeatedly(Return(kMerchantFamiliarityThreshold - 0.1));
  service()->RecordMerchantTrustInteraction(
      GURL("https://foo.com"),
      MerchantTrustInteraction::kBubbleOpenedFromPageInfo);

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "MerchantTrust.BubbleOpenedFromPageInfo.UnfamiliarSite"));
  t.ExpectUniqueSample(
      "Security.PageInfo.MerchantTrustInteraction.UnfamiliarSite",
      MerchantTrustInteraction::kBubbleOpenedFromPageInfo, 1);
  t.ExpectBucketCount("Security.PageInfo.MerchantTrustEngagement.BubbleOpened",
                      kMerchantFamiliarityThreshold - 1, 1);
}

}  // namespace page_info
