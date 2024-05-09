// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ML_WEBNN_GRAPH_VALIDATION_UTILS_H_
#define COMPONENTS_ML_WEBNN_GRAPH_VALIDATION_UTILS_H_

#include <optional>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/containers/span.h"
#include "base/types/expected.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace webnn {

// The struct defined in this file need to be synced with,
// - "services/webnn/public/mojom/webnn_graph.mojom"
//
// Represents the `MLOperand` which describes not only input and constant
// operand, but also the output operand of operator.
struct Operand {
  // Represents the `MLOperandDataType` in the WebIDL definition.
  enum DataType {
    kMinValue = 0,
    kFloat32 = 0,
    kFloat16,
    kInt32,
    kUint32,
    kInt64,
    kUint64,
    kInt8,
    kUint8,
    kMaxValue = kUint8,
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

using DataTypeConstraintSet = base::EnumSet<Operand::DataType,
                                            Operand::DataType::kMinValue,
                                            Operand::DataType::kMaxValue>;

namespace DataTypeConstraint {

static constexpr DataTypeConstraintSet kFloat = {Operand::DataType::kFloat32,
                                                 Operand::DataType::kFloat16};

static constexpr DataTypeConstraintSet kFloat16To32Int8To32 = {
    Operand::DataType::kFloat32, Operand::DataType::kFloat16,
    Operand::DataType::kInt32, Operand::DataType::kInt8};

static constexpr DataTypeConstraintSet kGatherOperatorIndexDataTypes = {
    Operand::DataType::kInt32, Operand::DataType::kUint32,
    Operand::DataType::kInt64, Operand::DataType::kUint64};

}  // namespace DataTypeConstraint

std::string DataTypeConstraintToString(
    const DataTypeConstraintSet& constraint_set);

// Represents the `MLInputOperandLayout` that specifies the layout format of
// the input tensor. N is the batch, C is input channels, H is height and W is
// the width of the tensor.
enum class InputOperandLayout { kNchw, kNhwc };

// Represents the `MLConv2dFilterOperandLayout` that specifies the layout format
// of the filter tensor. O is output channels, I is input channels / groups, H
// is height and W is the width of filter.
enum class Conv2dFilterOperandLayout { kOihw, kHwio, kOhwi, kIhwo };

// Represents the `MLConvTranspose2dFilterOperandLayout` that specifies the
// layout format of the filter tensor. I is input channels, O is output channels
// / groups, H is height and W is the width of filter.
enum class ConvTranspose2dFilterOperandLayout { kIohw, kHwoi, kOhwi };

// Represents the `MLRoundingType` that is used to compute the output shape.
enum class RoundingType { kFloor, kCeil };

// Represents the `MLRecurrentNetworkDirection` that specifies the processing
// direction of the input sequence.
enum class RecurrentNetworkDirection { kForward, kBackward, kBoth };

enum ReduceKind {
  kL1,
  kL2,
  kLogSum,
  kLogSumExp,
  kMax,
  kMean,
  kMin,
  kProduct,
  kSum,
  kSumSquare
};

// A size has height and width values.
template <typename T>
struct Size2d {
  T height;
  T width;
};

// The additional rows and columns added to the beginning and ending of each
// spatial dimension of input.
struct Padding2d {
  // The height and width padding at the beginning of input tensor.
  Size2d<uint32_t> beginning;
  // The height and width padding at the ending of input tensor.
  Size2d<uint32_t> ending;
};

// Contains the attributes of batchNormalization operator.
struct BatchNormalizationAttributes {
  BatchNormalizationAttributes();
  ~BatchNormalizationAttributes();

  BatchNormalizationAttributes(BatchNormalizationAttributes&& other);
  BatchNormalizationAttributes& operator=(BatchNormalizationAttributes&& other);

  BatchNormalizationAttributes(const BatchNormalizationAttributes&) = delete;
  BatchNormalizationAttributes& operator=(const BatchNormalizationAttributes&) =
      delete;

  // The 1-D tensor of the scaling values.
  std::optional<Operand> scale;
  // The 1-D tensor of the bias values.
  std::optional<Operand> bias;
  // The number which specifies the index to the feature count dimension of the
  // input shape for which the mean and variance values are.
  uint32_t axis = 1;
};

// Contains the attributes of conv2d operator.
struct Conv2dAttributesBase {
  Conv2dAttributesBase();
  ~Conv2dAttributesBase();

