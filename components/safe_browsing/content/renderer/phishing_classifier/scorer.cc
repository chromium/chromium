// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"

#include <math.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/content/common/visual_utils.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/renderer/render_thread.h"
#include "crypto/sha2.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorSpace.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "third_party/tflite/src/tensorflow/lite/kernels/builtin_op_kernels.h"
#include "third_party/tflite/src/tensorflow/lite/op_resolver.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/vision/image_classifier.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/vision/image_embedder.h"
#endif

namespace safe_browsing {

namespace {

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
std::unique_ptr<tflite::MutableOpResolver> CreateOpResolver() {
  tflite::MutableOpResolver resolver;
  // The minimal set of OPs required to run the visual model.
  resolver.AddBuiltin(tflite::BuiltinOperator_ADD,
                      tflite::ops::builtin::Register_ADD(),
                      /* min_version = */ 1,
                      /* max_version = */ 2);
  resolver.AddBuiltin(tflite::BuiltinOperator_AVERAGE_POOL_2D,
                      tflite::ops::builtin::Register_AVERAGE_POOL_2D(),
                      /* min_version */ 1,
                      /* max_version */ 3);
  resolver.AddBuiltin(tflite::BuiltinOperator_CONV_2D,
                      tflite::ops::builtin::Register_CONV_2D(),
                      /* min_version = */ 1,
                      /* max_version = */ 5);
  resolver.AddBuiltin(tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
                      tflite::ops::builtin::Register_DEPTHWISE_CONV_2D(),
                      /* min_version = */ 1,
                      /* max_version = */ 6);
  resolver.AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED,
                      tflite::ops::builtin::Register_FULLY_CONNECTED(),
                      /* min_version = */ 1,
                      /* max_version = */ 9);
  resolver.AddBuiltin(tflite::BuiltinOperator_LOGISTIC,
                      tflite::ops::builtin::Register_LOGISTIC(),
                      /* min_version = */ 1,
                      /* max_version = */ 3);
  resolver.AddBuiltin(tflite::BuiltinOperator_L2_NORMALIZATION,
                      tflite::ops::builtin::Register_L2_NORMALIZATION(), 1, 2);
  resolver.AddBuiltin(tflite::BuiltinOperator_MEAN,
                      tflite::ops::builtin::Register_MEAN(),
                      /* min_version = */ 1,
                      /* max_version = */ 2);
  resolver.AddBuiltin(tflite::BuiltinOperator_MUL,
                      tflite::ops::builtin::Register_MUL(),
                      /* min_version = */ 1,
                      /* max_version = */ 4);
  resolver.AddBuiltin(tflite::BuiltinOperator_RESHAPE,
                      tflite::ops::builtin::Register_RESHAPE());
  resolver.AddBuiltin(tflite::BuiltinOperator_SOFTMAX,
                      tflite::ops::builtin::Register_SOFTMAX(),
                      /* min_version = */ 1,
                      /* max_version = */ 3);
  resolver.AddBuiltin(tflite::BuiltinOperator_SUB,
                      tflite::ops::builtin::Register_SUB(), 1, 2);
  resolver.AddBuiltin(tflite::BuiltinOperator_DEQUANTIZE,
                      tflite::ops::builtin::Register_DEQUANTIZE(),
                      /* min_version = */ 1,
                      /* max_version = */ 4);
  resolver.AddBuiltin(tflite::BuiltinOperator_QUANTIZE,
                      tflite::ops::builtin::Register_QUANTIZE(),
                      /* min_version = */ 1,
                      /* max_version = */ 2);
  return std::make_unique<tflite::MutableOpResolver>(resolver);
}

std::unique_ptr<tflite::task::vision::ImageClassifier> CreateClassifier(
    std::string model_data) {
  TRACE_EVENT0("safe_browsing", "CreateTfLiteClassifier");
  tflite::task::vision::ImageClassifierOptions options;
  tflite::task::core::BaseOptions* base_options =
      options.mutable_base_options();
  base_options->mutable_model_file()->set_file_content(std::move(model_data));
  base_options->mutable_compute_settings()
      ->mutable_tflite_settings()
      ->mutable_cpu_settings()
      ->set_num_threads(1);
  auto statusor_classifier =
      tflite::task::vision::ImageClassifier::CreateFromOptions(
          options, CreateOpResolver());
  if (!statusor_classifier.ok()) {
    VLOG(1) << statusor_classifier.status().ToString();
    return nullptr;
  }

  return std::move(*statusor_classifier);
}

std::unique_ptr<tflite::task::vision::ImageEmbedder> CreateImageEmbedder(
    std::string model_data) {
  TRACE_EVENT0("safe_browsing", "CreateTfLiteImageEmbedder");
  tflite::task::vision::ImageEmbedderOptions embedder_options;
  embedder_options.mutable_model_file_with_metadata()->set_file_content(
      model_data);
  auto embedder = tflite::task::vision::ImageEmbedder::CreateFromOptions(
      embedder_options, CreateOpResolver());
  if (!embedder.ok()) {
    VLOG(1) << "Failed to create the embedder. Embedder status is: "
            << embedder.status().ToString();
    return nullptr;
  }

  return std::move(*embedder);
}

std::string GetModelInput(const SkBitmap& bitmap, int width, int height) {
  TRACE_EVENT0("safe_browsing", "GetTfLiteModelInput");
  // Use the Rec. 2020 color space, in case the user input is wide-gamut.
  sk_sp<SkColorSpace> rec2020 = SkColorSpace::MakeRGB(
      {2.22222f, 0.909672f, 0.0903276f, 0.222222f, 0.0812429f, 0, 0},
      SkNamedGamut::kRec2020);

  SkBitmap downsampled = skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_GOOD, static_cast<int>(width),
      static_cast<int>(height));

