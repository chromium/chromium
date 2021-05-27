// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"

#include <math.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/features.h"
#include "components/safe_browsing/core/common/visual_utils.h"
#include "components/safe_browsing/core/proto/client_model.pb.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "content/public/renderer/render_thread.h"
#include "crypto/sha2.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/tflite-support/src/tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "third_party/tflite-support/src/tensorflow_lite_support/cc/task/vision/image_classifier.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/builtin_op_kernels.h"
#include "third_party/tflite/src/tensorflow/lite/op_resolver.h"

namespace safe_browsing {

namespace {

std::unique_ptr<tflite::MutableOpResolver> CreateOpResolver() {
  tflite::MutableOpResolver resolver;
  // The minimal set of OPs required to run the visual model.
  resolver.AddBuiltin(tflite::BuiltinOperator_ADD,
                      tflite::ops::builtin::Register_ADD());
  resolver.AddBuiltin(tflite::BuiltinOperator_CONV_2D,
                      tflite::ops::builtin::Register_CONV_2D());
  resolver.AddBuiltin(tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
                      tflite::ops::builtin::Register_DEPTHWISE_CONV_2D());
  resolver.AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED,
                      tflite::ops::builtin::Register_FULLY_CONNECTED());
  resolver.AddBuiltin(tflite::BuiltinOperator_MEAN,
                      tflite::ops::builtin::Register_MEAN());
  resolver.AddBuiltin(tflite::BuiltinOperator_SOFTMAX,
                      tflite::ops::builtin::Register_SOFTMAX());
  return std::make_unique<tflite::MutableOpResolver>(resolver);
}

std::unique_ptr<tflite::task::vision::ImageClassifier> CreateClassifier(
    const std::string& model_data) {
  TRACE_EVENT0("safe_browsing", "CreateTfLiteClassifier");
  tflite::task::vision::ImageClassifierOptions options;
  options.mutable_model_file_with_metadata()->set_file_content(model_data);
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

  SkImageInfo downsampled_info = SkImageInfo::MakeN32(
      width, height, SkAlphaType::kUnpremul_SkAlphaType, rec2020);
  SkBitmap downsampled;
  if (!downsampled.tryAllocPixels(downsampled_info))
    return std::string();
  bitmap.pixmap().scalePixels(
      downsampled.pixmap(),
      SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNearest));

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

}  // namespace

std::vector<double> Scorer::ApplyVisualTfLiteModelHelper(
    const SkBitmap& bitmap,
    int input_width,
    int input_height,
    const std::string& model_data) {
  TRACE_EVENT0("safe_browsing", "ApplyVisualTfLiteModel");
  std::unique_ptr<tflite::task::vision::ImageClassifier> classifier =
      CreateClassifier(model_data);
  if (!classifier)
    return std::vector<double>();

  std::string model_input = GetModelInput(bitmap, input_width, input_height);
  if (model_input.empty())
    return std::vector<double>();

  tflite::task::vision::FrameBuffer::Plane plane{
      reinterpret_cast<const tflite::uint8*>(model_input.data()),
      {3 * input_width, 3}};
  auto frame_buffer = tflite::task::vision::FrameBuffer::Create(
      {plane}, {input_width, input_height},
      tflite::task::vision::FrameBuffer::Format::kRGB,
      tflite::task::vision::FrameBuffer::Orientation::kTopLeft);
  auto statusor_result = classifier->Classify(*frame_buffer);
  if (!statusor_result.ok()) {
    VLOG(1) << statusor_result.status().ToString();
    return std::vector<double>();
  } else {
    std::vector<double> scores(
        statusor_result->classifications(0).classes().size());
    for (const tflite::task::vision::Class& clas :
         statusor_result->classifications(0).classes()) {
      scores[clas.index()] = clas.score();
    }
    return scores;
  }
}

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

}  // namespace safe_browsing
