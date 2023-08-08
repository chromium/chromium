// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ml/webnn/graph_validation_utils.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"

namespace webnn {

namespace {

bool IsFloatingPointType(Operand::DataType data_type) {
  switch (data_type) {
    case Operand::DataType::kFloat32:
    case Operand::DataType::kFloat16:
      return true;
    case Operand::DataType::kInt32:
    case Operand::DataType::kUint32:
    case Operand::DataType::kInt8:
    case Operand::DataType::kUint8:
      return false;
  }
  NOTREACHED_NORETURN();
}

struct FloatSize2D {
  double height;
  double width;
};

// Calculate the output size for conv2d based on WebNN spec:
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-conv2d
// Return the calculated output size if no error.
base::expected<double, std::string> CalculateConv2dOutputSize(
    const uint32_t input_size,
    const uint32_t filter_size,
    const uint32_t beginning_padding,
    const uint32_t ending_padding,
    const uint32_t stride,
    const uint32_t dilation) {
  // Calculate the dilated filter sizes.
  auto checked_effective_filter_size =
      (base::MakeCheckedNum<uint32_t>(filter_size) - 1) * dilation + 1;
  if (!checked_effective_filter_size.IsValid()) {
    return base::unexpected("The effective filter size is too large.");
  }

  // Calculate the output size in double precision floating point number that
  // ensures all dimension values of type uint32_t can be exactly represented.
  // https://en.wikipedia.org/wiki/Double-precision_floating-point_format#Precision_limitations_on_integer_values
  // The max value of checked_output_size should be 3 * UINT_MAX + 1,
  // which is smaller than the max safe integer value for double type.
  auto checked_output_size =
      (base::MakeCheckedNum<double>(input_size) -
       checked_effective_filter_size + beginning_padding + ending_padding) /
          stride +
      1;

  if (checked_output_size.ValueOrDie() < 0) {
    return base::unexpected("The input size is too small to fill the window.");
  }

  // Check if the value is valid for rounding to uint32_t type.
  if (!checked_output_size.IsValid<uint32_t>()) {
    return base::unexpected("The output size is too large.");
  }

  return checked_output_size.ValueOrDie();
}

// Validate and calculate the output spatial dimensions of conv2d given
// input sizes, filter sizes, padding, strides and dilations.
// Return the calculated output sizes in double precision floating point number
// if no errors.
base::expected<FloatSize2D, std::string> ValidateAndCalculateConv2dOutputSizes(
    const uint32_t input_height,
    const uint32_t input_width,
    const uint32_t filter_height,
    const uint32_t filter_width,
    const Padding2d& padding,
    const Size2d& strides,
    const Size2d& dilations,
    const AutoPad auto_pad) {
  uint32_t padding_beginning_height = padding.beginning.height;
  uint32_t padding_ending_height = padding.ending.height;
  uint32_t padding_beginning_width = padding.beginning.width;
  uint32_t padding_ending_width = padding.ending.width;

  if (strides.height == 0 || strides.width == 0) {
    return base::unexpected("All strides should be greater than 0.");
  }
  const uint32_t stride_height = strides.height;
  const uint32_t stride_width = strides.width;

  if (dilations.height == 0 || dilations.width == 0) {
    return base::unexpected("All dilations should be greater than 0.");
  }
  const uint32_t dilation_height = dilations.height;
  const uint32_t dilation_width = dilations.width;

  // When the autoPad is other than "explicit", the values in the
  // options.padding array are ignored and the explicit padding values need to
  // be calculated.
  if (auto_pad != AutoPad::kExplicit) {
    auto padding_sizes_height = CalculateConv2dPadding(
        auto_pad, input_height, filter_height, stride_height, dilation_height);
    if (!padding_sizes_height) {
      return base::unexpected(
          "Overflow occurred when calculating the padding along the height "
          "dimension.");
    }
    padding_beginning_height = padding_sizes_height->begin;
    padding_ending_height = padding_sizes_height->end;
    auto padding_sizes_width = CalculateConv2dPadding(
        auto_pad, input_width, filter_width, stride_width, dilation_width);
    if (!padding_sizes_width) {
      return base::unexpected(
          "Overflow occurred when calculating the padding along the width "
          "dimension.");
    }
    padding_beginning_width = padding_sizes_width->begin;
    padding_ending_width = padding_sizes_width->end;
  }

  auto float_output_height = CalculateConv2dOutputSize(
      input_height, filter_height, padding_beginning_height,
      padding_ending_height, stride_height, dilation_height);
  if (!float_output_height.has_value()) {
    return base::unexpected("Failed to calculate the output height: " +
                            float_output_height.error());
  }

  auto float_output_width = CalculateConv2dOutputSize(
      input_width, filter_width, padding_beginning_width, padding_ending_width,
      stride_width, dilation_width);
  if (!float_output_width.has_value()) {
    return base::unexpected("Failed to calculate the output width: " +
                            float_output_width.error());
  }

  return FloatSize2D({.height = float_output_height.value(),
                      .width = float_output_width.value()});
}

}  // namespace

Operand::Operand(DataType data_type, std::vector<uint32_t> dimensions) {
  this->data_type = data_type;
  this->dimensions = std::move(dimensions);
}

Operand::Operand(DataType data_type, base::span<const uint32_t> dimensions) {
  this->data_type = data_type;
  this->dimensions.assign(dimensions.begin(), dimensions.end());
}

Operand::~Operand() = default;

Operand::Operand(Operand&& other) = default;
Operand& Operand::operator=(Operand&& other) = default;

bool Operand::operator==(const Operand& other) const {
  return data_type == other.data_type && dimensions == other.dimensions;
}

bool Operand::operator!=(const Operand& other) const {
  return !(*this == other);
}

base::expected<Operand, std::string> ValidateSoftmaxAndInferOutput(
    Operand input) {
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-softmax, The input must be
  // a 2-D tensor.
  if (input.dimensions.size() != 2) {
    return base::unexpected("The input must be a 2-D tensor.");
  }
  // The input type must be one of the floating point types.
  if (!IsFloatingPointType(input.data_type)) {
    return base::unexpected(
        "The input type must be one of the floating point types.");
  }
  // The output tensor of softmax is the same shape as the input tensor.
  return Operand(input.data_type, std::move(input.dimensions));
}

Conv2dAttributes::Conv2dAttributes() = default;
Conv2dAttributes::~Conv2dAttributes() = default;

Conv2dAttributes::Conv2dAttributes(Conv2dAttributes&& other) = default;
Conv2dAttributes& Conv2dAttributes::operator=(Conv2dAttributes&& other) =
    default;

base::expected<Operand, std::string> ValidateConv2dAndInferOutput(
    const Operand& input,
    const Operand& filter,
    const Conv2dAttributes& attributes) {
  // Validate input operand and set its sizes.
  const auto input_shape = input.dimensions;
  if (input_shape.size() != 4) {
    return base::unexpected("The input should be a 4-D tensor.");
  }
  // The input layout option specifies the layout format of the input tensor.
  uint32_t input_batches, input_channels, input_height, input_width;
  switch (attributes.input_layout) {
    case InputOperandLayout::kNchw:
      // "nchw": [batches, input_channels, height, width]
      input_batches = input_shape[0];
      input_channels = input_shape[1];
      input_height = input_shape[2];
      input_width = input_shape[3];
      break;
    case InputOperandLayout::kNhwc:
      // "nhwc": [batches, height, width, input_channels]
      input_batches = input_shape[0];
      input_height = input_shape[1];
      input_width = input_shape[2];
      input_channels = input_shape[3];
      break;
  }

  // Validate filter operand and set its sizes.
  if (filter.data_type != input.data_type) {
    return base::unexpected("The filter type doesn't match the input type.");
  }
  const auto filter_shape = filter.dimensions;
  if (filter_shape.size() != 4) {
    return base::unexpected("The filter should be a 4-D tensor.");
  }
  // The filter layout specifies the filter layout format.
  uint32_t filter_height, filter_width, output_channels, filter_input_channels;
  switch (attributes.filter_layout) {
    case Conv2dFilterOperandLayout::kHwio:
      // "hwio": [height, width, input_channels/groups, output_channels]
      filter_height = filter_shape[0];
      filter_width = filter_shape[1];
      filter_input_channels = filter_shape[2];
      output_channels = filter_shape[3];
      break;
    case Conv2dFilterOperandLayout::kOhwi:
      // "ohwi": [output_channels, height, width, input_channels/groups]
      output_channels = filter_shape[0];
      filter_height = filter_shape[1];
      filter_width = filter_shape[2];
      filter_input_channels = filter_shape[3];
      break;
    case Conv2dFilterOperandLayout::kIhwo:
      // "ihwo": [input_channels/groups, height, width, output_channels]
      filter_input_channels = filter_shape[0];
      filter_height = filter_shape[1];
      filter_width = filter_shape[2];
      output_channels = filter_shape[3];
      break;
    case Conv2dFilterOperandLayout::kOihw:
      // "oihw": [output_channels, input_channels/groups, height, width]
      output_channels = filter_shape[0];
      filter_input_channels = filter_shape[1];
      filter_height = filter_shape[2];
      filter_width = filter_shape[3];
      break;
  }
  // Validate bias operand if it is present.
  if (attributes.bias_operand) {
    const auto bias_shape = attributes.bias_operand->dimensions;
    if (bias_shape.size() != 1) {
      return base::unexpected("The bias should be a 1-D tensor.");
    }
    if (bias_shape[0] != output_channels) {
      return base::unexpected(base::StringPrintf(
          "The bias shape should be [%u].", output_channels));
    }
    if (attributes.bias_operand->data_type != input.data_type) {
      return base::unexpected("The bias type doesn't match input type.");
    }
  }
  // Validate groups.
  if (attributes.groups == 0) {
    return base::unexpected("The groups should be greater than 0.");
  }
  if (input_channels % attributes.groups != 0 ||
      filter_input_channels != input_channels / attributes.groups) {
    return base::unexpected(
        "The groups must evenly divide the input channels to filter input "
        "channels.");
  }

  const auto output_sizes = ValidateAndCalculateConv2dOutputSizes(
      input_height, input_width, filter_height, filter_width,
      attributes.padding, attributes.strides, attributes.dilations,
      attributes.auto_pad);
  if (!output_sizes.has_value()) {
    return base::unexpected(output_sizes.error());
  }
  const uint32_t output_height =
      base::ClampFloor<uint32_t>(output_sizes->height);
  const uint32_t output_width = base::ClampFloor<uint32_t>(output_sizes->width);
  // The input layout option specifies the layout format of the output tensor.
  std::vector<uint32_t> output_shape;
  switch (attributes.input_layout) {
    case InputOperandLayout::kNchw:
      // "nchw": [batches, output_channels, height, width]
      output_shape = {input_batches, output_channels, output_height,
                      output_width};
      break;
    case InputOperandLayout::kNhwc:
      // "nhwc": [batches, height, width, output_channels]
      output_shape = {input_batches, output_height, output_width,
                      output_channels};
      break;
  }

  return Operand(input.data_type, std::move(output_shape));
}

base::expected<Operand, std::string> ValidatePool2dAndInferOutput(
    const Operand& input,
    const Pool2dAttributes& attributes) {
  // Validate input operand and set its sizes.
  const auto input_shape = input.dimensions;
  if (input_shape.size() != 4) {
    return base::unexpected("The input should be a 4-D tensor.");
  }
  // The layout option specifies the layout format of the input tensor.
  uint32_t input_batches, input_channels, input_height, input_width;
  switch (attributes.layout) {
    case InputOperandLayout::kNchw:
      // "nchw": [batches, channels, height, width]
      input_batches = input_shape[0];
      input_channels = input_shape[1];
      input_height = input_shape[2];
      input_width = input_shape[3];
      break;
    case InputOperandLayout::kNhwc:
      // "nhwc": [batches, height, width, channels]
      input_batches = input_shape[0];
      input_height = input_shape[1];
      input_width = input_shape[2];
      input_channels = input_shape[3];
      break;
  }

  // Validate windowDimensions and get its values. If not present, the window
  // dimensions are assumed to be the height and width dimensions of the input
  // shape.
  uint32_t window_height = input_height;
  uint32_t window_width = input_width;
  if (attributes.window_dimensions) {
    if (attributes.window_dimensions->height == 0 ||
        attributes.window_dimensions->width == 0) {
      return base::unexpected(
          "All window dimensions should be greater than 0.");
    }
    window_height = attributes.window_dimensions->height;
    window_width = attributes.window_dimensions->width;
  }

  // Reuse ValidateAndCalculateConv2dOutputSizes to calculate pool2d output
  // sizes.
  const auto output_sizes = ValidateAndCalculateConv2dOutputSizes(
      input_height, input_width, window_height, window_width,
      attributes.padding, attributes.strides, attributes.dilations,
      attributes.auto_pad);
  if (!output_sizes.has_value()) {
    return base::unexpected(output_sizes.error());
  }
  const uint32_t floor_output_height =
      base::ClampFloor<uint32_t>(output_sizes->height);
  const uint32_t ceil_output_height =
      base::ClampCeil<uint32_t>(output_sizes->height);
  const uint32_t floor_output_width =
      base::ClampFloor<uint32_t>(output_sizes->width);
  const uint32_t ceil_output_width =
      base::ClampCeil<uint32_t>(output_sizes->width);

  uint32_t output_height, output_width;
  if (attributes.output_sizes) {
    auto& output_size = attributes.output_sizes.value();
    if (output_size.height == 0 || output_size.width == 0) {
      return base::unexpected("All output sizes should be greater than 0.");
    }
    uint32_t user_output_height = output_size.height;
    uint32_t user_output_width = output_size.width;

    // Check whether the user supplied output sizes is either floor or ceil
    // rounding of the calculated output sizes. The backend implementation
    // should check whether the indicated rounding type is supported.
    if ((user_output_height == floor_output_height &&
         user_output_width == floor_output_width) ||
        (user_output_height == ceil_output_height &&
         user_output_width == ceil_output_width)) {
      output_height = user_output_height;
      output_width = user_output_width;
    } else {
      return base::unexpected(
          (floor_output_height == ceil_output_height &&
           floor_output_width == ceil_output_width)
              ? base::StringPrintf("The output sizes should be [%u, %u].",
                                   floor_output_height, floor_output_width)
              : base::StringPrintf(
                    "The output sizes should be either [%u, %u] or [%u, %u].",
                    floor_output_height, floor_output_width, ceil_output_height,
                    ceil_output_width));
    }
  } else {
    switch (attributes.rounding_type) {
      case RoundingType::kFloor:
        output_height = floor_output_height;
        output_width = floor_output_width;
        break;
      case RoundingType::kCeil:
        output_height = ceil_output_height;
        output_width = ceil_output_width;
        break;
    }
  }
  // The layout option specifies the layout format of the output tensor.
  std::vector<uint32_t> output_shape;
  switch (attributes.layout) {
    case InputOperandLayout::kNchw:
      // "nchw": [batches, channels, height, width]
      output_shape = {input_batches, input_channels, output_height,
                      output_width};
      break;
    case InputOperandLayout::kNhwc:
      // "nhwc": [batches, height, width, channels]
      output_shape = {input_batches, output_height, output_width,
                      input_channels};
      break;
  }
  return Operand(input.data_type, std::move(output_shape));
}

GemmAttributes::GemmAttributes() = default;
GemmAttributes::~GemmAttributes() = default;

GemmAttributes::GemmAttributes(GemmAttributes&& other) = default;
GemmAttributes& GemmAttributes::operator=(GemmAttributes&& other) = default;

base::expected<Operand, std::string> ValidateGemmAndInferOutput(
    const Operand& a,
    const Operand& b,
    const GemmAttributes& attributes) {
  if (a.data_type != b.data_type) {
    return base::unexpected("The types of first two inputs don't match.");
  }
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-gemm, the first input 2-D
  // tensor with shape [M, K] if aTranspose is false, or [K, M] if aTranspose is
  // true.
  auto shape_a = a.dimensions;
  if (shape_a.size() != 2) {
    return base::unexpected("The first input must be a 2-D tensor.");
  }
  if (attributes.a_transpose) {
    std::reverse(shape_a.begin(), shape_a.end());
  }
  // The second input 2-D tensor with shape [K, N] if bTranspose is false, or
  // [N, K] if bTranspose is true.
  auto shape_b = b.dimensions;
  if (shape_b.size() != 2) {
    return base::unexpected("The second input must be a 2-D tensor.");
  }
  if (attributes.b_transpose) {
    std::reverse(shape_b.begin(), shape_b.end());
  }
  // The number of columns in the first matrix must be equal to the number of
  // rows in the second matrix.
  if (shape_a[1] != shape_b[0]) {
    return base::unexpected(base::StringPrintf(
        "The number of columns (%u) in the %sfirst matrix isn't equal to "
        "the number of rows (%u) in the %ssecond matrix.",
        shape_a[1], attributes.a_transpose ? "transposed " : "", shape_b[0],
        attributes.b_transpose ? "transposed " : ""));
  };
  // The output is 2-D tensor of shape [M, N].
  std::vector<uint32_t> output_shape = {shape_a[0], shape_b[1]};
  // The third input tensor c is either a scalar, or of the shape that is
  // unidirectionally broadcastable to the output shape [M, N].
  if (attributes.c_operand) {
    if (attributes.c_operand->data_type != a.data_type) {
      return base::unexpected(
          "The third input type doesn't match other inputs' type.");
    }
    const auto shape_c = attributes.c_operand->dimensions;
    if (shape_c.size() > 2) {
      return base::unexpected(
          "The third input tensor should be either a scalar or a 2-D tensor.");
    }
    if (!BroadcastShapes(shape_c, output_shape, false)) {
      return base::unexpected(
          "The third input tensor isn't unidirectionally broadcastable to the "
          "output tensor.");
    }
  }
  return Operand(a.data_type, std::move(output_shape));
}

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

absl::optional<PaddingSizes> CalculateConv2dPadding(AutoPad auto_pad,
                                                    const uint32_t input_size,
                                                    const uint32_t filter_size,
                                                    const uint32_t stride,
                                                    const uint32_t dilation) {
  auto checked_output_size =
      (base::MakeCheckedNum<uint32_t>(input_size) + stride - 1) / stride;
  auto checked_dilated_filter_size =
      (base::MakeCheckedNum<uint32_t>(filter_size) - 1) * dilation + 1;
  auto checked_needed_input_size =
      (checked_output_size - 1) * stride + checked_dilated_filter_size;
  if (!checked_needed_input_size.IsValid()) {
    return absl::nullopt;
  }
  auto checked_total_padding =
      checked_needed_input_size.ValueOrDie() > input_size
          ? checked_needed_input_size - input_size
          : base::MakeCheckedNum<uint32_t>(0);
  base::CheckedNumeric<uint32_t> checked_padding_begin, checked_padding_end;
  switch (auto_pad) {
    case AutoPad::kSameUpper:
      checked_padding_begin = checked_total_padding / 2;
      checked_padding_end = (checked_total_padding + 1) / 2;
      break;
    case AutoPad::kSameLower:
      checked_padding_begin = (checked_total_padding + 1) / 2;
      checked_padding_end = checked_total_padding / 2;
      break;
    case AutoPad::kExplicit:
      // The case has been ruled out before the function be called.
      NOTREACHED_NORETURN()
          << "Invalid auto pad value when calculating conv2d padding.";
  }
  uint32_t padding_begin, padding_end;
  if (!checked_padding_begin.AssignIfValid(&padding_begin) ||
      !checked_padding_end.AssignIfValid(&padding_end)) {
    return absl::nullopt;
  }
  return PaddingSizes({.begin = padding_begin, .end = padding_end});
}

}  // namespace webnn
