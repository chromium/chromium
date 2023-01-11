// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/binary_classifier_predictor.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "components/assist_ranker/fake_ranker_model_loader.h"
#include "components/assist_ranker/proto/ranker_model.pb.h"
#include "components/assist_ranker/ranker_model.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace assist_ranker {

using ::assist_ranker::testing::FakeRankerModelLoader;

class BinaryClassifierPredictorTest : public ::testing::Test {
 public:
  void SetUp() override;

  std::unique_ptr<BinaryClassifierPredictor> InitPredictor(
      std::unique_ptr<RankerModel> ranker_model,
      const PredictorConfig& config);

  // This model will return the value of |feature| as a prediction.
  GenericLogisticRegressionModel GetSimpleLogisticRegressionModel();

  PredictorConfig GetConfig();
  PredictorConfig GetConfig(float predictor_threshold_replacement);

 protected:
  const std::string feature_ = "feature";
  const float weight_ = 1.0;
  const float threshold_ = 0.5;
  base::test::ScopedFeatureList scoped_feature_list_;
};

void BinaryClassifierPredictorTest::SetUp() {
  ::testing::Test::SetUp();
  scoped_feature_list_.Init();
}

std::unique_ptr<BinaryClassifierPredictor>
BinaryClassifierPredictorTest::InitPredictor(
    std::unique_ptr<RankerModel> ranker_model,
    const PredictorConfig& config) {
  std::unique_ptr<BinaryClassifierPredictor> predictor(
      new BinaryClassifierPredictor(config));
  auto fake_model_loader = std::make_unique<FakeRankerModelLoader>(
      base::BindRepeating(&BinaryClassifierPredictor::ValidateModel),
      base::BindRepeating(&BinaryClassifierPredictor::OnModelAvailable,
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

PredictorConfig BinaryClassifierPredictorTest::GetConfig() {
  return GetConfig(kNoPredictThresholdReplacement);
}

PredictorConfig BinaryClassifierPredictorTest::GetConfig(
    float predictor_threshold_replacement) {
  PredictorConfig config("model_name", "logging_name", "uma_prefix", LOG_NONE,
                         GetEmptyAllowlist(), &kTestRankerQuery,
                         &kTestRankerUrl, predictor_threshold_replacement);

  return config;
}

GenericLogisticRegressionModel
BinaryClassifierPredictorTest::GetSimpleLogisticRegressionModel() {
  GenericLogisticRegressionModel lr_model;
  lr_model.set_bias(-0.5);
  lr_model.set_threshold(threshold_);
  (*lr_model.mutable_weights())[feature_].set_scalar(weight_);
  return lr_model;
}

// TODO(hamelphi): Test BinaryClassifierPredictor::Create.

TEST_F(BinaryClassifierPredictorTest, EmptyRankerModel) {
  auto ranker_model = std::make_unique<RankerModel>();
  auto predictor = InitPredictor(std::move(ranker_model), GetConfig());
  EXPECT_FALSE(predictor->IsReady());

  RankerExample ranker_example;
  auto& features = *ranker_example.mutable_features();
  features[feature_].set_bool_value(true);
  bool bool_response;
  EXPECT_FALSE(predictor->Predict(ranker_example, &bool_response));
  float float_response;
  EXPECT_FALSE(predictor->PredictScore(ranker_example, &float_response));
}

TEST_F(BinaryClassifierPredictorTest, NoInferenceModuleForModel) {
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
  features[feature_].set_bool_value(true);
  bool bool_response;
  EXPECT_FALSE(predictor->Predict(ranker_example, &bool_response));
  float float_response;
  EXPECT_FALSE(predictor->PredictScore(ranker_example, &float_response));
}

TEST_F(BinaryClassifierPredictorTest, GenericLogisticRegressionModel) {
  auto ranker_model = std::make_unique<RankerModel>();
  *ranker_model->mutable_proto()->mutable_logistic_regression() =
      GetSimpleLogisticRegressionModel();
  auto predictor = InitPredictor(std::move(ranker_model), GetConfig());
  EXPECT_TRUE(predictor->IsReady());

  RankerExample ranker_example;
  auto& features = *ranker_example.mutable_features();
  features[feature_].set_bool_value(true);
  bool bool_response;
  EXPECT_TRUE(predictor->Predict(ranker_example, &bool_response));
  EXPECT_TRUE(bool_response);
  float float_response;
  EXPECT_TRUE(predictor->PredictScore(ranker_example, &float_response));
  EXPECT_GT(float_response, threshold_);

  features[feature_].set_bool_value(false);
  EXPECT_TRUE(predictor->Predict(ranker_example, &bool_response));
  EXPECT_FALSE(bool_response);
  EXPECT_TRUE(predictor->PredictScore(ranker_example, &float_response));
  EXPECT_LT(float_response, threshold_);
}

TEST_F(BinaryClassifierPredictorTest,
       GenericLogisticRegressionPreprocessedModel) {
  auto ranker_model = std::make_unique<RankerModel>();
  auto& glr = *ranker_model->mutable_proto()->mutable_logistic_regression();
  glr = GetSimpleLogisticRegressionModel();
  glr.clear_weights();
  glr.set_is_preprocessed_model(true);
  (*glr.mutable_fullname_weights())[feature_] = weight_;

  auto predictor = InitPredictor(std::move(ranker_model), GetConfig());
  EXPECT_TRUE(predictor->IsReady());

  RankerExample ranker_example;
  auto& features = *ranker_example.mutable_features();
  features[feature_].set_bool_value(true);
  bool bool_response;
  EXPECT_TRUE(predictor->Predict(ranker_example, &bool_response));
  EXPECT_TRUE(bool_response);
  float float_response;
  EXPECT_TRUE(predictor->PredictScore(ranker_example, &float_response));
  EXPECT_GT(float_response, threshold_);

  features[feature_].set_bool_value(false);
  EXPECT_TRUE(predictor->Predict(ranker_example, &bool_response));
  EXPECT_FALSE(bool_response);
  EXPECT_TRUE(predictor->PredictScore(ranker_example, &float_response));
  EXPECT_LT(float_response, threshold_);
}

TEST_F(BinaryClassifierPredictorTest,
       GenericLogisticRegressionPreprocessedModelReplacedThreshold) {
  auto ranker_model = std::make_unique<RankerModel>();
  auto& glr = *ranker_model->mutable_proto()->mutable_logistic_regression();
  glr = GetSimpleLogisticRegressionModel();
  glr.clear_weights();
  glr.set_is_preprocessed_model(true);
  (*glr.mutable_fullname_weights())[feature_] = weight_;

  float high_threshold = 0.9;  // Some high threshold.
  auto predictor =
      InitPredictor(std::move(ranker_model), GetConfig(high_threshold));
  EXPECT_TRUE(predictor->IsReady());

  RankerExample ranker_example;
  auto& features = *ranker_example.mutable_features();
  features[feature_].set_bool_value(true);
  bool bool_response;
  EXPECT_TRUE(predictor->Predict(ranker_example, &bool_response));
  EXPECT_FALSE(bool_response);
  float float_response;
  EXPECT_TRUE(predictor->PredictScore(ranker_example, &float_response));
  EXPECT_GT(float_response, threshold_);
  EXPECT_LT(float_response, high_threshold);
}

}  // namespace assist_ranker
