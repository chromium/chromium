// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ml/webnn/graph_validation_utils.h"

#include <algorithm>
#include <numeric>
#include <set>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace webnn {

namespace {

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
base::expected<Size2d<double>, std::string>
ValidateAndCalculateConv2dOutputSizes(const uint32_t input_height,
                                      const uint32_t input_width,
                                      const uint32_t filter_height,
                                      const uint32_t filter_width,
                                      const Padding2d& padding,
                                      const Size2d<uint32_t>& strides,
                                      const Size2d<uint32_t>& dilations,
                                      const AutoPad auto_pad) {
  if (strides.height == 0 || strides.width == 0) {
    return base::unexpected("All strides should be greater than 0.");
  }
  if (dilations.height == 0 || dilations.width == 0) {
    return base::unexpected("All dilations should be greater than 0.");
  }
  const uint32_t stride_height = strides.height;
  const uint32_t stride_width = strides.width;
  const uint32_t dilation_height = dilations.height;
  const uint32_t dilation_width = dilations.width;

  uint32_t padding_beginning_height = padding.beginning.height;
  uint32_t padding_ending_height = padding.ending.height;
  uint32_t padding_beginning_width = padding.beginning.width;
  uint32_t padding_ending_width = padding.ending.width;

  // When the autoPad is other than "explicit", the values in the
  // options.padding array are ignored and the explicit padding values need to
  // be calculated.
  if (auto_pad != AutoPad::kExplicit) {
    const auto padding_sizes_height = CalculateConv2dPadding(
        auto_pad, input_height, filter_height, stride_height, dilation_height);
    if (!padding_sizes_height) {
      return base::unexpected(
          "Overflow occurred when calculating the padding along the height "
          "dimension.");
    }
    padding_beginning_height = padding_sizes_height->begin;
    padding_ending_height = padding_sizes_height->end;
    const auto padding_sizes_width = CalculateConv2dPadding(
        auto_pad, input_width, filter_width, stride_width, dilation_width);
    if (!padding_sizes_width) {
      return base::unexpected(
          "Overflow occurred when calculating the padding along the width "
          "dimension.");
    }
    padding_beginning_width = padding_sizes_width->begin;
    padding_ending_width = padding_sizes_width->end;
  }

  const auto float_output_height = CalculateConv2dOutputSize(
      input_height, filter_height, padding_beginning_height,
      padding_ending_height, stride_height, dilation_height);
  if (!float_output_height.has_value()) {
    return base::unexpected("Failed to calculate the output height: " +
                            float_output_height.error());
  }

  const auto float_output_width = CalculateConv2dOutputSize(
      input_width, filter_width, padding_beginning_width, padding_ending_width,
      stride_width, dilation_width);
  if (!float_output_width.has_value()) {
    return base::unexpected("Failed to calculate the output width: " +
                            float_output_width.error());
  }

  return Size2d<double>{.height = float_output_height.value(),
                        .width = float_output_width.value()};
}

// Validate and calculate the output spatial dimensions of convTranspose2d given
// input sizes, filter sizes, padding, strides, dilations and output padding.
base::expected<Size2d<uint32_t>, std::string>
ValidateAndCalculateConvTranspose2dOutputSizes(
    const uint32_t input_height,
    const uint32_t input_width,
    const uint32_t filter_height,
    const uint32_t filter_width,
    const Padding2d& padding,
    const Size2d<uint32_t>& strides,
    const Size2d<uint32_t>& dilations,
    const Size2d<uint32_t>& output_padding,
    const AutoPad auto_pad) {
  if (strides.height == 0 || strides.width == 0) {
    return base::unexpected("All strides should be greater than 0.");
  }
  if (dilations.height == 0 || dilations.width == 0) {
    return base::unexpected("All dilations should be greater than 0.");
  }
  const uint32_t stride_height = strides.height;
  const uint32_t stride_width = strides.width;
  const uint32_t dilation_height = dilations.height;
  const uint32_t dilation_width = dilations.width;

  const uint32_t output_padding_height = output_padding.height;
  const uint32_t output_padding_width = output_padding.width;
  if (output_padding_height >= stride_height ||
      output_padding_width >= stride_width) {
    return base::unexpected(
        "The output padding must be smaller than the stride along the same "
        "dimension.");
  }

  uint32_t padding_beginning_height = padding.beginning.height;
  uint32_t padding_ending_height = padding.ending.height;
  uint32_t padding_beginning_width = padding.beginning.width;
  uint32_t padding_ending_width = padding.ending.width;

  // When the autoPad is other than "explicit", the values in the
  // options.padding array are ignored and the padding values need to be
  // calculated.
  if (auto_pad != AutoPad::kExplicit) {
    const auto padding_sizes_height = CalculateConvTranspose2dPadding(
        auto_pad, input_height, filter_height, stride_height, dilation_height,
        output_padding_height);
    if (!padding_sizes_height) {
      return base::unexpected(
          "Overflow occurred when calculating the padding along the height "
          "dimension.");
    }
    padding_beginning_height = padding_sizes_height->begin;
    padding_ending_height = padding_sizes_height->end;
    const auto padding_sizes_width = CalculateConvTranspose2dPadding(
        auto_pad, input_width, filter_width, stride_width, dilation_width,
        output_padding_width);
    if (!padding_sizes_width) {
      return base::unexpected(
          "Overflow occurred when calculating the padding along the width "
          "dimension.");
    }
    padding_beginning_width = padding_sizes_width->begin;
    padding_ending_width = padding_sizes_width->end;
  }

  const auto output_height = CalculateConvTranspose2dOutputSize(
      input_height, filter_height, padding_beginning_height,
      padding_ending_height, stride_height, dilation_height,
      output_padding_height);
  if (!output_height.has_value()) {
    return base::unexpected("Failed to calculate the output height: " +
                            output_height.error());
  }

  const auto output_width = CalculateConvTranspose2dOutputSize(
      input_width, filter_width, padding_beginning_width, padding_ending_width,
      stride_width, dilation_width, output_padding_width);
  if (!output_width.has_value()) {
    return base::unexpected("Failed to calculate the output width: " +
                            output_width.error());
  }

  return Size2d<uint32_t>{.height = output_height.value(),
                          .width = output_width.value()};
}

struct Conv2dInputOutputInfo {
  uint32_t batches;
  uint32_t channels;
  uint32_t height;
  uint32_t width;
};

// Validate and get the input info of 2-D direct and transposed convolution
// operation given input operand and attributes.
base::expected<Conv2dInputOutputInfo, std::string>
ValidateAndGetConv2dInputInfo(const Operand& input,
                              const Conv2dAttributesBase& attributes) {
  // Validate input operand.
  const auto& input_shape = input.dimensions;
  if (input_shape.size() != 4) {
    return base::unexpected("The input should be a 4-D tensor.");
  }
  // The input layout option specifies the layout format of the input tensor.
  uint32_t batches, channels, height, width;
  switch (attributes.input_layout) {
    case InputOperandLayout::kNchw:
      // "nchw": [batches, input_channels, height, width]
      batches = input_shape[0];
      channels = input_shape[1];
      height = input_shape[2];
      width = input_shape[3];
      break;
    case InputOperandLayout::kNhwc:
      // "nhwc": [batches, height, width, input_channels]
      batches = input_shape[0];
      height = input_shape[1];
      width = input_shape[2];
      channels = input_shape[3];
      break;
  }

  return Conv2dInputOutputInfo{.batches = batches,
                               .channels = channels,
                               .height = height,
                               .width = width};
}

// Validate the bias of 2-D direct and transposed convolution operation and
// create output operand given input operand, attributes and output info.
base::expected<Operand, std::string> ValidateConv2dBiasAndCreateOutputOperand(
    const Operand& input,
    const Conv2dAttributesBase& attributes,
    const Conv2dInputOutputInfo& output_info) {
  // Validate bias operand if it is present.
  if (attributes.bias_operand) {
    const auto& bias_shape = attributes.bias_operand->dimensions;
    if (bias_shape.size() != 1) {
      return base::unexpected("The bias should be a 1-D tensor.");
    }
    if (bias_shape[0] != output_info.channels) {
      return base::unexpected(base::StringPrintf(
          "The bias shape should be [%u].", output_info.channels));
    }
    if (attributes.bias_operand->data_type != input.data_type) {
      return base::unexpected(
          "The bias data type doesn't match input data type.");
    }
  }

  // The input layout option specifies the layout format of the output tensor.
  std::vector<uint32_t> output_shape;
  switch (attributes.input_layout) {
    case InputOperandLayout::kNchw:
      // "nchw": [batches, output_channels, height, width]
      output_shape = {output_info.batches, output_info.channels,
                      output_info.height, output_info.width};
      break;
    case InputOperandLayout::kNhwc:
      // "nhwc": [batches, height, width, output_channels]
      output_shape = {output_info.batches, output_info.height,
                      output_info.width, output_info.channels};
      break;
  }

  return Operand(input.data_type, std::move(output_shape));
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

std::string DataTypeToString(Operand::DataType data_type) {
  switch (data_type) {
    case Operand::DataType::kFloat32:
      return "float32";
    case Operand::DataType::kFloat16:
      return "float16";
    case Operand::DataType::kInt32:
      return "int32";
    case Operand::DataType::kUint32:
      return "uint32";
    case Operand::DataType::kInt64:
      return "int64";
    case Operand::DataType::kUint64:
      return "uint64";
    case Operand::DataType::kInt8:
      return "int8";
    case Operand::DataType::kUint8:
      return "uint8";
  }
  NOTREACHED_NORETURN();
}

std::string DataTypeConstraintToString(
    const DataTypeConstraintSet& constraint_set) {
  std::vector<std::string> data_types;
  data_types.reserve(constraint_set.Size());
  for (auto data_type : constraint_set) {
    data_types.push_back(DataTypeToString(data_type));
  }
  return base::JoinString(data_types, /* saperator */ ",");
}

base::expected<Operand, std::string> ValidateSoftmaxAndInferOutput(
    Operand input) {
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-softmax, The input must be
  // a 2-D tensor.
  if (input.dimensions.size() != 2) {
    return base::unexpected("The input must be a 2-D tensor.");
  }
  // The input data type must be one of the floating point types.
  if (!IsFloatingPointType(input.data_type)) {
    return base::unexpected(
        "The input data type must be one of the floating point types.");
  }
  // The output tensor of softmax is the same shape as the input tensor.
  return Operand(input.data_type, std::move(input.dimensions));
}

base::expected<std::vector<Operand>, std::string> ValidateSplitAndInferOutput(
    const Operand& input,
    const SplitAttribute& attributes) {
  std::vector<Operand> outputs;

  if (attributes.axis >= input.dimensions.size()) {
    return base::unexpected(
        "The axis must be in the range [0, N-1] where N is the rank of the "
        "input tensor.");
  }

  static_assert(absl::variant_size<decltype(attributes.splits)>() == 2,
                "When adding new variants update the branches below.");
  if (absl::holds_alternative<uint32_t>(attributes.splits)) {
    uint32_t splits = absl::get<uint32_t>(attributes.splits);
    if (splits == 0) {
      return base::unexpected("The splits must be greater than zero.");
    }

    if (input.dimensions[attributes.axis] % splits != 0) {
      return base::unexpected(
          "The dimension size of the input tensor along "
          "options.axis must be divisible by splits.");
    }

    outputs.reserve(splits);
    for (uint32_t i = 0; i < splits; ++i) {
      // When splits is of type uint32_t, we create splits number of Operands.
      // Each Operand will have the same new_dimensions shape.
      std::vector<uint32_t> new_dimensions = input.dimensions;
      new_dimensions[attributes.axis] /= splits;
      outputs.emplace_back(input.data_type, std::move(new_dimensions));
    }
  } else if (absl::holds_alternative<base::span<const uint32_t>>(
                 attributes.splits)) {
    const auto& splits =
        absl::get<base::span<const uint32_t>>(attributes.splits);
    if (base::ranges::any_of(splits,
                             [](uint32_t split) { return split == 0; })) {
      return base::unexpected("All splits must be greater than zero.");
    }

    base::CheckedNumeric<uint32_t> sum = std::accumulate(
        splits.begin(), splits.end(), base::MakeCheckedNum<uint32_t>(0));
    if (!sum.IsValid() ||
        sum.ValueOrDie() != input.dimensions[attributes.axis]) {
      return base::unexpected(
          "The sum of all sizes in splits must be equal to the dimension size "
          "of the input tensor specified by options.axis.");
    }

    outputs.reserve(splits.size());
    for (uint32_t split : splits) {
      std::vector<uint32_t> new_dimensions = input.dimensions;
      new_dimensions[attributes.axis] = split;
      outputs.emplace_back(input.data_type, std::move(new_dimensions));
    }
  } else {
    NOTREACHED_NORETURN();
  }

  return outputs;
}

// This helper method is intended to validate mean, variance, scale and bias
// operands of batchNormalization against the input operand. These operands
// share the same constraint.
base::expected<void, std::string>
ValidateBatchNormalizationOperandIsCompatibleWithInput(
    const Operand& operand,
    const Operand::DataType input_data_type,
    size_t input_size_on_axis) {
  if (operand.data_type != input_data_type) {
    return base::unexpected("the data type doesn't match the input data type.");
  }
  if (operand.dimensions.size() != 1) {
    return base::unexpected("the operand should be a 1-D tensor.");
  }

  if (operand.dimensions[0] != input_size_on_axis) {
    return base::unexpected(
        "the size of operand must be equal to the size of the input dimension "
        "denoted by axis.");
  }

  return base::ok();
}

BatchNormalizationAttributes::BatchNormalizationAttributes() = default;
BatchNormalizationAttributes::~BatchNormalizationAttributes() = default;

BatchNormalizationAttributes::BatchNormalizationAttributes(
    BatchNormalizationAttributes&& other) = default;
BatchNormalizationAttributes& BatchNormalizationAttributes::operator=(
    BatchNormalizationAttributes&& other) = default;

base::expected<Operand, std::string> ValidateBatchNormalizationAndInferOutput(
    const Operand& input,
    const Operand& mean,
    const Operand& variance,
    const BatchNormalizationAttributes& attributes) {
  // Validate input type.
  if (!IsFloatingPointType(input.data_type)) {
    return base::unexpected(
        "The input type must be one of the floating point types.");
  }
  if (base::MakeStrictNum(attributes.axis) >= input.dimensions.size()) {
    return base::unexpected(
        "The value of axis must be in the range [0, N-1] where N is the rank "
        "of the input tensor.");
  }

  auto input_size_on_axis = input.dimensions[attributes.axis];
  auto input_data_type = input.data_type;
  // Validate mean operand.
  const auto validation_mean =
      ValidateBatchNormalizationOperandIsCompatibleWithInput(
          mean, input_data_type, input_size_on_axis);
  if (!validation_mean.has_value()) {
    return base::unexpected("For mean operand: " + validation_mean.error());
  }

  // Validate variance operand.
  const auto validation_variance =
      ValidateBatchNormalizationOperandIsCompatibleWithInput(
          variance, input_data_type, input_size_on_axis);
  if (!validation_variance.has_value()) {
    return base::unexpected("For variance operand: " +
                            validation_variance.error());
  }

  // Validate scale operand.
  if (attributes.scale) {
    const auto validation_scale =
        ValidateBatchNormalizationOperandIsCompatibleWithInput(
            attributes.scale.value(), input_data_type, input_size_on_axis);
    if (!validation_scale.has_value()) {
      return base::unexpected("For scale operand: " + validation_scale.error());
    }
  }

  // Validate bias operand.
  if (attributes.bias) {
    const auto validation_bias =
        ValidateBatchNormalizationOperandIsCompatibleWithInput(
            attributes.bias.value(), input_data_type, input_size_on_axis);
    if (!validation_bias.has_value()) {
      return base::unexpected("For bias operand: " + validation_bias.error());
    }
  }

  // The output tensor of batchNormalization is the same shape as the input
  // tensor.
  return Operand(input_data_type, std::move(input.dimensions));
}

Conv2dAttributesBase::Conv2dAttributesBase() = default;
Conv2dAttributesBase::~Conv2dAttributesBase() = default;

Conv2dAttributesBase::Conv2dAttributesBase(Conv2dAttributesBase&& other) =
    default;
Conv2dAttributesBase& Conv2dAttributesBase::operator=(
    Conv2dAttributesBase&& other) = default;

Conv2dAttributes::Conv2dAttributes() = default;
Conv2dAttributes::~Conv2dAttributes() = default;

Conv2dAttributes::Conv2dAttributes(Conv2dAttributes&& other) = default;
Conv2dAttributes& Conv2dAttributes::operator=(Conv2dAttributes&& other) =
    default;

base::expected<Operand, std::string> ValidateConv2dAndInferOutput(
    const Operand& input,
    const Operand& filter,
    const Conv2dAttributes& attributes) {
  // Validate input operand.
  const auto input_info = ValidateAndGetConv2dInputInfo(input, attributes);
  if (!input_info.has_value()) {
    return base::unexpected(input_info.error());
  }
  // Validate filter operand.
  if (filter.data_type != input.data_type) {
    return base::unexpected(
        "The filter data type doesn't match the input data type.");
  }
  const auto filter_shape = filter.dimensions;
  if (filter_shape.size() != 4) {
    return base::unexpected("The filter should be a 4-D tensor.");
  }

  uint32_t filter_height, filter_width, output_channels, filter_input_channels;
  // The conv2d filter layout specifies the filter layout format.
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

  // Validate groups and input channels.
  if (attributes.groups == 0) {
    return base::unexpected("The groups should be greater than 0.");
  }
  if (input_info->channels % attributes.groups != 0 ||
      filter_input_channels != input_info->channels / attributes.groups) {
    return base::unexpected(
        "The groups must evenly divide the input channels to filter input "
        "channels.");
  }

  // Validate and calculate output sizes.
  const auto output_sizes = ValidateAndCalculateConv2dOutputSizes(
      input_info->height, input_info->width, filter_height, filter_width,
      attributes.padding, attributes.strides, attributes.dilations,
      attributes.auto_pad);
  if (!output_sizes.has_value()) {
    return base::unexpected(output_sizes.error());
  }
  uint32_t output_height = base::ClampFloor<uint32_t>(output_sizes->height);
  uint32_t output_width = base::ClampFloor<uint32_t>(output_sizes->width);

  Conv2dInputOutputInfo output_info{.batches = input_info->batches,
                                    .channels = output_channels,
                                    .height = output_height,
                                    .width = output_width};
  return ValidateConv2dBiasAndCreateOutputOperand(input, attributes,
                                                  output_info);
}

ConvTranspose2dAttributes::ConvTranspose2dAttributes() = default;
ConvTranspose2dAttributes::~ConvTranspose2dAttributes() = default;

ConvTranspose2dAttributes::ConvTranspose2dAttributes(
    ConvTranspose2dAttributes&& other) = default;
ConvTranspose2dAttributes& ConvTranspose2dAttributes::operator=(
    ConvTranspose2dAttributes&& other) = default;

base::expected<Operand, std::string> ValidateConvTranspose2dAndInferOutput(
    const Operand& input,
    const Operand& filter,
    const ConvTranspose2dAttributes& attributes) {
  // Validate input operand.
  const auto input_info = ValidateAndGetConv2dInputInfo(input, attributes);
  if (!input_info.has_value()) {
    return base::unexpected(input_info.error());
  }
  // Validate filter operand.
  if (filter.data_type != input.data_type) {
    return base::unexpected(
        "The filter data type doesn't match the input data type.");
  }
  const auto filter_shape = filter.dimensions;
  if (filter_shape.size() != 4) {
    return base::unexpected("The filter should be a 4-D tensor.");
  }

  uint32_t input_channels, filter_height, filter_width, filter_output_channels;
  // The conv2d filter layout specifies the filter layout format.
  switch (attributes.filter_layout) {
    case ConvTranspose2dFilterOperandLayout::kIohw:
      // "iohw": [input_channels, output_channels/groups, height, width]
      input_channels = filter_shape[0];
      filter_output_channels = filter_shape[1];
      filter_height = filter_shape[2];
      filter_width = filter_shape[3];
      break;
    case ConvTranspose2dFilterOperandLayout::kHwoi:
      // "hwoi": [height, width, output_channels/groups, input_channels]
      filter_height = filter_shape[0];
      filter_width = filter_shape[1];
      filter_output_channels = filter_shape[2];
      input_channels = filter_shape[3];
      break;
    case ConvTranspose2dFilterOperandLayout::kOhwi:
      // "ohwi": [output_channels/groups, height, width, input_channels]
      filter_output_channels = filter_shape[0];
      filter_height = filter_shape[1];
      filter_width = filter_shape[2];
      input_channels = filter_shape[3];
      break;
  }
  // Validate groups, input channels and calculate output channels.
  if (attributes.groups == 0) {
    return base::unexpected("The groups should be greater than 0.");
  }
  if (input_info->channels != input_channels) {
    return base::unexpected(
        "The input channels should equal to filter input channels.");
  }
  const auto checked_output_channels =
      base::MakeCheckedNum<uint32_t>(filter_output_channels) *
      attributes.groups;
  if (!checked_output_channels.IsValid()) {
    return base::unexpected("The output channels is too large.");
  }
  const uint32_t output_channels = checked_output_channels.ValueOrDie();

  // Validate and calculate output sizes.
  uint32_t output_height, output_width;
  if (attributes.output_sizes) {
    const auto& output_sizes = attributes.output_sizes;
    output_height = output_sizes->height;
    output_width = output_sizes->width;
    if (output_height <= 0 || output_width <= 0) {
      return base::unexpected("All output sizes should be greater than 0.");
    }
    const auto strides = attributes.strides;
    const auto calculated_output_sizes =
        ValidateAndCalculateConvTranspose2dOutputSizes(
            input_info->height, input_info->width, filter_height, filter_width,
            attributes.padding, strides, attributes.dilations,
            // According to WebNN spec:
            // https://webmachinelearning.github.io/webnn/#dom-mlconvtranspose2doptions-outputsizes
            // When the output sizes are explicitly specified, the output
            // padding values in outputPadding are ignored.
            {0, 0}, attributes.auto_pad);
    if (!calculated_output_sizes.has_value()) {
      return base::unexpected(calculated_output_sizes.error());
    }
    const auto calculated_output_height = calculated_output_sizes->height;
    const auto max_output_height =
        base::MakeCheckedNum<uint32_t>(calculated_output_height) +
        strides.height;
    if (!max_output_height.IsValid()) {
      return base::unexpected("The checked maximum output height is too large");
    }
    if (output_height < calculated_output_height ||
        output_height >= max_output_height.ValueOrDie()) {
      return base::unexpected("The height of output sizes is invalid.");
    }
    const auto calculated_output_width = calculated_output_sizes->width;
    const auto max_output_width =
        base::MakeCheckedNum<uint32_t>(calculated_output_width) + strides.width;
    if (!max_output_width.IsValid()) {
      return base::unexpected("The checked maximum output width is too large");
    }
    if (output_width < calculated_output_width ||
        output_width >= max_output_width.ValueOrDie()) {
      return base::unexpected("The width of output sizes is invalid.");
    }
  } else {
    const auto output_sizes = ValidateAndCalculateConvTranspose2dOutputSizes(
        input_info->height, input_info->width, filter_height, filter_width,
        attributes.padding, attributes.strides, attributes.dilations,
        attributes.output_padding, attributes.auto_pad);
    if (!output_sizes.has_value()) {
      return base::unexpected(output_sizes.error());
    }
    output_height = output_sizes->height;
    output_width = output_sizes->width;
  }

  Conv2dInputOutputInfo output_info{.batches = input_info->batches,
                                    .channels = output_channels,
                                    .height = output_height,
                                    .width = output_width};
  return ValidateConv2dBiasAndCreateOutputOperand(input, attributes,
                                                  output_info);
}

base::expected<Operand, std::string> ValidatePadAndInferOutput(
    const Operand& input,
    base::span<const uint32_t> beginning_padding,
    base::span<const uint32_t> ending_padding) {
  // Validate the beginning_padding and ending_padding.
  const auto input_shape = input.dimensions;
  auto input_rank = input_shape.size();
  if (input_rank == 0) {
    return base::unexpected("The input should not be a scalar.");
  }
  if (beginning_padding.size() != input_rank) {
    return base::unexpected(
        "The length of beginningPadding must be "
        "equal to the rank of the input tensor.");
  }
  if (ending_padding.size() != input_rank) {
    return base::unexpected(
        "The length of endingPadding must be "
        "equal to the rank of the input tensor.");
  }

  // Infer the output.
  // Each dimension of the output tensor can be calculated as follow:
  // input_size = input_shape[i];
  // output_size = beginning_padding + input_size + ending_padding.
  std::vector<uint32_t> output_shape(input_rank);
  for (size_t i = 0; i < input_rank; ++i) {
    auto checked_output_size = base::MakeCheckedNum<uint32_t>(input_shape[i]) +
                               beginning_padding[i] + ending_padding[i];
    if (!checked_output_size.AssignIfValid(&output_shape[i])) {
      return base::unexpected(base::StringPrintf(
          "The padding of dimension (%zu) is too large.", i));
    }
  }

  return Operand(input.data_type, std::move(output_shape));
}

base::expected<Operand, std::string> ValidateMatmulAndInferOutput(
    const Operand& a,
    const Operand& b) {
  if (a.data_type != b.data_type) {
    return base::unexpected("The data types of first two inputs don't match.");
  }

  std::vector<uint32_t> a_dimensions = a.dimensions;
  std::vector<uint32_t> b_dimensions = b.dimensions;

  // Based on the WG discussion:
  // https://github.com/webmachinelearning/webnn/issues/470, prototype the
  // matmul without 1-D input tensors support.
  if (a_dimensions.size() < 2 || b_dimensions.size() < 2) {
    return base::unexpected(
        "The rank of input must be larger than or equal to 2.");
  }

  // The number of columns in the first matrix must be equal to the number of
  // rows in the second matrix.
  const uint32_t a_cols = a_dimensions[a_dimensions.size() - 1];
  const uint32_t a_rows = a_dimensions[a_dimensions.size() - 2];
  const uint32_t b_cols = b_dimensions[b_dimensions.size() - 1];
  const uint32_t b_rows = b_dimensions[b_dimensions.size() - 2];
  if (a_cols != b_rows) {
    return base::unexpected(base::StringPrintf(
        "The number of columns (%u) in the first matrix isn't equal to "
        "the number of rows (%u) in the second matrix.",
        a_cols, b_rows));
  }

  size_t output_rank = std::max(a_dimensions.size(), b_dimensions.size());
  std::vector<uint32_t> output_dimensions;
  // Figure out the output shape by broadcasting all the dimensions except the
  // last two. The output is 2-D tensor of shape [M, N].
  if (a_dimensions.size() > 2 && b_dimensions.size() > 2) {
    std::vector<uint32_t> sliced_a_dimensions(a_dimensions.begin(),
                                              a_dimensions.end() - 2);
    std::vector<uint32_t> sliced_b_dimensions(b_dimensions.begin(),
                                              b_dimensions.end() - 2);
    absl::optional<std::vector<uint32_t>> optional_output_dimensions =
        BroadcastShapes(sliced_a_dimensions, sliced_b_dimensions, true);
    if (!optional_output_dimensions) {
      return base::unexpected("The matmul input shapes are not broadcastable.");
    }
    output_dimensions = *optional_output_dimensions;
    output_dimensions.push_back(a_rows);
    output_dimensions.push_back(b_cols);
  } else if (a_dimensions.size() == 2 && b_dimensions.size() == 2) {
    output_dimensions.push_back(a_rows);
    output_dimensions.push_back(b_cols);
  } else {
    output_dimensions =
        a_dimensions.size() > b_dimensions.size() ? a_dimensions : b_dimensions;
    output_dimensions[output_rank - 2] = a_rows;
    output_dimensions[output_rank - 1] = b_cols;
  }
  CHECK_EQ(output_rank, output_dimensions.size());
  return Operand(a.data_type, std::move(output_dimensions));
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

// The current WebNN spec doesn't define the calculation formula of the output
// size for resample2d. An issue has been filed to track it -
// https://github.com/webmachinelearning/webnn/issues/360.
base::expected<uint32_t, std::string> CalculateResample2dOutputSize(
    const uint32_t input_size,
    const float scale) {
  // Calculate the output size in double precision floating point number that
  // ensures values of type uint32_t can be exactly represented.
  // https://en.wikipedia.org/wiki/Double-precision_floating-point_format#Precision_limitations_on_integer_values
  auto checked_output_size = base::MakeCheckedNum<double>(input_size) * scale;

  // Check if the value is valid for rounding to uint32_t type.
  if (!checked_output_size.IsValid<uint32_t>()) {
    return base::unexpected("The scale is too large.");
  }
  const uint32_t output_size =
      base::ClampFloor<uint32_t>(double(checked_output_size.ValueOrDie()));
  if (output_size == 0) {
    return base::unexpected("The scale is too small.");
  }
  return output_size;
}

base::expected<Operand, std::string> ValidateResample2dAndInferOutput(
    const Operand& input,
    const absl::variant<base::span<const float>, base::span<const uint32_t>>&
        scales_or_sizes,
    base::span<const uint32_t> axes) {
  // Validate the input.
  const auto& input_shape = input.dimensions;
  if (input_shape.size() != 4) {
    return base::unexpected("The input must be a 4-D tensor.");
  }

  // Validate axes.
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-resample2d,
  // the valid values in the sequence are [0, 1], [1, 2] or [2, 3].
  if (axes.size() != 2) {
    return base::unexpected("The length of axes should be 2.");
  }
  if (!((axes[0] == 0 && axes[1] == 1) || (axes[0] == 1 && axes[1] == 2) ||
        (axes[0] == 2 && axes[1] == 3))) {
    return base::unexpected("The values of axes are invalid.");
  }

  // Validate scales or sizes and infer the output.
  std::vector<uint32_t> output_shape(input_shape);
  if (absl::holds_alternative<base::span<const float>>(scales_or_sizes)) {
    const auto& scales = absl::get<base::span<const float>>(scales_or_sizes);
    if (scales.size() != 2) {
      return base::unexpected("The length of scales should be 2.");
    }
    if (scales[0] < 0 || scales[1] < 0) {
      return base::unexpected("All scales should be greater than 0.");
    }

    auto output_height =
        CalculateResample2dOutputSize(input_shape[axes[0]], scales[0]);
    if (!output_height.has_value()) {
      return base::unexpected("Failed to calculate the output height: " +
                              output_height.error());
    }
    output_shape[axes[0]] = output_height.value();

    auto output_width =
        CalculateResample2dOutputSize(input_shape[axes[1]], scales[1]);
    if (!output_width.has_value()) {
      return base::unexpected("Failed to calculate the output width: " +
                              output_width.error());
    }
    output_shape[axes[1]] = output_width.value();
  } else if (absl::holds_alternative<base::span<const uint32_t>>(
                 scales_or_sizes)) {
    const auto& sizes = absl::get<base::span<const uint32_t>>(scales_or_sizes);
    if (sizes.size() != 2) {
      return base::unexpected("The length of sizes should be 2.");
    }
    if (sizes[0] == 0 || sizes[1] == 0) {
      return base::unexpected("All sizes should be greater than 0.");
    }

    output_shape[axes[0]] = sizes[0];
    output_shape[axes[1]] = sizes[1];
  } else {
    NOTREACHED_NORETURN();
  }

  return Operand(input.data_type, std::move(output_shape));
}

base::expected<Operand, std::string> ValidateGatherAndInferOutput(
    const Operand& input,
    const Operand& indices,
    const uint32_t axis) {
  const auto& input_dimensions = input.dimensions;
  const auto input_rank = input_dimensions.size();
  if (input_rank == 0) {
    return base::unexpected("The input should not be a scalar.");
  }

  if (base::MakeStrictNum(input_rank) <= axis) {
    return base::unexpected(
        "The axis must be in the range [0, N-1] where N is the rank of input "
        "tensor.");
  }

  if (!DataTypeConstraint::kGatherOperatorIndexDataTypes.Has(
          indices.data_type)) {
    return base::unexpected(base::StringPrintf(
        "The indices type must be one of the %s types.",
        DataTypeConstraintToString(
            DataTypeConstraint::kGatherOperatorIndexDataTypes)
            .c_str()));
  }

  const auto& indices_dimensions = indices.dimensions;
  auto checked_output_rank =
      base::MakeCheckedNum<size_t>(input_rank) - 1 + indices_dimensions.size();
  if (!checked_output_rank.IsValid()) {
    return base::unexpected("The output rank is too large.");
  }

  std::vector<uint32_t> output_shape;
  output_shape.reserve(checked_output_rank.ValueOrDie());
  for (size_t i = 0; i < input_rank; ++i) {
    if (i == axis) {
      base::ranges::copy(indices_dimensions, std::back_inserter(output_shape));
    } else {
      output_shape.push_back(input_dimensions[i]);
    }
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
    return base::unexpected("The data types of first two inputs don't match.");
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
          "The third input data type doesn't match other inputs' data type.");
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

base::expected<Operand, std::string> ValidateConcatAndInferOutput(
    const std::vector<Operand>& inputs,
    const uint32_t axis) {
  if (inputs.empty()) {
    return base::unexpected("The inputs should not be empty.");
  }
  const auto& first_input_shape = inputs[0].dimensions;
  const auto first_input_rank = first_input_shape.size();
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#dom-mlgraphbuilder-concat-inputs-axis-axis,
  // the axis that the inputs concatenate along, with the value in the interval
  // [0, N-1] where N is the rank of input tensors. We just check the first
  // input rank here because we will check all inputs have same rank in the
  // following loop.
  if (axis >= first_input_rank) {
    return base::unexpected(
        "The axis must be in the range [0, N-1] where N is the rank of input "
        "tensor.");
  }
  const auto output_type = inputs[0].data_type;
  // The loop skips the first input to avoid repeated checks.
  for (size_t i = 1; i < inputs.size(); ++i) {
    if (inputs[i].data_type != output_type) {
      return base::unexpected("The input data types don't match.");
    }
    // According to WebNN spec:
    // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-concat, all input tensors
    // must have the same dimension.
    if (inputs[i].dimensions.size() != first_input_rank) {
      return base::unexpected(
          "All input tensors must have the same dimension.");
    }
    // According to WebNN spec:
    // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-concat, all input tensors
    // must have the same shape, except for the size of the dimension to
    // concatenate on.
    for (size_t dim = 0; dim < first_input_rank; ++dim) {
      if (dim == axis || inputs[i].dimensions[dim] == first_input_shape[dim]) {
        continue;
      }
      return base::unexpected(
          "All input tensors must have the same shape, except for the size of "
          "the dimension to concatenate on.");
    }
  }
  // Calculate the output shape according to WebNN spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-concat, the output tensor
  // has the same shape except on the dimension that all the inputs concatenated
  // along. The size of that dimension is computed as the sum of all the input
  // sizes of the same dimension.
  auto axis_size = base::MakeCheckedNum<uint32_t>(0);
  for (auto& input : inputs) {
    axis_size += input.dimensions[axis];
  }
  auto output_shape = first_input_shape;
  if (!axis_size.AssignIfValid(&output_shape[axis])) {
    return base::unexpected("The concatenated dimension size is too large.");
  }

  return Operand(output_type, std::move(output_shape));
}

base::expected<Operand, std::string> ValidatePreluAndInferOutput(
    const Operand& input,
    const Operand& slope) {
  if (input.data_type != slope.data_type) {
    return base::unexpected(
        "The data type of slope doesn't match the data type of input.");
  }
  if (!IsFloatingPointType(input.data_type)) {
    return base::unexpected(
        "The data type of input and slope must be one of the floating point "
        "types.");
  }
  // BroadcastShape unidirectionally broadcasts slope.dimensions to
  // input.dimensions.
  if (!BroadcastShapes(slope.dimensions, input.dimensions, false)) {
    return base::unexpected(
        "The shape of slope is not broadcastable to the shape of input.");
  }

  return Operand(input.data_type, input.dimensions);
}

base::expected<Operand, std::string> ValidateTransposeAndInferOutput(
    const Operand& input,
    base::span<const uint32_t> permutation) {
  auto input_dimensions = input.dimensions;
  auto input_rank = input_dimensions.size();
  if (permutation.size() != input_rank) {
    return base::unexpected(
        "The number of values in permutation must be the same as the rank of "
        "the input tensor.");
  }
  auto validation_result = ValidateAxes(permutation, input_rank);
  if (!validation_result.has_value()) {
    return base::unexpected(validation_result.error());
  }

  std::vector<uint32_t> output_shape(input_rank);
  for (size_t i = 0; i < input_rank; ++i) {
    output_shape[i] = input_dimensions[permutation[i]];
  }
  return Operand(input.data_type, std::move(output_shape));
}

SliceAttributes::SliceAttributes() = default;
SliceAttributes::~SliceAttributes() = default;

SliceAttributes::SliceAttributes(SliceAttributes&& other) = default;
SliceAttributes& SliceAttributes::operator=(SliceAttributes&& other) = default;

base::expected<Operand, std::string> ValidateSliceAndInferOutput(
    const Operand& input,
    const SliceAttributes& attributes) {
  const auto input_rank = input.dimensions.size();
  if (input_rank == 0) {
    return base::unexpected("The input should not be a scalar.");
  }

  if (attributes.starts.size() != input_rank) {
    return base::unexpected(
        "The length of starts must be equal to the rank of the input tensor.");
  }

  if (attributes.sizes.size() != input_rank) {
    return base::unexpected(
        "The length of sizes must be equal to the rank of the input tensor.");
  }

  for (uint32_t i = 0; i < input_rank; ++i) {
    if (attributes.starts[i] >= input.dimensions[i]) {
      return base::unexpected(base::StringPrintf(
          "For dimension (%u): the starting index to slice must "
          "be less than input size (%u).",
          i, input.dimensions[i]));
    }

    // WebNN plans to allow 0 size dimensions and an issue has been filed to
    // track it: https://github.com/webmachinelearning/webnn/issues/391.
    if (attributes.sizes[i] == 0) {
      return base::unexpected(base::StringPrintf(
          "For dimension (%u): the number of elements to slice "
          "must not be 0.",
          i));
    }

    auto checked_ending_index =
        base::MakeCheckedNum<uint32_t>(attributes.starts[i]) +
        attributes.sizes[i];
    if (!checked_ending_index.IsValid<uint32_t>()) {
      return base::unexpected(base::StringPrintf(
          "For dimension (%u): the ending index to slice is too large.", i));
    }

    if (checked_ending_index.ValueOrDie() > input.dimensions[i]) {
      return base::unexpected(
          base::StringPrintf("For dimension (%u): the ending index to slice "
                             "must not be greater than input size (%u).",
                             i, input.dimensions[i]));
    }
  }

  // The output is a tensor the same as the specified slice sizes.
  std::vector<uint32_t> output_shape;
  output_shape.assign(attributes.sizes.begin(), attributes.sizes.end());
  return Operand(input.data_type, std::move(output_shape));
}

base::expected<Operand, std::string> ValidateReduceAndInferOutput(
    ReduceKind kind,
    const Operand& input,
    base::span<const uint32_t> axes,
    bool keep_dimensions) {
  auto input_dimensions = input.dimensions;
  auto input_rank = input_dimensions.size();
  auto validation_result = ValidateAxes(axes, input_rank);
  if (!validation_result.has_value()) {
    return base::unexpected(validation_result.error());
  }

  if (kind == ReduceKind::kL2 || kind == ReduceKind::kMean ||
      kind == ReduceKind::kLogSum || kind == ReduceKind::kLogSumExp) {
    if (!IsFloatingPointType(input.data_type)) {
      return base::unexpected(
          "The input data type must be one of the floating point types.");
    }
  }

  std::vector<uint32_t> output_shape;
  if (keep_dimensions) {
    output_shape = input_dimensions;
    for (auto axis : axes) {
      output_shape[axis] = 1;
    }
  } else {
    for (size_t i = 0; i < input_rank; i++) {
      if (!base::Contains(axes, i)) {
        output_shape.push_back(input_dimensions[i]);
      }
    }
  }
  return Operand(input.data_type, std::move(output_shape));
}

base::expected<Operand, std::string> ValidateWhereAndInferOutput(
    const Operand& condition,
    const Operand& true_value,
    const Operand& false_value) {
  if (condition.data_type != Operand::DataType::kUint8) {
    return base::unexpected("The condition data type must be uint8.");
  }

  if (true_value.data_type != false_value.data_type) {
    return base::unexpected(
        "The data types of true_value and false_value don't match.");
  }

  const auto value_shape =
      BroadcastShapes(true_value.dimensions, false_value.dimensions, true);
  if (!value_shape) {
    return base::unexpected(
        "The shapes of true_value and false_value are not broadcastable.");
  }

  const auto output_shape =
      BroadcastShapes(condition.dimensions, value_shape.value(), true);
  if (!output_shape) {
    return base::unexpected(
        "The condition shape is not broadcastable to the shape broadcasted "
        "from true_value and false_value.");
  }
  return Operand(true_value.data_type, std::move(output_shape.value()));
}

base::expected<size_t, std::string> ValidateAndCalculateElementsNumber(
    base::span<const uint32_t> dimensions) {
  // Empty dimensions represents a scalar whose number of elements is 1.
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

base::expected<void, std::string> ValidateAxes(base::span<const uint32_t> axes,
                                               uint32_t rank) {
  if (base::ranges::any_of(axes, [rank](uint32_t axis) {
        return base::MakeStrictNum(axis) >= rank;
      })) {
    return base::unexpected(base::StringPrintf(
        "The values in axes must be within the range from 0 to (%u).",
        rank - 1));
  }

  if (axes.size() != std::set<uint32_t>(axes.begin(), axes.end()).size()) {
    return base::unexpected(
        "Two or more values are same in the axes sequence.");
  }

  return base::ok();
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

absl::optional<PaddingSizes> CalculateConvTranspose2dPadding(
    AutoPad auto_pad,
    const uint32_t input_size,
    const uint32_t filter_size,
    const uint32_t stride,
    const uint32_t dilation,
    const uint32_t output_padding) {
  auto checked_output_size =
      base::MakeCheckedNum<uint32_t>(input_size) * stride;
  auto checked_effective_filter_size =
      (base::MakeCheckedNum<uint32_t>(filter_size) - 1) * dilation + 1;
  auto checked_total_padding =
      stride * (base::MakeCheckedNum<uint32_t>(input_size) - 1) +
      checked_effective_filter_size + output_padding - checked_output_size;
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
          << "Invalid auto pad value when calculating convTranspose2d padding.";
  }
  uint32_t padding_begin, padding_end;
  if (!checked_padding_begin.AssignIfValid(&padding_begin) ||
      !checked_padding_end.AssignIfValid(&padding_end)) {
    return absl::nullopt;
  }
  return webnn::PaddingSizes({.begin = padding_begin, .end = padding_end});
}

base::expected<uint32_t, std::string> CalculateConvTranspose2dOutputSize(
    const uint32_t input_size,
    const uint32_t filter_size,
    const uint32_t beginning_padding,
    const uint32_t ending_padding,
    const uint32_t stride,
    const uint32_t dilation,
    const uint32_t output_padding) {
  // Calculate the dilated filter sizes.
  auto checked_effective_filter_size =
      (base::MakeCheckedNum<uint32_t>(filter_size) - 1) * dilation + 1;
  if (!checked_effective_filter_size.IsValid()) {
    return base::unexpected("The effective filter size is too large.");
  }
  auto checked_output_size =
      (base::MakeCheckedNum<uint32_t>(input_size) - 1) * stride +
      checked_effective_filter_size - beginning_padding - ending_padding +
      output_padding;
  if (!checked_output_size.IsValid()) {
    return base::unexpected(
        "The stride is too large or the input size is too small for padding.");
  }

  return checked_output_size.ValueOrDie();
}

bool IsFloatingPointType(Operand::DataType data_type) {
  return DataTypeConstraint::kFloat.Has(data_type);
}

}  // namespace webnn
