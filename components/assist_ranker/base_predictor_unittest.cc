// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/base_predictor.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/assist_ranker/fake_ranker_model_loader.h"
#include "components/assist_ranker/predictor_config.h"
#include "components/assist_ranker/proto/ranker_example.pb.h"
#include "components/assist_ranker/ranker_model.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace assist_ranker {

using ::assist_ranker::testing::FakeRankerModelLoader;

namespace {

// Predictor config for testing.
const char kTestModelName[] = "test_model";
// This name needs to be an entry in ukm.xml
const char kTestLoggingName[] = "ContextualSearch";
const char kTestUmaPrefixName[] = "Test.Ranker";
const char kTestUrlParamName[] = "ranker-model-url";
const char kTestDefaultModelUrl[] = "https://foo.bar/model.bin";

const char kTestNavigationUrl[] = "https://foo.com";

const base::flat_set<std::string> kFeatureAllowlist;

BASE_FEATURE(kTestRankerQuery,
             "TestRankerQuery",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kTestRankerUrl{
    &kTestRankerQuery, kTestUrlParamName, kTestDefaultModelUrl};

const PredictorConfig kTestPredictorConfig =
    PredictorConfig{kTestModelName,     kTestLoggingName,
                    kTestUmaPrefixName, LOG_UKM,
                    &kFeatureAllowlist, &kTestRankerQuery,
                    &kTestRankerUrl,    kNoPredictThresholdReplacement};

// Class that implements virtual functions of the base class.
class FakePredictor : public BasePredictor {
 public:
  // Creates a |FakePredictor| using the default config (from this file).
  static std::unique_ptr<FakePredictor> Create() {
    return Create(kTestPredictorConfig);
  }
  // Creates a |FakePredictor| using the |PredictorConfig| passed in
  // |predictor_config|.
  static std::unique_ptr<FakePredictor> Create(
      PredictorConfig predictor_config);

  FakePredictor(const FakePredictor&) = delete;
  FakePredictor& operator=(const FakePredictor&) = delete;

  ~FakePredictor() override {}
  // Validation will always succeed.
  static RankerModelStatus ValidateModel(const RankerModel& model) {
    return RankerModelStatus::OK;
  }

 protected:
  // Not implementing any inference logic.
  bool Initialize() override { return true; }

 private:
  FakePredictor(const PredictorConfig& config) : BasePredictor(config) {}
};

std::unique_ptr<FakePredictor> FakePredictor::Create(
    PredictorConfig predictor_config) {
  std::unique_ptr<FakePredictor> predictor(new FakePredictor(predictor_config));
  auto ranker_model = std::make_unique<RankerModel>();
  auto fake_model_loader = std::make_unique<FakeRankerModelLoader>(
      base::BindRepeating(&FakePredictor::ValidateModel),
      base::BindRepeating(&FakePredictor::OnModelAvailable,
                          base::Unretained(predictor.get())),
      std::move(ranker_model));
  predictor->LoadModel(std::move(fake_model_loader));
  return predictor;
}

}  // namespace

class BasePredictorTest : public ::testing::Test {
 public:
  BasePredictorTest(const BasePredictorTest&) = delete;
  BasePredictorTest& operator=(const BasePredictorTest&) = delete;

 protected:
  BasePredictorTest() = default;

  void SetUp() override;

  ukm::SourceId GetSourceId();

  ukm::TestUkmRecorder* GetTestUkmRecorder() { return &test_ukm_recorder_; }

 private:
  // Sets up the task scheduling/task-runner environment for each test.
  base::test::TaskEnvironment task_environment_;

  // Sets itself as the global UkmRecorder on construction.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;

  // Manages the enabling/disabling of features within the scope of a test.
  base::test::ScopedFeatureList scoped_feature_list_;
};

void BasePredictorTest::SetUp() {
  ::testing::Test::SetUp();
  scoped_feature_list_.Init();
}

ukm::SourceId BasePredictorTest::GetSourceId() {
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  test_ukm_recorder_.UpdateSourceURL(source_id, GURL(kTestNavigationUrl));
  return source_id;
}

TEST_F(BasePredictorTest, BaseTest) {
  auto predictor = FakePredictor::Create();
  EXPECT_EQ(kTestModelName, predictor->GetModelName());
  EXPECT_EQ(kTestDefaultModelUrl, predictor->GetModelUrl());
  EXPECT_TRUE(predictor->is_query_enabled());
  EXPECT_TRUE(predictor->IsReady());
}

TEST_F(BasePredictorTest, QueryDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kTestRankerQuery);
  auto predictor = FakePredictor::Create();
  EXPECT_EQ(kTestModelName, predictor->GetModelName());
  EXPECT_EQ(kTestDefaultModelUrl, predictor->GetModelUrl());
  EXPECT_FALSE(predictor->is_query_enabled());
  EXPECT_FALSE(predictor->IsReady());
}

TEST_F(BasePredictorTest, GetPredictThresholdReplacement) {
  float altered_threshold = 0.78f;  // Arbitrary value.
  const PredictorConfig altered_threshold_config{
      kTestModelName,  kTestLoggingName,   kTestUmaPrefixName,
      LOG_UKM,         &kFeatureAllowlist, &kTestRankerQuery,
      &kTestRankerUrl, altered_threshold};
  auto predictor = FakePredictor::Create(altered_threshold_config);
  EXPECT_EQ(altered_threshold, predictor->GetPredictThresholdReplacement());
}

}  // namespace assist_ranker