  Conv2dAttributesBase(Conv2dAttributesBase&& other);
  Conv2dAttributesBase& operator=(Conv2dAttributesBase&& other);

  Conv2dAttributesBase(const Conv2dAttributesBase&) = delete;
  Conv2dAttributesBase& operator=(const Conv2dAttributesBase&) = delete;

  // The additional rows and columns added to the beginning and ending of each
  // spatial dimension of input.
  Padding2d padding;
  // The stride of the sliding window for each spatial dimension of input.
  Size2d<uint32_t> strides;
  // The dilation factor for each spatial dimension of input.
  Size2d<uint32_t> dilations;
  // The number of groups that input channels and output channels are divided
  // into.
  uint32_t groups = 1;
  // The layout format of the input.
  InputOperandLayout input_layout = InputOperandLayout::kNchw;
  // The additional 1-D tensor with the shape of [output_channels] whose values
  // are to be added to the convolution result.
  std::optional<Operand> bias_operand;
};

// Contains the attributes of conv2d operator.
struct Conv2dAttributes : Conv2dAttributesBase {
  Conv2dAttributes();
  ~Conv2dAttributes();

  Conv2dAttributes(Conv2dAttributes&& other);
  Conv2dAttributes& operator=(Conv2dAttributes&& other);

  Conv2dAttributes(const Conv2dAttributes&) = delete;
  Conv2dAttributes& operator=(const Conv2dAttributes&) = delete;

  // The layout format of the conv2d filter.
  Conv2dFilterOperandLayout filter_layout = Conv2dFilterOperandLayout::kOihw;
};

// Contains the attributes of convTranspose2d operator.
struct ConvTranspose2dAttributes : Conv2dAttributesBase {
  ConvTranspose2dAttributes();
  ~ConvTranspose2dAttributes();

  ConvTranspose2dAttributes(ConvTranspose2dAttributes&& other);
  ConvTranspose2dAttributes& operator=(ConvTranspose2dAttributes&& other);

  ConvTranspose2dAttributes(const ConvTranspose2dAttributes&) = delete;
  ConvTranspose2dAttributes& operator=(const ConvTranspose2dAttributes&) =
      delete;

  // The padding values applied to each spatial dimension of the output tensor.
  Size2d<uint32_t> output_padding;
  // The sizes of the last two dimensions of the output tensor.
  std::optional<Size2d<uint32_t>> output_sizes;
  // The layout format of the convTranspose2d filter.
  ConvTranspose2dFilterOperandLayout filter_layout =
      ConvTranspose2dFilterOperandLayout::kIohw;
};

// Contains the attributes of pool2d operator.
struct Pool2dAttributes {
  // The dimensions of the sliding window.
  std::optional<Size2d<uint32_t>> window_dimensions;
  // The additional rows and columns added to the beginning and ending of each
  // spatial dimension of input.
  Padding2d padding;
  // The element stride of the sliding window for each spatial dimension of
  // input.
  Size2d<uint32_t> strides;
  // The dilation factor for each spatial dimension of input.
  Size2d<uint32_t> dilations;
  // The layout format of the input.
  InputOperandLayout layout = InputOperandLayout::kNchw;
  // The rounding function used to compute the output shape.
  RoundingType rounding_type = RoundingType::kFloor;
  // The element height and width of the output tensor.
  std::optional<Size2d<uint32_t>> output_sizes;
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
  std::optional<Operand> c_operand;
  // A float scalar multiplier for the `A * B`.
  float alpha = 1.0;
  // A float scalar multiplier for the third tensor.
  float beta = 1.0;
  // True is to transpose the first tensor matrix multiplication.
  bool a_transpose = false;
  // True is to transpose the second tensor matrix multiplication.
  bool b_transpose = false;
};

// Contains the attributes of gru operator.
struct GruAttributes {
  GruAttributes();
  ~GruAttributes();

  GruAttributes(GruAttributes&& other);
  GruAttributes& operator=(GruAttributes&& other);

  GruAttributes(const GruAttributes&) = delete;
  GruAttributes& operator=(const GruAttributes&) = delete;

  // The bias operand.
  std::optional<Operand> bias;
  // The recurrent bias operand.
  std::optional<Operand> recurrent_bias;
  // The initial hidden state operand.
  std::optional<Operand> initial_hidden_state;
  // Indicates whether to return the outputs of the entire sequence.
  bool return_sequence;
  // Specifies the processing direction of the input sequence.
  RecurrentNetworkDirection direction;
  // The number of activations.
  uint32_t activation_count;
};

// Contains the attributes of gruCell operator.
struct GruCellAttributes {
  GruCellAttributes();
  ~GruCellAttributes();

