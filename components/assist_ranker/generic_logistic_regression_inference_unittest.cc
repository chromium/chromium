// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/assist_ranker/generic_logistic_regression_inference.h"
#include "components/assist_ranker/example_preprocessing.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/map.h"

namespace assist_ranker {
using ::google::protobuf::Map;

class GenericLogisticRegressionInferenceTest : public testing::Test {
 protected:
  GenericLogisticRegressionModel GetProto() {
    GenericLogisticRegressionModel proto;
    proto.set_bias(bias_);
    proto.set_threshold(threshold_);

    auto& weights = *proto.mutable_weights();
    weights[scalar1_name_].set_scalar(scalar1_weight_);
    weights[scalar2_name_].set_scalar(scalar2_weight_);
    weights[scalar3_name_].set_scalar(scalar3_weight_);

    auto* one_hot_feat = weights[one_hot_name_].mutable_one_hot();
    one_hot_feat->set_default_weight(one_hot_default_weight_);
    (*one_hot_feat->mutable_weights())[one_hot_elem1_name_] =
        one_hot_elem1_weight_;
    (*one_hot_feat->mutable_weights())[one_hot_elem2_name_] =
        one_hot_elem2_weight_;
    (*one_hot_feat->mutable_weights())[one_hot_elem3_name_] =
        one_hot_elem3_weight_;

    SparseWeights* sparse_feat = weights[sparse_name_].mutable_sparse();
    sparse_feat->set_default_weight(sparse_default_weight_);
    (*sparse_feat->mutable_weights())[sparse_elem1_name_] =
        sparse_elem1_weight_;
    (*sparse_feat->mutable_weights())[sparse_elem2_name_] =
        sparse_elem2_weight_;

    BucketizedWeights* bucketized_feat =
        weights[bucketized_name_].mutable_bucketized();
    bucketized_feat->set_default_weight(bucketization_default_weight_);
    for (const float boundary : bucketization_boundaries_) {
      bucketized_feat->add_boundaries(boundary);
    }
    for (const float weight : bucketization_weights_) {
      bucketized_feat->add_weights(weight);
    }

    return proto;
  }

  const std::string scalar1_name_ = "scalar_feature1";
  const std::string scalar2_name_ = "scalar_feature2";
  const std::string scalar3_name_ = "scalar_feature3";
  const std::string one_hot_name_ = "one_hot_feature";
  const std::string one_hot_elem1_name_ = "one_hot_elem1";
  const std::string one_hot_elem2_name_ = "one_hot_elem2";
  const std::string one_hot_elem3_name_ = "one_hot_elem3";
  const float bias_ = 1.5f;
  const float threshold_ = 0.6f;
  const float scalar1_weight_ = 0.8f;
  const float scalar2_weight_ = -2.4f;
  const float scalar3_weight_ = 0.01f;
  const float one_hot_elem1_weight_ = -1.0f;
  const float one_hot_elem2_weight_ = 5.0f;
  const float one_hot_elem3_weight_ = -1.5f;
  const float one_hot_default_weight_ = 10.0f;
  const float epsilon_ = 0.001f;

  const std::string sparse_name_ = "sparse_feature";
  const std::string sparse_elem1_name_ = "sparse_elem1";
  const std::string sparse_elem2_name_ = "sparse_elem2";
  const float sparse_elem1_weight_ = -2.2f;
  const float sparse_elem2_weight_ = 3.1f;
  const float sparse_default_weight_ = 4.4f;

