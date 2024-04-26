// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/v8_compile_hints/v8_compile_hints_tab_helper.h"

#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

using ::testing::_;
using ::testing::An;
using ::testing::ByRef;
using ::testing::Invoke;
using ::testing::NiceMock;

namespace v8_compile_hints {

class V8CompileHintsTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override;
  void TearDown() override;

  void NavigateAndCommitInFrame(const std::string& url,
                                content::RenderFrameHost* rfh);

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  // Owned by OptimizationGuide.
  raw_ptr<NiceMock<MockOptimizationGuideKeyedService>>
      mock_optimization_guide_keyed_service_;
  // Owned by |web_contents()|.
  raw_ptr<V8CompileHintsTabHelper> tab_helper_;
};

void V8CompileHintsTabHelperTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  CreateSessionServiceTabHelper(web_contents());
  scoped_feature_list_.InitWithFeatures(
      {blink::features::kConsumeCompileHints,
       optimization_guide::features::kOptimizationHints},
      {});

  mock_optimization_guide_keyed_service_ =
      static_cast<NiceMock<MockOptimizationGuideKeyedService>*>(
          OptimizationGuideKeyedServiceFactory::GetInstance()
              ->SetTestingFactoryAndUse(
                  profile(),
                  base::BindRepeating([](content::BrowserContext* context)
                                          -> std::unique_ptr<KeyedService> {
                    return std::make_unique<
                        NiceMock<MockOptimizationGuideKeyedService>>();
                  })));
  V8CompileHintsTabHelper::MaybeCreateForWebContents(web_contents());
  tab_helper_ = V8CompileHintsTabHelper::FromWebContents(web_contents());
}

void V8CompileHintsTabHelperTest::TearDown() {
  mock_optimization_guide_keyed_service_ = nullptr;
  tab_helper_ = nullptr;
  ChromeRenderViewHostTestHarness::TearDown();
}

void V8CompileHintsTabHelperTest::NavigateAndCommitInFrame(
    const std::string& url,
    content::RenderFrameHost* rfh) {
  auto navigation =
      content::NavigationSimulator::CreateRendererInitiated(GURL(url), rfh);
  // TODO(crbug.com/40276923): Consider refactoring to rely on load
  // events dispatched by NavigationSimulator.
  navigation->SetKeepLoading(true);
  navigation->Start();
  navigation->Commit();
}

namespace {

optimization_guide::OptimizationMetadata CreateMetadata(
    size_t bloom_filter_size = V8CompileHintsTabHelper::kModelInt64Count,
    int32_t clear_zeros = 50000,
    int32_t clear_ones = 2000) {
  optimization_guide::OptimizationMetadata optimization_metadata;
  proto::Model model;
  for (size_t i = 0; i < bloom_filter_size; ++i) {
    model.add_bloom_filter(static_cast<int64_t>(i));
  }
  model.set_sample_count(1000);
  model.set_clear_zeros(clear_zeros);
  model.set_clear_ones(clear_ones);
  optimization_guide::proto::Any any;
  any.set_type_url(model.GetTypeName());
  model.SerializeToString(any.mutable_value());
  optimization_metadata.set_any_metadata(any);
  return optimization_metadata;
}

optimization_guide::OptimizationMetadata CreateInvalidMetadata() {
  return CreateMetadata(V8CompileHintsTabHelper::kModelInt64Count - 1);
}

optimization_guide::OptimizationMetadata CreateBadMetadata() {
  return CreateMetadata(V8CompileHintsTabHelper::kModelInt64Count, 1000, 0);
}

}  // namespace

TEST_F(V8CompileHintsTabHelperTest, DataFromOptimizationGuide) {
  base::HistogramTester histogram_tester;
  bool data_sent = false;
  tab_helper_->SetSendDataToRendererForTesting(base::BindLambdaForTesting(
      [&data_sent](const proto::Model& model) { data_sent = true; }));

  optimization_guide::OptimizationMetadata optimization_metadata =
      CreateMetadata();
  optimization_guide::OptimizationGuideDecisionCallback stored_callback;
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(
          _, _, An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(MoveArg<2>(&stored_callback));

  NavigateAndCommitInFrame("http://test.org", main_rfh());

  std::move(stored_callback)
      .Run(optimization_guide::OptimizationGuideDecision::kTrue,
           ByRef(optimization_metadata));
  EXPECT_TRUE(data_sent);
  histogram_tester.ExpectUniqueSample(
      V8CompileHintsTabHelper::kModelQualityHistogramName,
      V8CompileHintsModelQuality::kGoodModel, 1);
}

TEST_F(V8CompileHintsTabHelperTest, NonHttpNavigationIgnored) {
  bool data_sent = false;
  tab_helper_->SetSendDataToRendererForTesting(base::BindLambdaForTesting(
      [&data_sent](const proto::Model& model) { data_sent = true; }));

  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(
          _, _, An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);

  NavigateAndCommitInFrame("ftp://test.org", main_rfh());
  EXPECT_FALSE(data_sent);
}

TEST_F(V8CompileHintsTabHelperTest, InvalidModelIgnored) {
  base::HistogramTester histogram_tester;
  bool data_sent = false;
  tab_helper_->SetSendDataToRendererForTesting(base::BindLambdaForTesting(
      [&data_sent](const proto::Model& model) { data_sent = true; }));

  optimization_guide::OptimizationMetadata optimization_metadata =
      CreateInvalidMetadata();
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(
                  _, optimization_guide::proto::V8_COMPILE_HINTS,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          ByRef(optimization_metadata)));

  NavigateAndCommitInFrame("http://test.org", main_rfh());

  EXPECT_FALSE(data_sent);
  histogram_tester.ExpectUniqueSample(
      V8CompileHintsTabHelper::kModelQualityHistogramName,
      V8CompileHintsModelQuality::kNoModel, 1);
}

TEST_F(V8CompileHintsTabHelperTest, BadModelIgnored) {
  base::HistogramTester histogram_tester;
  bool data_sent = false;
  tab_helper_->SetSendDataToRendererForTesting(base::BindLambdaForTesting(
      [&data_sent](const proto::Model& model) { data_sent = true; }));

  optimization_guide::OptimizationMetadata optimization_metadata =
      CreateBadMetadata();
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(
                  _, optimization_guide::proto::V8_COMPILE_HINTS,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          ByRef(optimization_metadata)));

  NavigateAndCommitInFrame("http://test.org", main_rfh());

  EXPECT_FALSE(data_sent);
  histogram_tester.ExpectUniqueSample(
      V8CompileHintsTabHelper::kModelQualityHistogramName,
      V8CompileHintsModelQuality::kBadModel, 1);
}

}  // namespace v8_compile_hints