  GruCellAttributes(GruCellAttributes&& other);
  GruCellAttributes& operator=(GruCellAttributes&& other);

  GruCellAttributes(const GruCellAttributes&) = delete;
  GruCellAttributes& operator=(const GruCellAttributes&) = delete;

  // The bias operand.
  std::optional<Operand> bias;
  // The recurrent bias operand.
  std::optional<Operand> recurrent_bias;
  // The number of activations.
  uint32_t activation_count;
};

// Contains the attributes of instanceNormalization operator.
struct InstanceNormalizationAttributes {
  InstanceNormalizationAttributes();
  ~InstanceNormalizationAttributes();

  InstanceNormalizationAttributes(InstanceNormalizationAttributes&& other);
  InstanceNormalizationAttributes& operator=(
      InstanceNormalizationAttributes&& other);

  InstanceNormalizationAttributes(const InstanceNormalizationAttributes&) =
      delete;
  InstanceNormalizationAttributes& operator=(
      const InstanceNormalizationAttributes&) = delete;

  // The scale operand.
  std::optional<Operand> scale;
  // The bias operand.
  std::optional<Operand> bias;
  // The layout format of the input.
  InputOperandLayout layout = InputOperandLayout::kNchw;
};

// Contains the attributes of layerNormalization operator.
struct LayerNormalizationAttributes {
  LayerNormalizationAttributes();
  ~LayerNormalizationAttributes();

  LayerNormalizationAttributes(LayerNormalizationAttributes&& other);
  LayerNormalizationAttributes& operator=(LayerNormalizationAttributes&& other);

  LayerNormalizationAttributes(const LayerNormalizationAttributes&) = delete;
  LayerNormalizationAttributes& operator=(const LayerNormalizationAttributes&) =
      delete;

  // The scale operand.
  std::optional<Operand> scale;
  // The bias operand.
  std::optional<Operand> bias;
};

struct LstmAttributes {
  LstmAttributes();
  ~LstmAttributes();

  LstmAttributes(LstmAttributes&& other);
  LstmAttributes& operator=(LstmAttributes&& other);

  LstmAttributes(const LstmAttributes&) = delete;
  LstmAttributes& operator=(const LstmAttributes&) = delete;

  // The bias operand.
  std::optional<Operand> bias;
  // The recurrent bias operand.
  std::optional<Operand> recurrent_bias;
  // The peephole weight operand.
  std::optional<Operand> peephole_weight;
  // The initial hidden state operand.
  std::optional<Operand> initial_hidden_state;
  // The initial cell state operand.
  std::optional<Operand> initial_cell_state;
  // The number of activations.
  size_t activation_count;
  // Indicates whether to return the outputs of the entire sequence.
  bool return_sequence;
  // The processing direction of the input sequence.
  RecurrentNetworkDirection direction;
};

struct LstmCellAttributes {
  LstmCellAttributes();
  ~LstmCellAttributes();

  LstmCellAttributes(LstmCellAttributes&& other);
  LstmCellAttributes& operator=(LstmCellAttributes&& other);

  LstmCellAttributes(const LstmCellAttributes&) = delete;
  LstmCellAttributes& operator=(const LstmCellAttributes&) = delete;

  // The bias operand.
  std::optional<Operand> bias;
  // The recurrent bias operand.
  std::optional<Operand> recurrent_bias;
  // The peephole weight operand.
  std::optional<Operand> peephole_weight;
  // The number of activations.
  size_t activation_count;
};

struct SliceAttributes {
  SliceAttributes();
  ~SliceAttributes();

  SliceAttributes(SliceAttributes&& other);
  SliceAttributes& operator=(SliceAttributes&& other);

  SliceAttributes(const SliceAttributes&) = delete;
  SliceAttributes& operator=(const SliceAttributes&) = delete;

