// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_ASSIST_RANKER_NN_CLASSIFIER_H_
#define COMPONENTS_ASSIST_RANKER_NN_CLASSIFIER_H_

#include <vector>

#include "components/assist_ranker/proto/nn_classifier.pb.h"

namespace assist_ranker {
namespace nn_classifier {

// Implements inference for a neural network model trained using
// tf.contrib.learn.DNNClassifier. The network has a single hidden layer
// with tf.nn.relu as the activation function. The output logits layer has no
// activation function.
//
// Returns a vector of scores for each class in the range -INF to +INF.
std::vector<float> Inference(const NNClassifierModel& model,
                             const std::vector<float>& input);

// Validates that the dimensions of the biases and weights in an
// NNClassifierModel are valid. Returns true if the model is valid, false
// otherwise.
bool Validate(const NNClassifierModel& model);

}  // namespace nn_classifier
}  // namespace assist_ranker

#endif  // COMPONENTS_ASSIST_RANKER_NN_CLASSIFIER_H_
