// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"

#include <math.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "base/metrics/histogram_macros.h"
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

// Enum used to keep stats about the status of the Scorer creation.
enum ScorerCreationStatus {
  SCORER_SUCCESS,
  SCORER_FAIL_MODEL_OPEN_FAIL,       // Not used anymore
  SCORER_FAIL_MODEL_FILE_EMPTY,      // Not used anymore
  SCORER_FAIL_MODEL_FILE_TOO_LARGE,  // Not used anymore
  SCORER_FAIL_MODEL_PARSE_ERROR,
  SCORER_FAIL_MODEL_MISSING_FIELDS,
  SCORER_FAIL_MAP_VISUAL_TFLITE_MODEL,
  SCORER_STATUS_MAX  // Always add new values before this one.
};

void RecordScorerCreationStatus(ScorerCreationStatus status) {
  UMA_HISTOGRAM_ENUMERATION("SBClientPhishing.ScorerCreationStatus", status,
                            SCORER_STATUS_MAX);
}

std::unique_ptr<ClientPhishingRequest> GetMatchingVisualTargetsHelper(
    const SkBitmap& bitmap,
    const ClientSideModel& model,
    std::unique_ptr<ClientPhishingRequest> request) {
  DCHECK(!content::RenderThread::IsMainThread());

  TRACE_EVENT0("safe_browsing", "GetMatchingVisualTargets");

  VisualFeatures::BlurredImage blurred_image;
  // Obtaining a blurred image is essential for both adding a vision match or
  // populating telemetry.
  if (!visual_utils::GetBlurredImage(bitmap, &blurred_image)) {
    return request;
  }
  const std::string blurred_image_hash =
      visual_utils::GetHashFromBlurredImage(blurred_image);

  VisualFeatures::ColorHistogram histogram;
  if (visual_utils::GetHistogramForImage(bitmap, &histogram)) {
    for (const VisualTarget& target : model.vision_model().targets()) {
      absl::optional<VisionMatchResult> result = visual_utils::IsVisualMatch(
          bitmap, blurred_image_hash, histogram, target);
      if (result.has_value()) {
        *request->add_vision_match() = result.value();
      }
    }
  }

  // Populate these fields for telemetry purposes. They will be filtered in
  // the browser process if they are not needed.
  std::string raw_digest = crypto::SHA256HashString(blurred_image.data());
  request->set_screenshot_digest(
      base::HexEncode(raw_digest.data(), raw_digest.size()));
  request->set_screenshot_phash(blurred_image_hash);
  request->set_phash_dimension_size(48);

  return request;
}

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

std::vector<double> ApplyVisualTfLiteModelHelper(
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

}  // namespace

// Helper function which converts log odds to a probability in the range
// [0.0,1.0].
static double LogOdds2Prob(double log_odds) {
  // 709 = floor(1023*ln(2)).  2**1023 is the largest finite double.
  // Small log odds aren't a problem.  as the odds will be 0.  It's only
  // when we get +infinity for the odds, that odds/(odds+1) would be NaN.
  if (log_odds >= 709) {
    return 1.0;
  }
  double odds = exp(log_odds);
  return odds / (odds + 1.0);
}

Scorer::Scorer() {}
Scorer::~Scorer() {}

/* static */
Scorer* Scorer::Create(const base::StringPiece& model_str,
                       base::File visual_tflite_model) {
  std::unique_ptr<Scorer> scorer(new Scorer());
  ClientSideModel& model = scorer->model_;
  // Parse the phishing model.
  if (!model_str.empty() &&
      !model.ParseFromArray(model_str.data(), model_str.size())) {
    RecordScorerCreationStatus(SCORER_FAIL_MODEL_PARSE_ERROR);
    return nullptr;
  }

  if (!model_str.empty() && !model.IsInitialized()) {
    // The model may be missing some required fields.
    RecordScorerCreationStatus(SCORER_FAIL_MODEL_MISSING_FIELDS);
    return nullptr;
  }

  if (!model_str.empty()) {
    for (int i = 0; i < model.page_term_size(); ++i) {
      if (model.page_term(i) < 0 || model.page_term(i) >= model.hashes().size())
        return nullptr;
      scorer->page_terms_.insert(model.hashes(model.page_term(i)));
    }
    for (int i = 0; i < model.page_word_size(); ++i) {
      scorer->page_words_.insert(model.page_word(i));
    }

    for (const ClientSideModel::Rule& rule : model.rule()) {
      for (int feature_index : rule.feature()) {
        if (feature_index < 0 || feature_index >= model.hashes().size()) {
          return nullptr;
        }
      }
    }
  }

  // Only do this part if the visual model file exists
  if (visual_tflite_model.IsValid() && !scorer->visual_tflite_model_.Initialize(
                                           std::move(visual_tflite_model))) {
    RecordScorerCreationStatus(SCORER_FAIL_MAP_VISUAL_TFLITE_MODEL);
    return nullptr;
  }

  RecordScorerCreationStatus(SCORER_SUCCESS);
  return scorer.release();
}

