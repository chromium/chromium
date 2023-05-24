// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ml/webnn/graph_validation_utils.h"

#include "base/check_op.h"
#include "base/numerics/checked_math.h"

namespace webnn {

base::expected<size_t, std::string> ValidateAndCalculateElementsNumber(
    base::span<const uint32_t> dimensions) {
  if (dimensions.empty()) {
    return base::unexpected("The dimensions is empty.");
  }
  base::CheckedNumeric<size_t> checked_number_of_elements = 1;
  for (auto& d : dimensions) {
    if (d == 0) {
      return base::unexpected("All dimensions should be positive.");
    }
    checked_number_of_elements *= d;
  }
  if (!checked_number_of_elements.IsValid()) {
    return base::unexpected("The number of elements is too large.");
  }
  return checked_number_of_elements.ValueOrDie();
}

base::expected<size_t, std::string> ValidateAndCalculateByteLength(
    size_t type_bytes,
    base::span<const uint32_t> dimensions) {
  auto elements_num_result = ValidateAndCalculateElementsNumber(dimensions);
  if (!elements_num_result.has_value()) {
    return elements_num_result;
  }
  auto checked_byte_length =
      base::MakeCheckedNum<size_t>(elements_num_result.value()) * type_bytes;
  if (!checked_byte_length.IsValid()) {
    return base::unexpected("The byte length is too large.");
  }
  return checked_byte_length.ValueOrDie();
}

absl::optional<std::vector<uint32_t>> BroadcastShapes(
    base::span<const uint32_t> dims_lhs,
    base::span<const uint32_t> dims_rhs,
    bool bidirectional) {
  // If bidirectional is true, the rank of the output shape is the maximum rank
  // of the input shapes. Otherwise it is as the same as the rhs' rank.
  auto rank_lhs = dims_lhs.size(), rank_rhs = dims_rhs.size();
  auto rank_output = bidirectional ? std::max(rank_lhs, rank_rhs) : rank_rhs;
  std::vector<uint32_t> dims_output(rank_output);
  for (size_t i = 0; i < rank_output; ++i) {
    auto dim_lhs = i < rank_lhs ? dims_lhs[rank_lhs - i - 1] : 1;
    DCHECK_GT(dim_lhs, static_cast<uint32_t>(0));
    auto dim_rhs = i < rank_rhs ? dims_rhs[rank_rhs - i - 1] : 1;
    DCHECK_GT(dim_rhs, static_cast<uint32_t>(0));
    // If bidirectional is true, two dimensions are compatible when they are
    // equal, or one of them is 1. Otherwise, two dimensions are compatible when
    // they are equal, or the lhs dimension is 1.
    if (bidirectional) {
      if (dim_lhs != dim_rhs && dim_lhs != 1 && dim_rhs != 1) {
        return absl::nullopt;
      }
    } else if (dim_lhs != dim_rhs && dim_lhs != 1) {
      return absl::nullopt;
    }
    // If bidirectional is true, for each dimension of the output tensor, its
    // size is the maximum size along that dimension of the input shapes.
    // Otherwise, its size is the same as the rhs.
    dims_output[rank_output - i - 1] =
        bidirectional ? std::max(dim_lhs, dim_rhs) : dim_rhs;
  }
  return dims_output;
}

}  // namespace webnn
