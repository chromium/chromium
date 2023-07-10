// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ML_WEBNN_GRAPH_VALIDATION_UTILS_H_
#define COMPONENTS_ML_WEBNN_GRAPH_VALIDATION_UTILS_H_

#include <vector>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace webnn {

// The struct defined in this file need to be synced with,
// - "services/webnn/public/mojom/webnn_graph.mojom"
//
// Represents the `MLOperand` which describes not only input and constant
// operand, but also the output operand of operator.
struct Operand {
  // Represents the `MLOperandType` in the WebIDL definition.
  enum DataType {
    kFloat32,
    kFloat16,
    kInt32,
    kUint32,
    kInt8,
    kUint8,
  };

  Operand(DataType data_type, std::vector<uint32_t> dimensions);
  // Used for converting MLOperand to the component::Operand.
  Operand(DataType data_type, base::span<const uint32_t> dimensions);
  ~Operand();

  Operand(Operand&& other);
  Operand& operator=(Operand&& other);

  bool operator==(const Operand& other) const;
  bool operator!=(const Operand& other) const;

  Operand(const Operand&) = delete;
  Operand& operator=(const Operand&) = delete;

  // The data type of the operand.
  DataType data_type;
  // The dimensions of the operand.
  std::vector<uint32_t> dimensions;
};

// Validate softmax operator defined in WebIDL here
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-softmax
base::expected<Operand, std::string> ValidateSoftmaxAndInferOutput(
    Operand input);

base::expected<size_t, std::string> ValidateAndCalculateElementsNumber(
    base::span<const uint32_t> dimensions);

base::expected<size_t, std::string> ValidateAndCalculateByteLength(
    size_t type_bytes,
    base::span<const uint32_t> dimensions);

// Broadcast the input shapes and return the output shape.
// If bidirectional is true, its behavior follows the numpy-broadcasting-rule:
// https://numpy.org/doc/stable/user/basics.broadcasting.html#general-broadcasting-rules.
// Otherwise, it unidirectionally broadcasts the lhs to the rhs.
absl::optional<std::vector<uint32_t>> BroadcastShapes(
    base::span<const uint32_t> dims_lhs,
    base::span<const uint32_t> dims_rhs,
    bool bidirectional = true);

}  // namespace webnn

#endif  // COMPONENTS_ML_WEBNN_GRAPH_VALIDATION_UTILS_H_
