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

// Represents the `MLInputOperandLayout` that specifies the layout format of
// the input tensor. N is the batch, C is input channels, H is height and W is
// the width of the tensor.
enum InputOperandLayout { kNchw, kNhwc };

// Represents the `MLConv2dFilterOperandLayout` that specifies the layout format
// of the filter tensor. O is output channels, I is input channels, H is height
// and W is the width of filter.
enum Conv2dFilterOperandLayout { kOihw, kHwio, kOhwi, kIhwo };

// Represents the `MLAutoPad`. `Explicit` means that the values in the padding
// array should be used for calculating input padding, the `SameUpper` and
// `SameLower` options mean the padding values are automatically computed.
enum AutoPad { kExplicit, kSameUpper, kSameLower };

// Represents the `MLRoundingType` that is used to compute the output shape.
enum RoundingType { kFloor, kCeil };

// A size has height and width values.
struct Size2d {
  uint32_t height;
  uint32_t width;
};

// The additional rows and columns added to the beginning and ending of each
// spatial dimension of input.
struct Padding2d {
  // The height and width padding at the beginning of input tensor.
  Size2d beginning;
  // The height and width padding at the ending of input tensor.
  Size2d ending;
};

// Contains the attributes of conv2d operator.
struct Conv2dAttributes {
  Conv2dAttributes();
  ~Conv2dAttributes();

  Conv2dAttributes(Conv2dAttributes&& other);
  Conv2dAttributes& operator=(Conv2dAttributes&& other);

  Conv2dAttributes(const Conv2dAttributes&) = delete;
  Conv2dAttributes& operator=(const Conv2dAttributes&) = delete;

  // The additional rows and columns added to the beginning and ending of each
  // spatial dimension of input.
  Padding2d padding;
  // The stride of the sliding window for each spatial dimension of input.
  Size2d strides;
  // The dilation factor for each spatial dimension of input.
  Size2d dilations;
  // The automatic input padding options.
  AutoPad auto_pad = AutoPad::kExplicit;
  // The number of groups that input channels and output channels are divided
  // into.
  uint32_t groups = 1;
  // The layout format of the input.
  InputOperandLayout input_layout = InputOperandLayout::kNchw;
  // The layout format of the filter.
  Conv2dFilterOperandLayout filter_layout = Conv2dFilterOperandLayout::kOihw;
  // The additional 1-D tensor with the shape of [output_channels] whose values
  // are to be added to the convolution result.
  absl::optional<Operand> bias_operand;
};

// Contains the attributes of pool2d operator.
struct Pool2dAttributes {
  // The dimensions of the sliding window.
  absl::optional<Size2d> window_dimensions;
  // The additional rows and columns added to the beginning and ending of each
  // spatial dimension of input.
  Padding2d padding;
  // The element stride of the sliding window for each spatial dimension of
  // input.
  Size2d strides;
  // The dilation factor for each spatial dimension of input.
  Size2d dilations;
  // The automatic input padding options.
  AutoPad auto_pad = AutoPad::kExplicit;
  // The layout format of the input.
  InputOperandLayout layout = InputOperandLayout::kNchw;
  // The rounding function used to compute the output shape.
  RoundingType rounding_type = RoundingType::kFloor;
  // The element height and width of the output tensor.
  absl::optional<Size2d> output_sizes;
};

// Contains the attributes of gemm operator.
struct GemmAttributes {
  GemmAttributes();
  ~GemmAttributes();

  GemmAttributes(GemmAttributes&& other);
  GemmAttributes& operator=(GemmAttributes&& other);

  GemmAttributes(const GemmAttributes&) = delete;
  GemmAttributes& operator=(const GemmAttributes&) = delete;

  // The optional third tensor in expression alpha * A * B + beta * C.
  absl::optional<Operand> c_operand;
  // A float scalar multiplier for the `A * B`.
  float alpha = 1.0;
  // A float scalar multiplier for the third tensor.
  float beta = 1.0;
  // True is to transpose the first tensor matrix multiplication.
  bool a_transpose = false;
  // True is to transpose the second tensor matrix multiplication.
  bool b_transpose = false;
};

// Validate softmax operator defined in WebIDL here
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-softmax
base::expected<Operand, std::string> ValidateSoftmaxAndInferOutput(
    Operand input);

// Validate and infer output information of 2-D convolution operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-conv2d
base::expected<Operand, std::string> ValidateConv2dAndInferOutput(
    const Operand& input,
    const Operand& filter,
    const Conv2dAttributes& attributes);

// Validate a mean, L2 norm, or max reduction operator defined in WebIDL here
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-pool2d
base::expected<Operand, std::string> ValidatePool2dAndInferOutput(
    const Operand& input,
    const Pool2dAttributes& attributes);

// Validate gemm operator defined in WebIDL here
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-gemm
base::expected<Operand, std::string> ValidateGemmAndInferOutput(
    const Operand& a,
    const Operand& b,
    const GemmAttributes& attributes);

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

// TODO(crbug.com/1273291): Don't export PaddingSizes when moving the validation
// of ConvTransposed2d to the shared library.
struct PaddingSizes {
  uint32_t begin;
  uint32_t end;
};

// Calculate the effective padding for conv2d based on WebNN auto padding
// rules.
//
// TODO(crbug.com/1273291): Add the link to WebNN spec's algorithm once it is
// defined, tracked by: https://github.com/webmachinelearning/webnn/issues/326
absl::optional<PaddingSizes> CalculateConv2dPadding(AutoPad auto_pad,
                                                    const uint32_t input_size,
                                                    const uint32_t filter_size,
                                                    const uint32_t stride,
                                                    const uint32_t dilation);

}  // namespace webnn

#endif  // COMPONENTS_ML_WEBNN_GRAPH_VALIDATION_UTILS_H_