  // Format as an RGB buffer for input into the model
  std::string data;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      SkColor color = downsampled.getColor(x, y);
      data += static_cast<char>(SkColorGetR(color));
      data += static_cast<char>(SkColorGetG(color));
      data += static_cast<char>(SkColorGetB(color));
    }
  }

  return data;
}

auto CreateFrameBuffer(const std::string& model_input,
                       int input_width,
                       int input_height) {
  tflite::task::vision::FrameBuffer::Plane plane{
      reinterpret_cast<const tflite::uint8*>(model_input.data()),
      {3 * input_width, 3}};
  return tflite::task::vision::FrameBuffer::Create(
      {plane}, {input_width, input_height},
      tflite::task::vision::FrameBuffer::Format::kRGB,
      tflite::task::vision::FrameBuffer::Orientation::kTopLeft);
}

void OnModelInputCreatedForClassifier(
    const std::string& model_input,
    int input_width,
    int input_height,
    std::unique_ptr<tflite::task::vision::ImageClassifier> classifier,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(std::vector<double>)> callback) {
  base::Time before_operation = base::Time::Now();
  auto frame_buffer = CreateFrameBuffer(model_input, input_width, input_height);
  auto statusor_result = classifier->Classify(*frame_buffer);
  base::UmaHistogramTimes("SBClientPhishing.ApplyTfliteTime.Classify",
                          base::Time::Now() - before_operation);
  if (!statusor_result.ok()) {
    VLOG(1) << statusor_result.status().ToString();
    callback_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::vector<double>()));
    return;
  }

  std::vector<double> scores(
      statusor_result->classifications(0).classes().size());
  for (const tflite::task::vision::Class& clas :
       statusor_result->classifications(0).classes()) {
    scores[clas.index()] = clas.score();
  }

  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(scores)));
}

void OnModelInputCreatedForImageEmbedding(
    const std::string& model_input,
    int input_width,
    int input_height,
    std::unique_ptr<tflite::task::vision::ImageEmbedder> image_embedder,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(ImageFeatureEmbedding)> callback) {
  auto frame_buffer = CreateFrameBuffer(model_input, input_width, input_height);
  tflite::support::StatusOr<tflite::task::vision::EmbeddingResult>
      statusor_result = image_embedder->Embed(*frame_buffer);

  ImageFeatureEmbedding image_feature_embedding;

  if (!statusor_result.ok()) {
    VLOG(1) << "Embedding failed with the status "
            << statusor_result.status().ToString();
    callback_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), image_feature_embedding));
    return;
  }

  auto feature_vector = statusor_result->embeddings(0).feature_vector();

  std::vector<float> value_floats = std::vector<float>(
      feature_vector.value_float().begin(), feature_vector.value_float().end());
  for (float value : value_floats) {
    image_feature_embedding.add_embedding_value(value);
  }

  callback_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(image_feature_embedding)));
}

