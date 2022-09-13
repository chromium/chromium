// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/assist_ranker/quantized_nn_classifier.h"

#include "components/assist_ranker/nn_classifier.h"

namespace assist_ranker {
namespace quantized_nn_classifier {
namespace {

// Dequantized a set of unsigned 8-bit weights using the specified scaling
// factor and base value.
void DequantizeVector(const std::string& s,
                      float scale,
                      float low,
                      FloatVector* v) {
  for (const unsigned char ch : s) {
    v->mutable_values()->Add(scale * ch + low);
  }
}

// Dequantizes a quantized NN layer.
void DequantizeLayer(const QuantizedNNLayer& quantized, NNLayer* layer) {
  const float low = quantized.low();
  const float scale = (quantized.high() - low) / 256;
  DequantizeVector(quantized.biases(), scale, low, layer->mutable_biases());
  for (const std::string& s : quantized.weights()) {
    auto* p = layer->mutable_weights()->Add();
    DequantizeVector(s, scale, low, p);
  }
}

bool ValidateLayer(const QuantizedNNLayer& layer) {
  // The quantization low value must always be less than the high value.
  return layer.low() < layer.high();
}

}  // namespace

NNClassifierModel Dequantize(const QuantizedNNClassifierModel& quantized) {
  NNClassifierModel model;
  DequantizeLayer(quantized.hidden_layer(), model.mutable_hidden_layer());
  DequantizeLayer(quantized.logits_layer(), model.mutable_logits_layer());
  return model;
}

bool Validate(const QuantizedNNClassifierModel& quantized) {
  if (!ValidateLayer(quantized.hidden_layer()) ||
      !ValidateLayer(quantized.logits_layer())) {
    return false;
  }

  return nn_classifier::Validate(Dequantize(quantized));
}

}  // namespace quantized_nn_classifier
}  // namespace assist_ranker
