// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/classifier_predictor.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "components/assist_ranker/example_preprocessing.h"
#include "components/assist_ranker/fake_ranker_model_loader.h"
#include "components/assist_ranker/nn_classifier_test_util.h"
#include "components/assist_ranker/proto/ranker_model.pb.h"
#include "components/assist_ranker/ranker_model.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace assist_ranker {

using ::assist_ranker::testing::FakeRankerModelLoader;
using ::testing::FloatEq;

// Preprocessor feature names.
const char kFeatureName0[] = "feature_0";
const char kFeatureName1[] = "feature_1";
const char kFeatureExtra[] = "feature_extra";

class ClassifierPredictorTest : public ::testing::Test {
 public:
  void SetUp() override;

  std::unique_ptr<ClassifierPredictor> InitPredictor(
      std::unique_ptr<RankerModel> ranker_model,
      const PredictorConfig& config);

  PredictorConfig GetConfig();

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

void ClassifierPredictorTest::SetUp() {
  ::testing::Test::SetUp();
  scoped_feature_list_.Init();
}

std::unique_ptr<ClassifierPredictor> ClassifierPredictorTest::InitPredictor(
    std::unique_ptr<RankerModel> ranker_model,
    const PredictorConfig& config) {
  std::unique_ptr<ClassifierPredictor> predictor(
      new ClassifierPredictor(config));
  auto fake_model_loader = std::make_unique<FakeRankerModelLoader>(
      base::BindRepeating(&ClassifierPredictor::ValidateModel),
      base::BindRepeating(&ClassifierPredictor::OnModelAvailable,
                          base::Unretained(predictor.get())),
      std::move(ranker_model));
  predictor->LoadModel(std::move(fake_model_loader));
  return predictor;
}

BASE_FEATURE(kTestRankerQuery,
             "TestRankerQuery",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kTestRankerUrl{
    &kTestRankerQuery, "url-param-name", "https://default.model.url"};

PredictorConfig ClassifierPredictorTest::GetConfig() {
  return PredictorConfig("model_name", "logging_name", "uma_prefix", LOG_NONE,
                         GetEmptyAllowlist(), &kTestRankerQuery,
                         &kTestRankerUrl, 0);
}

TEST_F(ClassifierPredictorTest, EmptyRankerModel) {
  auto ranker_model = std::make_unique<RankerModel>();
  auto predictor = InitPredictor(std::move(ranker_model), GetConfig());
  EXPECT_FALSE(predictor->IsReady());

  RankerExample ranker_example;
  auto& features = *ranker_example.mutable_features();
  features[kFeatureName0].set_bool_value(true);
  std::vector<float> response;
  EXPECT_FALSE(predictor->Predict(ranker_example, &response));
}

TEST_F(ClassifierPredictorTest, NoInferenceModuleForModel) {
  auto ranker_model = std::make_unique<RankerModel>();
  // TranslateRankerModel does not have an inference module. Validation will
  // fail.
  ranker_model->mutable_proto()
      ->mutable_translate()
      ->mutable_translate_logistic_regression_model()
      ->set_bias(1);
  auto predictor = InitPredictor(std::move(ranker_model), GetConfig());
  EXPECT_FALSE(predictor->IsReady());

  RankerExample ranker_example;
  auto& features = *ranker_example.mutable_features();
  features[kFeatureName0].set_bool_value(true);
  std::vector<float> response;
  EXPECT_FALSE(predictor->Predict(ranker_example, &response));
  EXPECT_FALSE(predictor->Predict({{0, 0}}, &response));
}

TEST_F(ClassifierPredictorTest, PredictFeatureVector) {
  auto ranker_model = std::make_unique<RankerModel>();
  *ranker_model->mutable_proto()->mutable_nn_classifier() =
      nn_classifier::CreateXorClassifierModel();
  auto predictor = InitPredictor(std::move(ranker_model), GetConfig());
  EXPECT_TRUE(predictor->IsReady());

  std::vector<float> response;
  // True responses.
  EXPECT_TRUE(predictor->Predict({{0.0, 1}}, &response));
  EXPECT_EQ(response.size(), 1u);
  EXPECT_THAT(response[0], FloatEq(2.8271765));
  EXPECT_TRUE(predictor->Predict({{1, 0}}, &response));
  EXPECT_EQ(response.size(), 1u);
  EXPECT_THAT(response[0], FloatEq(2.6790769));

  // False responses.
  EXPECT_TRUE(predictor->Predict({{0, 0}}, &response));
  EXPECT_EQ(response.size(), 1u);
  EXPECT_THAT(response[0], FloatEq(-2.7154054));
  EXPECT_TRUE(predictor->Predict({{1, 1}}, &response));
  EXPECT_EQ(response.size(), 1u);
  EXPECT_THAT(response[0], FloatEq(-3.1652793));
}

TEST_F(ClassifierPredictorTest, PredictRankerExampleNoPreprocessor) {
  auto ranker_model = std::make_unique<RankerModel>();
  *ranker_model->mutable_proto()->mutable_nn_classifier() =
      nn_classifier::CreateXorClassifierModel();
  auto predictor = InitPredictor(std::move(ranker_model), GetConfig());
  EXPECT_TRUE(predictor->IsReady());

  // Prediction of RankerExample without preprocessor config should fail.
  std::vector<float> response;
  RankerExample example;
  EXPECT_FALSE(predictor->Predict(RankerExample(), &response));
}

TEST_F(ClassifierPredictorTest, PredictRankerExampleWithPreprocessor) {
  auto ranker_model = std::make_unique<RankerModel>();
  auto& model = *ranker_model->mutable_proto()->mutable_nn_classifier();
  model = nn_classifier::CreateXorClassifierModel();

  // Set up the preprocessor config with two features at feature vector
  // indices 0 and 1.
  auto& indices =
      *model.mutable_preprocessor_config()->mutable_feature_indices();
  indices[kFeatureName0] = 0;
  indices[kFeatureName1] = 1;

  auto predictor = InitPredictor(std::move(ranker_model), GetConfig());
  EXPECT_TRUE(predictor->IsReady());

  // Prediction of RankerExample with preprocessor config should work.
  RankerExample example;
  auto& feature_map = *example.mutable_features();
  std::vector<float> response;

  // True responses.
  feature_map[kFeatureName0].set_float_value(0);
  feature_map[kFeatureName1].set_float_value(1);
  EXPECT_TRUE(predictor->Predict(example, &response));
  EXPECT_EQ(response.size(), 1u);
  EXPECT_THAT(response[0], FloatEq(2.8271765));

  feature_map[kFeatureName0].set_float_value(1);
  feature_map[kFeatureName1].set_float_value(0);
  EXPECT_TRUE(predictor->Predict(example, &response));
  EXPECT_EQ(response.size(), 1u);
  EXPECT_THAT(response[0], FloatEq(2.6790769));

  // False responses.
  feature_map[kFeatureName0].set_float_value(0);
  feature_map[kFeatureName1].set_float_value(0);
  EXPECT_TRUE(predictor->Predict(example, &response));
  EXPECT_EQ(response.size(), 1u);
  EXPECT_THAT(response[0], FloatEq(-2.7154054));

  feature_map[kFeatureName0].set_float_value(1);
  feature_map[kFeatureName1].set_float_value(1);
  EXPECT_TRUE(predictor->Predict(example, &response));
  EXPECT_EQ(response.size(), 1u);
  EXPECT_THAT(response[0], FloatEq(-3.1652793));

  // Check that extra features do not cause an error.
  feature_map[kFeatureName0].set_float_value(0);
  feature_map[kFeatureName1].set_float_value(1);
  feature_map[kFeatureExtra].set_float_value(1);
  EXPECT_TRUE(predictor->Predict(example, &response));
  EXPECT_EQ(response.size(), 1u);
  EXPECT_THAT(response[0], FloatEq(2.8271765));
}

TEST_F(ClassifierPredictorTest, PredictRankerExamplePreprocessorError) {
  auto ranker_model = std::make_unique<RankerModel>();
  auto& model = *ranker_model->mutable_proto()->mutable_nn_classifier();
  model = nn_classifier::CreateXorClassifierModel();

  // Set up the preprocessor config with two features at feature vector
  // indices 0 and 1.
  auto& config = *model.mutable_preprocessor_config();
  auto& indices = *config.mutable_feature_indices();
  indices[kFeatureName0] = 0;
  indices[kFeatureName1] = 1;
  // Zero normalizer will generate a preprocessing error.
  (*config.mutable_normalizers())[kFeatureName0] = 0;

  auto predictor = InitPredictor(std::move(ranker_model), GetConfig());
  EXPECT_TRUE(predictor->IsReady());

  // Prediction of RankerExample should fail due to preprocessing error.
  RankerExample example;
  auto& feature_map = *example.mutable_features();
  std::vector<float> response;
  feature_map[kFeatureName0].set_float_value(0);
  feature_map[kFeatureName1].set_float_value(1);
  EXPECT_FALSE(predictor->Predict(example, &response));
}
}  // namespace assist_ranker