void OnClassifierCreated(
    const SkBitmap& bitmap,
    int input_width,
    int input_height,
    std::unique_ptr<tflite::task::vision::ImageClassifier> classifier,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(std::vector<double>)> callback) {
  std::string model_input = GetModelInput(bitmap, input_width, input_height);
  if (model_input.empty()) {
    callback_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::vector<double>()));
    return;
  }

  // Break up the task to avoid blocking too long.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&OnModelInputCreatedForClassifier, std::move(model_input),
                     input_width, input_height, std::move(classifier),
                     std::move(callback_task_runner), std::move(callback)));
}

void OnImageEmbedderCreated(
    const SkBitmap& bitmap,
    int input_width,
    int input_height,
    std::unique_ptr<tflite::task::vision::ImageEmbedder> image_embedder,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(ImageFeatureEmbedding)> callback) {
  std::string model_input = GetModelInput(bitmap, input_width, input_height);
  if (model_input.empty()) {
    callback_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), ImageFeatureEmbedding()));
    return;
  }

  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&OnModelInputCreatedForImageEmbedding,
                     std::move(model_input), input_width, input_height,
                     std::move(image_embedder), std::move(callback_task_runner),
                     std::move(callback)));
}
#endif

}  // namespace

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
void Scorer::ApplyVisualTfLiteModelHelper(
    const SkBitmap& bitmap,
    int input_width,
    int input_height,
    std::string model_data,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(std::vector<double>)> callback) {
  TRACE_EVENT0("safe_browsing", "ApplyVisualTfLiteModel");
  std::unique_ptr<tflite::task::vision::ImageClassifier> classifier =
      CreateClassifier(std::move(model_data));
  if (!classifier) {
    callback_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::vector<double>()));
    return;
  }

  // Break up the task to avoid blocking too long.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&OnClassifierCreated, bitmap, input_width, input_height,
                     std::move(classifier), std::move(callback_task_runner),
                     std::move(callback)));
}

void Scorer::ApplyImageEmbeddingTfLiteModelHelper(
    const SkBitmap& bitmap,
    int input_width,
    int input_height,
    const std::string& model_data,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(ImageFeatureEmbedding)> callback) {
  TRACE_EVENT0("safe_browsing", "ApplyImageEmbeddingTfLiteModel");
  std::unique_ptr<tflite::task::vision::ImageEmbedder> image_embedder =
      CreateImageEmbedder(std::move(model_data));

  if (!image_embedder) {
    callback_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), ImageFeatureEmbedding()));
    return;
  }
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&OnImageEmbedderCreated, bitmap, input_width, input_height,
                     std::move(image_embedder), std::move(callback_task_runner),
                     std::move(callback)));
}
#endif

double Scorer::LogOdds2Prob(double log_odds) {
  // 709 = floor(1023*ln(2)).  2**1023 is the largest finite double.
  // Small log odds aren't a problem.  as the odds will be 0.  It's only
  // when we get +infinity for the odds, that odds/(odds+1) would be NaN.
  if (log_odds >= 709) {
    return 1.0;
  }
  double odds = exp(log_odds);
  return odds / (odds + 1.0);
}

Scorer::Scorer() = default;
Scorer::~Scorer() = default;

// static
ScorerStorage* ScorerStorage::GetInstance() {
  static base::NoDestructor<ScorerStorage> instance;
  return instance.get();
}

ScorerStorage::ScorerStorage() = default;
ScorerStorage::~ScorerStorage() = default;

void ScorerStorage::SetScorer(std::unique_ptr<Scorer> scorer) {
  scorer_ = std::move(scorer);
  for (Observer& obs : observers_)
    obs.OnScorerChanged();
}

Scorer* ScorerStorage::GetScorer() const {
  return scorer_.get();
}

void ScorerStorage::AddObserver(ScorerStorage::Observer* observer) {
  observers_.AddObserver(observer);
}

void ScorerStorage::RemoveObserver(ScorerStorage::Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace safe_browsing