  // The sequence of unsigned integer values indicating the starting index to
  // slice of each input dimension.
  std::vector<uint32_t> starts;
  // The sequence of unsigned integer values indicating the number of elements
  // to slice of each input dimension.
  std::vector<uint32_t> sizes;
};

// Validate and infer output information of argMin and argMax operator defined
// in
// https://pr-preview.s3.amazonaws.com/webmachinelearning/webnn/pull/478.html#api-mlgraphbuilder-argminmax
base::expected<Operand, std::string> ValidateArgMinMaxAndInferOutput(
    const Operand& input,
    base::span<const uint32_t> axes,
    bool keep_dimensions = false);

// Validate softmax operator defined in WebIDL here
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-softmax
base::expected<Operand, std::string> ValidateSoftmaxAndInferOutput(
    Operand input);

// Contains the attributes of the split operator.
struct SplitAttribute {
  // splits defines how the input tensor will be split.
  //  uint32_t: The input tensor will be split into splits number of outputs
  //   with equal sizes.
  //  base::span<const uint32_t>: The input tensor will be split into
  //   splits.size() number of outputs with sizes specified in splits.
  absl::variant<uint32_t, base::span<const uint32_t>> splits;
  // Axis specifies which input tensor dimension will be split.
  uint32_t axis = 0;
};

// Validate and infer the output tensors' ranks and sizes for split operator
// based on the WebNN WebIDL
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-split
base::expected<std::vector<Operand>, std::string> ValidateSplitAndInferOutput(
    const Operand& input,
    const SplitAttribute& attributes);

// Validate and infer output information of batchNormalization operator defined
// in WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-batchnorm.
base::expected<Operand, std::string> ValidateBatchNormalizationAndInferOutput(
    const Operand& input,
    const Operand& mean,
    const Operand& variance,
    const BatchNormalizationAttributes& attributes);

// Validate and infer output information of 2-D convolution operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-conv2d
base::expected<Operand, std::string> ValidateConv2dAndInferOutput(
    const Operand& input,
    const Operand& filter,
    const Conv2dAttributes& attributes);

// Validate and infer output information of 2-D transposed convolution operator
// defined in WebIDL here
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-convtranspose2d
base::expected<Operand, std::string> ValidateConvTranspose2dAndInferOutput(
    const Operand& input,
    const Operand& filter,
    const ConvTranspose2dAttributes& attributes);

// Validate and infer output information of pad operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-pad
base::expected<Operand, std::string> ValidatePadAndInferOutput(
    const Operand& input,
    base::span<const uint32_t> beginning_padding,
    base::span<const uint32_t> ending_padding);

// Validate and infer output information of matmul operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-matmul
base::expected<Operand, std::string> ValidateMatmulAndInferOutput(
    const Operand& a,
    const Operand& b);

// Validate and infer output information of 2-D pooling operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-pool2d
base::expected<Operand, std::string> ValidatePool2dAndInferOutput(
    const Operand& input,
    const Pool2dAttributes& attributes);

// Validate and infer output information of 2-D resample operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-resample2d
base::expected<Operand, std::string> ValidateResample2dAndInferOutput(
    const Operand& input,
    const absl::variant<base::span<const float>, base::span<const uint32_t>>&
        scales_or_sizes,
    base::span<const uint32_t> axes);

// Validate and infer output information of gather operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-gather
base::expected<Operand, std::string> ValidateGatherAndInferOutput(
    const Operand& input,
    const Operand& indices,
    const uint32_t axis);

// Validate gemm operator defined in WebIDL here
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-gemm
base::expected<Operand, std::string> ValidateGemmAndInferOutput(
    const Operand& a,
    const Operand& b,
    const GemmAttributes& attributes);

// Validate and infer output information of gru operator defined in WebIDL here
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-gru.
base::expected<std::vector<Operand>, std::string> ValidateGruAndInferOutput(
    const Operand& input,
    const Operand& weight,
    const Operand& recurrent_weight,
    uint32_t steps,
    uint32_t hidden_size,
    const GruAttributes& attributes);

// Validate and infer output information of gruCell operator defined in WebIDL
// here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-grucell.
base::expected<Operand, std::string> ValidateGruCellAndInferOutput(
    const Operand& input,
    const Operand& weight,
    const Operand& recurrent_weight,
    const Operand& hidden_state,
    uint32_t hidden_size,
    const GruCellAttributes& attributes);

// Validate and infer output information of instanceNormalization operator
// defined in WebIDL here
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-instancenorm.
base::expected<Operand, std::string>
ValidateInstanceNormalizationAndInferOutput(
    const Operand& input,
    const InstanceNormalizationAttributes& attributes);

// Validate and infer output information of layerNormalization operator defined
// in WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-layernorm.
base::expected<Operand, std::string> ValidateLayerNormalizationAndInferOutput(
    const Operand& input,
    base::span<const uint32_t> axes,
    const LayerNormalizationAttributes& attributes);

// Validate and infer output information of lstm operator defined
// in WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-lstm.
base::expected<std::vector<Operand>, std::string> ValidateLstmAndInferOutput(
    const Operand& input,
    const Operand& weight,
    const Operand& recurrent_weight,
    const uint32_t steps,
    const uint32_t hidden_size,
    const LstmAttributes& attributes);

// Validate and infer output information of lstmCell operator defined
// in WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-lstmcell.
base::expected<std::vector<Operand>, std::string>
ValidateLstmCellAndInferOutput(const Operand& input,
                               const Operand& weight,
                               const Operand& recurrent_weight,
                               const Operand& hidden_state,
                               const Operand& cell_state,
                               const uint32_t hidden_size,
                               const LstmCellAttributes& attributes);

// Validate concat operator defined in WebIDL here
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-concat
base::expected<Operand, std::string> ValidateConcatAndInferOutput(
    const std::vector<Operand>& input,
    const uint32_t axis);

// Validate prelu operator defined in WebIDL here:
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-prelu
base::expected<Operand, std::string> ValidatePreluAndInferOutput(
    const Operand& input,
    const Operand& slope);

// Validate transpose operator defined in WebIDL here
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-transpose
base::expected<Operand, std::string> ValidateTransposeAndInferOutput(
    const Operand& input,
    base::span<const uint32_t> permutation);

// Validate slice operator defined in WebIDL here:
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-slice
base::expected<Operand, std::string> ValidateSliceAndInferOutput(
    const Operand& input,
    const SliceAttributes& attributes);

// Validate and infer output information of reduce operator defined in
// WebIDL here https://www.w3.org/TR/webnn/#api-mlgraphbuilder-reduce
base::expected<Operand, std::string> ValidateReduceAndInferOutput(
    ReduceKind kind,
    const Operand& input,
    base::span<const uint32_t> axes,
    bool keepDimensions = false);

// Validate triangular operator defined in WebIDL here
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-triangular.
base::expected<Operand, std::string> ValidateTriangularAndInferOutput(
    Operand input);

// TODO(crbug.com/40206287): Add the link of the where operator definition in
// WebIDL.
// Validate where operator.
base::expected<Operand, std::string> ValidateWhereAndInferOutput(
    const Operand& condition,
    const Operand& true_value,
    const Operand& false_value);

base::expected<size_t, std::string> ValidateAndCalculateElementsNumber(
    base::span<const uint32_t> dimensions);

base::expected<size_t, std::string> ValidateAndCalculateByteLength(
    size_t type_bytes,
    base::span<const uint32_t> dimensions);

// Validate that the axes are within the range of [0, rank - 1] without
// duplication.
base::expected<void, std::string> ValidateAxes(base::span<const uint32_t> axes,
                                               const size_t rank);

// Broadcast the input shapes and return the output shape.
// If bidirectional is true, its behavior follows the numpy-broadcasting-rule:
// https://numpy.org/doc/stable/user/basics.broadcasting.html#general-broadcasting-rules.
// Otherwise, it unidirectionally broadcasts the lhs to the rhs.
std::optional<std::vector<uint32_t>> BroadcastShapes(
    base::span<const uint32_t> dims_lhs,
    base::span<const uint32_t> dims_rhs,
    bool bidirectional = true);

// Calculate the output size for convTranspose2d based on WebNN spec:
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-convtranspose2d
// Return the calculated output size if no error.
base::expected<uint32_t, std::string> CalculateConvTranspose2dOutputSize(
    const uint32_t input_size,
    const uint32_t filter_size,
    const uint32_t beginning_padding,
    const uint32_t ending_padding,
    const uint32_t stride,
    const uint32_t dilation,
    const uint32_t output_padding);

bool IsFloatingPointType(Operand::DataType data_type);

// A depthwise conv2d operation is a variant of grouped convolution where the
// options.groups == input_channels == output_channels according to WebNN conv2d
// spec: https://www.w3.org/TR/webnn/#api-mlgraphbuilder-conv2d.
bool IsDepthwiseConv2d(uint32_t input_channels,
                       uint32_t output_channels,
                       uint32_t groups);

}  // namespace webnn

#endif  // COMPONENTS_ML_WEBNN_GRAPH_VALIDATION_UTILS_H_
