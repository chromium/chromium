// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/embedding_lookup.h"

#include <type_traits>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "components/language_detection/core/quantization_utils.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flexbuffers.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/internal/tensor_ctypes.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/kernel_util.h"

namespace language_detection {

namespace {

using ::flexbuffers::GetRoot;
using ::flexbuffers::Map;
constexpr int kInputMessage = 0;
constexpr int kEmbeddingTable = 1;
constexpr int kMinVal = 2;
constexpr int kMaxVal = 3;
constexpr int kOutputLabel = 0;
constexpr int kNumFloatBits = 8 * sizeof(float);

class EmbeddingLookupOpParams {
 public:
  explicit EmbeddingLookupOpParams(const bool is_quantized,
                                   const int num_precision_bits)
      : is_quantized_(is_quantized), num_precision_bits_(num_precision_bits) {}
  bool IsQuantized() const { return is_quantized_; }
  int GetNumBits() const { return num_precision_bits_; }
  TfLiteStatus Validate(TfLiteContext* context) const {
    // Validate that the `num_precision_bits` and `is_quantized` are set to
    // sane values.
    if (!is_quantized_)
      return kTfLiteOk;

    if (!(num_precision_bits_ >= 2 && num_precision_bits_ < 32 &&
          (32 % num_precision_bits_) == 0)) {
      context->ReportError(
          context,
          "`num_precision_bits` must be in [2, 32) and a divisor of 32.");
      return kTfLiteError;
    }

    return kTfLiteOk;
  }