  const std::string bucketized_name_ = "bucketized_feature";
  const float bucketization_boundaries_[2] = {0.3f, 0.7f};
  const float bucketization_weights_[3] = {-1.0f, 1.0f, 3.0f};
  const float bucketization_default_weight_ = -3.3f;
};

TEST_F(GenericLogisticRegressionInferenceTest, BaseTest) {
  auto predictor = GenericLogisticRegressionInference(GetProto());

  RankerExample example;
  auto& features = *example.mutable_features();
  features[scalar1_name_].set_bool_value(true);
  features[scalar2_name_].set_int32_value(42);
  features[scalar3_name_].set_float_value(0.666f);
  features[one_hot_name_].set_string_value(one_hot_elem1_name_);

  float score = predictor.PredictScore(example);
  float expected_score =
      Sigmoid(bias_ + 1.0f * scalar1_weight_ + 42.0f * scalar2_weight_ +
              0.666f * scalar3_weight_ + one_hot_elem1_weight_);
  EXPECT_NEAR(expected_score, score, epsilon_);
  EXPECT_EQ(expected_score >= threshold_, predictor.Predict(example));
}

TEST_F(GenericLogisticRegressionInferenceTest, UnknownElement) {
  RankerExample example;
  auto& features = *example.mutable_features();
  features[one_hot_name_].set_string_value("Unknown element");

  auto predictor = GenericLogisticRegressionInference(GetProto());
  float score = predictor.PredictScore(example);
  float expected_score = Sigmoid(bias_ + one_hot_default_weight_);
  EXPECT_NEAR(expected_score, score, epsilon_);
}

TEST_F(GenericLogisticRegressionInferenceTest, MissingFeatures) {
  RankerExample example;

  auto predictor = GenericLogisticRegressionInference(GetProto());
  float score = predictor.PredictScore(example);
  // Missing features will use default weights for one_hot features and drop
  // scalar features.
  float expected_score = Sigmoid(bias_ + one_hot_default_weight_);
  EXPECT_NEAR(expected_score, score, epsilon_);
}

TEST_F(GenericLogisticRegressionInferenceTest, UnknownFeatures) {
  RankerExample example;
  auto& features = *example.mutable_features();
  features["foo1"].set_bool_value(true);
  features["foo2"].set_int32_value(42);
  features["foo3"].set_float_value(0.666f);
  features["foo4"].set_string_value(one_hot_elem1_name_);
  // All features except this one will be ignored.
  features[one_hot_name_].set_string_value(one_hot_elem2_name_);

  auto predictor = GenericLogisticRegressionInference(GetProto());
  float score = predictor.PredictScore(example);
  // Unknown features will be ignored.
  float expected_score = Sigmoid(bias_ + one_hot_elem2_weight_);
  EXPECT_NEAR(expected_score, score, epsilon_);
}

TEST_F(GenericLogisticRegressionInferenceTest, Threshold) {
  // In this test, we calculate the score for a given example and set the model
  // threshold to this value. We then add a feature to the example that should
  // tip the score slightly on either side of the treshold and verify that the
  // decision is as expected.

  auto proto = GetProto();
  auto threshold_calculator = GenericLogisticRegressionInference(proto);

  RankerExample example;
  auto& features = *example.mutable_features();
  features[scalar1_name_].set_bool_value(true);
  features[scalar2_name_].set_int32_value(2);
  features[one_hot_name_].set_string_value(one_hot_elem1_name_);

  float threshold = threshold_calculator.PredictScore(example);
  proto.set_threshold(threshold);

  // Setting the model with the calculated threshold.
  auto predictor = GenericLogisticRegressionInference(proto);

  // Adding small positive contribution from scalar3 to tip the decision the
  // positive side of the threshold.
  features[scalar3_name_].set_float_value(0.01f);
  float score = predictor.PredictScore(example);
  // The score is now greater than, but still near the threshold. The
  // decision should be positive.
  EXPECT_LT(threshold, score);
  EXPECT_NEAR(threshold, score, epsilon_);
  EXPECT_TRUE(predictor.Predict(example));

  // A small negative contribution from scalar3 should tip the decision the
  // other way.
  features[scalar3_name_].set_float_value(-0.01f);
  score = predictor.PredictScore(example);
  EXPECT_GT(threshold, score);
  EXPECT_NEAR(threshold, score, epsilon_);
  EXPECT_FALSE(predictor.Predict(example));
}

TEST_F(GenericLogisticRegressionInferenceTest, NoThreshold) {
  auto proto = GetProto();
  // When no threshold is specified, we use the default of 0.5.
  proto.clear_threshold();
  auto predictor = GenericLogisticRegressionInference(proto);

  RankerExample example;
  auto& features = *example.mutable_features();
  // one_hot_elem3 exactly balances the bias, so we expect the pre-sigmoid score
  // to be zero, and the post-sigmoid score to be 0.5 if this is the only active
  // feature.
  features[one_hot_name_].set_string_value(one_hot_elem3_name_);
  float score = predictor.PredictScore(example);
  EXPECT_NEAR(0.5f, score, epsilon_);

  // Adding small contribution from scalar3 to tip the decision on one side or
  // the other of the threshold.
  features[scalar3_name_].set_float_value(0.01f);
  score = predictor.PredictScore(example);
  // The score is now greater than, but still near 0.5. The decision should be
  // positive.
  EXPECT_LT(0.5f, score);
  EXPECT_NEAR(0.5f, score, epsilon_);
  EXPECT_TRUE(predictor.Predict(example));

  features[scalar3_name_].set_float_value(-0.01f);
  score = predictor.PredictScore(example);
  // The score is now lower than, but near 0.5. The decision should be
  // negative.
  EXPECT_GT(0.5f, score);
  EXPECT_NEAR(0.5f, score, epsilon_);
  EXPECT_FALSE(predictor.Predict(example));
}

TEST_F(GenericLogisticRegressionInferenceTest, PreprossessedModel) {
  GenericLogisticRegressionModel proto = GetProto();
  proto.set_is_preprocessed_model(true);
  // Clear the weights to make sure the inference is done by fullname_weights.
  proto.clear_weights();

  // Build fullname weights.
  Map<std::string, float>& weights = *proto.mutable_fullname_weights();
  weights[scalar1_name_] = scalar1_weight_;
  weights[scalar2_name_] = scalar2_weight_;
  weights[scalar3_name_] = scalar3_weight_;
  weights[ExamplePreprocessor::FeatureFullname(
      one_hot_name_, one_hot_elem1_name_)] = one_hot_elem1_weight_;
  weights[ExamplePreprocessor::FeatureFullname(
      one_hot_name_, one_hot_elem2_name_)] = one_hot_elem2_weight_;
  weights[ExamplePreprocessor::FeatureFullname(
      one_hot_name_, one_hot_elem3_name_)] = one_hot_elem3_weight_;
  weights[ExamplePreprocessor::FeatureFullname(
      sparse_name_, sparse_elem1_name_)] = sparse_elem1_weight_;
  weights[ExamplePreprocessor::FeatureFullname(
      sparse_name_, sparse_elem2_name_)] = sparse_elem2_weight_;
  weights[ExamplePreprocessor::FeatureFullname(bucketized_name_, "0")] =
      bucketization_weights_[0];
  weights[ExamplePreprocessor::FeatureFullname(bucketized_name_, "1")] =
      bucketization_weights_[1];
  weights[ExamplePreprocessor::FeatureFullname(bucketized_name_, "2")] =
      bucketization_weights_[2];
  weights[ExamplePreprocessor::FeatureFullname(
      ExamplePreprocessor::kMissingFeatureDefaultName, one_hot_name_)] =
      one_hot_default_weight_;
  weights[ExamplePreprocessor::FeatureFullname(
      ExamplePreprocessor::kMissingFeatureDefaultName, sparse_name_)] =
      sparse_default_weight_;
  weights[ExamplePreprocessor::FeatureFullname(
      ExamplePreprocessor::kMissingFeatureDefaultName, bucketized_name_)] =
      bucketization_default_weight_;

  // Build preprocessor_config.
  ExamplePreprocessorConfig& config = *proto.mutable_preprocessor_config();
  config.add_missing_features(one_hot_name_);
  config.add_missing_features(sparse_name_);
  config.add_missing_features(bucketized_name_);
  (*config.mutable_bucketizers())[bucketized_name_].add_boundaries(
      bucketization_boundaries_[0]);
  (*config.mutable_bucketizers())[bucketized_name_].add_boundaries(
      bucketization_boundaries_[1]);

  auto predictor = GenericLogisticRegressionInference(proto);

  // Build example.
  RankerExample example;
  Map<std::string, Feature>& features = *example.mutable_features();
  features[scalar1_name_].set_bool_value(true);
  features[scalar2_name_].set_int32_value(42);
  features[scalar3_name_].set_float_value(0.666f);
  features[one_hot_name_].set_string_value(one_hot_elem1_name_);
  features[sparse_name_].mutable_string_list()->add_string_value(
      sparse_elem1_name_);
  features[sparse_name_].mutable_string_list()->add_string_value(
      sparse_elem2_name_);
  features[bucketized_name_].set_float_value(0.98f);

  // Inference.
  float score = predictor.PredictScore(example);
  float expected_score = Sigmoid(
      bias_ + 1.0f * scalar1_weight_ + 42.0f * scalar2_weight_ +
      0.666f * scalar3_weight_ + one_hot_elem1_weight_ + sparse_elem1_weight_ +
      sparse_elem2_weight_ + bucketization_weights_[2]);

  EXPECT_NEAR(expected_score, score, epsilon_);
  EXPECT_EQ(expected_score >= threshold_, predictor.Predict(example));
}

}  // namespace assist_ranker
