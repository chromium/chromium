// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_ASSIST_RANKER_QUANTIZED_NN_CLASSIFIER_H_
#define COMPONENTS_ASSIST_RANKER_QUANTIZED_NN_CLASSIFIER_H_

#include "components/assist_ranker/proto/nn_classifier.pb.h"
#include "components/assist_ranker/proto/quantized_nn_classifier.pb.h"

namespace assist_ranker {
namespace quantized_nn_classifier {

// Verifies that the dimensions and quantization high / low values are valid.
// Returns true if value, false otherwise.
bool Validate(const QuantizedNNClassifierModel& quantized);

// Dequantizes the weights and biases in a quantized NN classifier model. This
// must be done before inferencing.
NNClassifierModel Dequantize(const QuantizedNNClassifierModel& quantized);

}  // namespace quantized_nn_classifier
}  // namespace assist_ranker

#endif  // COMPONENTS_ASSIST_RANKER_QUANTIZED_NN_CLASSIFIER_H_
