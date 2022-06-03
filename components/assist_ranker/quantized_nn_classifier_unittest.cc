// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/assist_ranker/quantized_nn_classifier.h"
#include "components/assist_ranker/nn_classifier.h"
#include "components/assist_ranker/nn_classifier_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace assist_ranker {
namespace quantized_nn_classifier {
namespace {

using ::google::protobuf::RepeatedFieldBackInserter;
using ::google::protobuf::RepeatedPtrField;
using ::std::copy;
using ::std::vector;

void CreateLayer(const vector<int>& biases,
                 const vector<vector<int>>& weights,
                 float low,
                 float high,
                 QuantizedNNLayer* layer) {
  layer->set_biases(std::string(biases.begin(), biases.end()));

  for (const auto& i : weights) {
    layer->mutable_weights()->Add(std::string(i.begin(), i.end()));
  }
  layer->set_low(low);
  layer->set_high(high);
}

// Creates a QuantizedDNNClassifierModel proto using a trained set of biases and
// weights.
QuantizedNNClassifierModel CreateModel(
    const vector<int>& hidden_biases,
    const vector<vector<int>>& hidden_weights,
    const vector<int>& logits_biases,
    const vector<vector<int>>& logits_weights,
    float low,
    float high) {
  QuantizedNNClassifierModel model;
  CreateLayer(hidden_biases, hidden_weights, low, high,
              model.mutable_hidden_layer());
  CreateLayer(logits_biases, logits_weights, low, high,
              model.mutable_logits_layer());
  return model;
}

TEST(QuantizedNNClassifierTest, Dequantize) {
  const QuantizedNNClassifierModel quantized = CreateModel(
      // Hidden biases.
      {{8, 16, 32}},
      // Hidden weights.
      {{2, 4, 6}, {10, 4, 8}},
      // Logits biases.
      {2},
      // Logits weights.
      {{4}, {2}, {6}},
      // Low.
      0,
      // High.
      128);

  ASSERT_TRUE(Validate(quantized));
  const NNClassifierModel model = Dequantize(quantized);
  const NNClassifierModel expected = nn_classifier::CreateModel(
      // Hidden biases.
      {{4, 8, 16}},
      // Hidden weights.
      {{1, 2, 3}, {5, 2, 4}},
      // Logits biases.
      {1},
      // Logits weights.
      {{2}, {1}, {3}});
  EXPECT_EQ(model.SerializeAsString(), expected.SerializeAsString());
}

TEST(QuantizedNNClassifierTest, XorTest) {
  // Creates a NN with a single hidden layer of 5 units that solves XOR.
  // Creates a QuantizedDNNClassifier model containing the trained biases and
  // weights.
  const QuantizedNNClassifierModel quantized = CreateModel(
      // Hidden biases.
      {{110, 139, 175, 55, 106}},
      // Hidden weights.
      {{228, 127, 97, 217, 158}, {55, 219, 80, 199, 152}},
      // Logits biases.
      {74},
      // Logits weights.
      {{255}, {211}, {53}, {0}, {86}},
      // Low.
      -2.96390629,
      // High.
      2.8636384);

  ASSERT_TRUE(Validate(quantized));
  const NNClassifierModel model = Dequantize(quantized);
  ASSERT_TRUE(nn_classifier::Validate(model));

  EXPECT_TRUE(nn_classifier::CheckInference(model, {0, 0}, {-2.7032}));
  EXPECT_TRUE(nn_classifier::CheckInference(model, {0, 1}, {2.80681}));
  EXPECT_TRUE(nn_classifier::CheckInference(model, {1, 0}, {2.64435}));
  EXPECT_TRUE(nn_classifier::CheckInference(model, {1, 1}, {-3.17825}));
}

TEST(QuantizedNNClassifierTest, ValidateQuantizedNNClassifierModel) {
  // Empty model.
  QuantizedNNClassifierModel model;
  EXPECT_FALSE(Validate(model));

  // Valid model.
  model = CreateModel({0, 0, 0}, {{0, 0, 0}, {0, 0, 0}}, {0}, {{0}, {0}, {0}},
                      0, 1);
  EXPECT_TRUE(Validate(model));

  // Hidden bias incorrect size.
  model =
      CreateModel({0, 0}, {{0, 0, 0}, {0, 0, 0}}, {0}, {{0}, {0}, {0}}, 0, 1);
  EXPECT_FALSE(Validate(model));

  // Hidden weight vector incorrect size.
  model =
      CreateModel({0, 0, 0}, {{0, 0, 0}, {0, 0}}, {0}, {{0}, {0}, {0}}, 0, 1);
  EXPECT_FALSE(Validate(model));

  // Logits weights incorrect size.
  model = CreateModel({0, 0, 0}, {{0, 0, 0}, {0, 0, 0}}, {0}, {{0}, {0}}, 0, 1);
  EXPECT_FALSE(Validate(model));

  // Empty logits bias.
  model =
      CreateModel({0, 0, 0}, {{0, 0, 0}, {0, 0, 0}}, {}, {{0}, {0}, {0}}, 0, 1);
  EXPECT_FALSE(Validate(model));

  // Low / high incorrect.
  model = CreateModel({0, 0, 0}, {{0, 0, 0}, {0, 0, 0}}, {0}, {{0}, {0}, {0}},
                      1, 0);
  EXPECT_FALSE(Validate(model));
}

}  // namespace
}  // namespace quantized_nn_classifier
}  // namespace assist_ranker