 private:
  const bool is_quantized_;
  const int num_precision_bits_;
};

int GetOutputEmbeddingSize(const int input_embedding_size,
                           const bool is_quantized,
                           const int num_precision_bits) {
  DCHECK_GT(num_precision_bits, 0);
  return is_quantized
             ? (input_embedding_size * kNumFloatBits) / num_precision_bits
             : input_embedding_size;
}

void* Init(TfLiteContext* context, const char* buffer, size_t length) {
  const uint8* buffer_t = reinterpret_cast<const uint8*>(buffer);
  const Map& m = GetRoot(buffer_t, length).AsMap();
  const bool is_quantized =
      !m["is_quantized"].IsNull() && m["is_quantized"].AsBool();
  const int num_precision_bits = m["num_precision_bits"].AsInt32();
  return new EmbeddingLookupOpParams(is_quantized, num_precision_bits);
}

void Free(TfLiteContext* context, void* buffer) {
  delete reinterpret_cast<EmbeddingLookupOpParams*>(buffer);
}

template <typename T, typename TensorType>
  requires(std::is_same_v<std::remove_const_t<TensorType>, TfLiteTensor>)
auto GetTensorDataSpan(TensorType* tensor) {
  CHECK_EQ(tensor->bytes % sizeof(T), 0u);
  // SAFETY: We have checked that the size of `data` (given by `tensor->bytes`)
  // divides cleanly by the sizeof T so this buffer is promised to be in bounds.
  return UNSAFE_BUFFERS(
      base::span(tflite::GetTensorData<T>(tensor), tensor->bytes / sizeof(T)));
}

template <typename TfLiteIntArrayType>
  requires(
      std::is_same_v<std::remove_const_t<TfLiteIntArrayType>, TfLiteIntArray>)
auto GetSpanFromTfLiteIntArray(TfLiteIntArrayType* array) {
  // SAFETY: TfLite stores the size with the TfLiteIntArray.
  return UNSAFE_BUFFERS(
      base::span(array->data, base::checked_cast<size_t>(array->size)));
}

TfLiteStatus Resize(TfLiteContext* context, TfLiteNode* node) {
  TfLiteTensor* output = tflite::GetOutput(context, node, kOutputLabel);
  TF_LITE_ENSURE(context, output != nullptr);
  const EmbeddingLookupOpParams* params =
      reinterpret_cast<EmbeddingLookupOpParams*>(node->user_data);
  TF_LITE_ENSURE_OK(context, params->Validate(context));
  TfLiteIntArray* output_size = TfLiteIntArrayCreate(2);
  base::span<int> output_data = GetSpanFromTfLiteIntArray(output_size);
  output_data[0] = 1;
  const TfLiteTensor* input_tensor =
      tflite::GetInput(context, node, kEmbeddingTable);
  TF_LITE_ENSURE(context, input_tensor != nullptr);
  const int input_embedding_size =
      GetSpanFromTfLiteIntArray(input_tensor->dims)[1];
  output_data[1] = GetOutputEmbeddingSize(
      input_embedding_size, params->IsQuantized(), params->GetNumBits());
  return context->ResizeTensor(context, output, output_size);
}

// This is the core method that generates the aggregated embedding from the
// given input and embedding table tensors.
//
// If `is_quantized` is set to false, the `embedding_table` is considered to
// be a regular floating-point tensor, with each row representing an
// embedding vector, and each element in the vector is an embedding dimension.
//
// If `is_quantized` is set to true, the `embedding_table` is considered to be
// a packed quantized tensor, with each row still representing an embedding
// vector. However, each element in the vector contains 'packed' n-bit quantized
// representation of m embedding dimensions.
//
// n = `num_precision_bits`,
// m = 32 / n.
void GetEmbedding(const TfLiteTensor* input,
                  const TfLiteTensor* embedding_table,
                  const float min_val,
                  const float max_val,
                  const EmbeddingLookupOpParams* params,
                  TfLiteTensor* output) {
  const bool is_quantized = params->IsQuantized();
  const int num_precision_bits = params->GetNumBits();
  const int input_embedding_size =
      GetSpanFromTfLiteIntArray(embedding_table->dims)[1];
  const int num_tokens = GetSpanFromTfLiteIntArray(input->dims)[1];
  const size_t output_embedding_size =
      base::checked_cast<size_t>(GetOutputEmbeddingSize(
          input_embedding_size, is_quantized, num_precision_bits));
  int num_embeddings = 0;
  std::vector<float> final_embedding(output_embedding_size, 0.0);
  base::span<const uint32> embedding_table_data =
      GetTensorDataSpan<uint32>(embedding_table);
  base::span<const int32> input_data = GetTensorDataSpan<int32>(input);
  for (int token_idx = 0; token_idx < num_tokens; token_idx++) {
    const int32 token = input_data[token_idx];
    if (token == 0) {
      break;
    }
    if (is_quantized) {
      // The embedding table contains the packed quantized representation of the
      // embedding table.
      const int compression_factor = 32 / num_precision_bits;
      const uint32 mask = (1L << num_precision_bits) - 1;
      const QuantizationParams quant_params =
          GetQuantizationParams(min_val, max_val, num_precision_bits);
      for (int embed_idx = 0; embed_idx < input_embedding_size; embed_idx++) {
        // Extract the packed embedding at the given index.
        uint32 packed_embedding =
            embedding_table_data[token * input_embedding_size + embed_idx];
        for (int num_dims_extracted = 0;
             num_dims_extracted < compression_factor; num_dims_extracted++) {
          uint32 quantized_val = (packed_embedding & mask);
          // Dequantize the quantized value, so that we can get an approximation
          // for the original value.
          float dequantized_value =
              QuantizedToFloatWithQuantParams(quantized_val, quant_params);
          final_embedding[embed_idx * compression_factor +
                          num_dims_extracted] += dequantized_value;
          packed_embedding >>= num_precision_bits;
        }
      }
    } else {
      // The embedding table is stored uncompressed.
      for (int embed_idx = 0; embed_idx < input_embedding_size; embed_idx++) {
        // Extract the raw value of the dimension in the embedding table.
        const float raw_dim_value = UNSAFE_TODO(
            embedding_table->data.f[token * input_embedding_size + embed_idx]);
        final_embedding[embed_idx] += raw_dim_value;
      }
    }
    ++num_embeddings;
  }

  base::span<float> output_data =
      GetTensorDataSpan<float>(output).first(output_embedding_size);
  // Compute the mean of the embeddings.
  for (size_t embed_idx = 0; embed_idx < output_embedding_size; embed_idx++) {
    output_data[embed_idx] =
        final_embedding[embed_idx] / (std::max(num_embeddings, 1));
  }
}

TfLiteStatus Eval(TfLiteContext* context, TfLiteNode* node) {
  const EmbeddingLookupOpParams* params =
      reinterpret_cast<EmbeddingLookupOpParams*>(node->user_data);
  TF_LITE_ENSURE_OK(context, params->Validate(context));
  const TfLiteTensor* input = tflite::GetInput(context, node, kInputMessage);
  TF_LITE_ENSURE(context, input != nullptr);
  const TfLiteTensor* embedding_table =
      tflite::GetInput(context, node, kEmbeddingTable);
  TF_LITE_ENSURE(context, embedding_table != nullptr);
  const TfLiteTensor* min_val = tflite::GetInput(context, node, kMinVal);
  TF_LITE_ENSURE(context, min_val != nullptr);
  const TfLiteTensor* max_val = tflite::GetInput(context, node, kMaxVal);
  TF_LITE_ENSURE(context, max_val != nullptr);
  TfLiteTensor* output = tflite::GetOutput(context, node, kOutputLabel);
  TF_LITE_ENSURE(context, output != nullptr);
  // Sanity checks on the input.
  const int batch_size = input->dims->data[0];
  if (batch_size != 1) {
    context->ReportError(context, "`batch_size` must be == 1.");
    return kTfLiteError;
  }
  if (output->type != kTfLiteFloat32) {
    context->ReportError(context, "Output type must be Float32.");
    return kTfLiteError;
  }
  // Compute the output embedding.
  GetEmbedding(input, embedding_table, GetTensorDataSpan<float>(min_val)[0],
               GetTensorDataSpan<float>(max_val)[0], params, output);
  return kTfLiteOk;
}

}  // namespace

TfLiteRegistration* Register_EMBEDDING_LOOKUP() {
  static TfLiteRegistration r = {Init, Free, Resize, Eval};
  return &r;
}

}  // namespace language_detection
