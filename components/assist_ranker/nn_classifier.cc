// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/assist_ranker/nn_classifier.h"

#include "base/check_op.h"
#include "components/assist_ranker/proto/nn_classifier.pb.h"

namespace assist_ranker {
namespace nn_classifier {
namespace {

using google::protobuf::RepeatedPtrField;
using std::vector;

vector<float> FeedForward(const NNLayer& layer, const vector<float>& input) {
  const RepeatedPtrField<FloatVector>& weights = layer.weights();
  const FloatVector& biases = layer.biases();

  // Number of nodes in the layer.
  const int num_nodes = biases.values().size();
  // Number of values in the input.
  const int num_input = input.size();
  DCHECK_EQ(weights.size(), num_input);

  // Initialize with the bias.
  vector<float> output(biases.values().begin(), biases.values().end());

  // For each value in the input.
  for (int j = 0; j < num_input; ++j) {
    const FloatVector& v = weights[j];
    DCHECK_EQ(v.values().size(), num_nodes);

    // For each node in the layer.
    for (int i = 0; i < num_nodes; ++i) {
      output[i] += v.values(i) * input[j];
    }
  }
  return output;
}

// Apply ReLU activation function to a vector, which sets all values to
// max(0, value).
void Relu(vector<float>* const v) {
  // We are modifying the vector so the iterator must be a reference.
  for (float& i : *v)
    if (i < 0.0f)
      i = 0.0f;
}

bool ValidateLayer(const NNLayer& layer) {
  // Number of nodes in the layer (must be non-zero).
  const int num_nodes = layer.biases().values().size();
  if (num_nodes == 0)
    return false;

  // Number of values in the input (must be non-zero).
  const int num_input = layer.weights().size();
  if (num_input == 0)
    return false;

  for (int j = 0; j < num_input; ++j) {
    // The size of each weight vector must be the number of nodes in the
    // layer.
    if (layer.weights(j).values().size() != num_nodes)
      return false;
  }

  return true;
}

}  // namespace

bool Validate(const NNClassifierModel& model) {
  // Check the size of the output from the hidden layer is equal to the size
  // of the input in the logits layer.
  if (model.hidden_layer().biases().values().size() !=
      model.logits_layer().weights().size()) {
    return false;
  }

  return ValidateLayer(model.hidden_layer()) &&
         ValidateLayer(model.logits_layer());
}

vector<float> Inference(const NNClassifierModel& model,
                        const vector<float>& input) {
  vector<float> v = FeedForward(model.hidden_layer(), input);
  Relu(&v);
  // Feed forward the logits layer.
  return FeedForward(model.logits_layer(), v);
}

}  // namespace nn_classifier
}  // namespace assist_ranker