double Scorer::ComputeScore(const FeatureMap& features) const {
  double logodds = 0.0;
  for (int i = 0; i < model_.rule_size(); ++i) {
    logodds += ComputeRuleScore(model_.rule(i), features);
  }
  return LogOdds2Prob(logodds);
}

void Scorer::GetMatchingVisualTargets(
    const SkBitmap& bitmap,
    std::unique_ptr<ClientPhishingRequest> request,
    base::OnceCallback<void(std::unique_ptr<ClientPhishingRequest>)> callback)
    const {
  DCHECK(content::RenderThread::IsMainThread());

  // Perform scoring off the main thread to avoid blocking.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::WithBaseSyncPrimitives()},
      base::BindOnce(&GetMatchingVisualTargetsHelper, bitmap, model_,
                     std::move(request)),
      std::move(callback));
}

void Scorer::ApplyVisualTfLiteModel(
    const SkBitmap& bitmap,
    base::OnceCallback<void(std::vector<double>)> callback) const {
  DCHECK(content::RenderThread::IsMainThread());
  if (visual_tflite_model_.IsValid()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&ApplyVisualTfLiteModelHelper, bitmap,
                       model_.tflite_metadata().input_width(),
                       model_.tflite_metadata().input_height(),
                       std::string(reinterpret_cast<const char*>(
                                       visual_tflite_model_.data()),
                                   visual_tflite_model_.length())),
        std::move(callback));
  } else {
    std::move(callback).Run(std::vector<double>());
  }
}

int Scorer::model_version() const {
  return model_.version();
}

bool Scorer::HasVisualTfLiteModel() const {
  return visual_tflite_model_.IsValid();
}

const std::unordered_set<std::string>& Scorer::page_terms() const {
  return page_terms_;
}

const std::unordered_set<uint32_t>& Scorer::page_words() const {
  return page_words_;
}

size_t Scorer::max_words_per_term() const {
  return model_.max_words_per_term();
}

uint32_t Scorer::murmurhash3_seed() const {
  return model_.murmur_hash_seed();
}

size_t Scorer::max_shingles_per_page() const {
  return model_.max_shingles_per_page();
}

size_t Scorer::shingle_size() const {
  return model_.shingle_size();
}

float Scorer::threshold_probability() const {
  return model_.threshold_probability();
}

int Scorer::tflite_model_version() const {
  return model_.tflite_metadata().model_version();
}

const google::protobuf::RepeatedPtrField<TfLiteModelMetadata::Threshold>&
Scorer::tflite_thresholds() const {
  return model_.tflite_metadata().thresholds();
}

double Scorer::ComputeRuleScore(const ClientSideModel::Rule& rule,
                                const FeatureMap& features) const {
  const std::unordered_map<std::string, double>& feature_map =
      features.features();
  double rule_score = 1.0;
  for (int i = 0; i < rule.feature_size(); ++i) {
    const auto it = feature_map.find(model_.hashes(rule.feature(i)));
    if (it == feature_map.end() || it->second == 0.0) {
      // If the feature of the rule does not exist in the given feature map the
      // feature weight is considered to be zero.  If the feature weight is zero
      // we leave early since we know that the rule score will be zero.
      return 0.0;
    }
    rule_score *= it->second;
  }
  return rule_score * rule.weight();
}
}  // namespace safe_browsing
