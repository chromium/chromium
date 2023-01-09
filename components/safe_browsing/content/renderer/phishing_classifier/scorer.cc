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
  resolver.AddBuiltin(tflite::BuiltinOperator_MEAN,
                      tflite::ops::builtin::Register_MEAN(),
                      /* min_version = */ 1,
                      /* max_version = */ 2);
  resolver.AddBuiltin(tflite::BuiltinOperator_SOFTMAX,
                      tflite::ops::builtin::Register_SOFTMAX(),
                      /* min_version = */ 1,
                      /* max_version = */ 3);
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

void OnModelInputCreated(
    const std::string& model_input,
    int input_width,
    int input_height,
    std::unique_ptr<tflite::task::vision::ImageClassifier> classifier,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(std::vector<double>)> callback) {
  base::Time before_operation = base::Time::Now();
  tflite::task::vision::FrameBuffer::Plane plane{
      reinterpret_cast<const tflite::uint8*>(model_input.data()),
      {3 * input_width, 3}};
  auto frame_buffer = tflite::task::vision::FrameBuffer::Create(
      {plane}, {input_width, input_height},
      tflite::task::vision::FrameBuffer::Format::kRGB,
      tflite::task::vision::FrameBuffer::Orientation::kTopLeft);
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

void OnClassifierCreated(
    const SkBitmap& bitmap,
    int input_width,
    int input_height,
    std::unique_ptr<tflite::task::vision::ImageClassifier> classifier,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(std::vector<double>)> callback) {
  base::Time before_operation = base::Time::Now();
  std::string model_input = GetModelInput(bitmap, input_width, input_height);
  if (model_input.empty()) {
    callback_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::vector<double>()));
    return;
  }
  base::UmaHistogramTimes("SBClientPhishing.ApplyTfliteTime.GetModelInput",
                          base::Time::Now() - before_operation);

  // Break up the task to avoid blocking too long.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&OnModelInputCreated, std::move(model_input), input_width,
                     input_height, std::move(classifier),
                     std::move(callback_task_runner), std::move(callback)));
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
  base::Time before_operation = base::Time::Now();
  before_operation = base::Time::Now();
  std::unique_ptr<tflite::task::vision::ImageClassifier> classifier =
      CreateClassifier(std::move(model_data));
  base::UmaHistogramTimes("SBClientPhishing.ApplyTfliteTime.CreateClassifier",
                          base::Time::Now() - before_operation);
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
