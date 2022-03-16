// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/protobuf_scorer.h"

#include <math.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/visual_utils.h"
#include "content/public/renderer/render_thread.h"
#include "crypto/sha2.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace safe_browsing {

namespace {
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

void RecordScorerCreationStatus(ScorerCreationStatus status) {
  UMA_HISTOGRAM_ENUMERATION("SBClientPhishing.ProtobufScorer.CreationStatus",
                            status, SCORER_STATUS_MAX);
}

}  // namespace

ProtobufModelScorer::ProtobufModelScorer() = default;
ProtobufModelScorer::~ProtobufModelScorer() = default;

/* static */
ProtobufModelScorer* ProtobufModelScorer::Create(
    const base::StringPiece& model_str,
    base::File visual_tflite_model) {
  std::unique_ptr<ProtobufModelScorer> scorer(new ProtobufModelScorer());
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
  if (visual_tflite_model.IsValid()) {
    scorer->visual_tflite_model_ = std::make_unique<base::MemoryMappedFile>();
    if (!scorer->visual_tflite_model_->Initialize(
            std::move(visual_tflite_model))) {
      RecordScorerCreationStatus(SCORER_FAIL_MAP_VISUAL_TFLITE_MODEL);
      return nullptr;
    }
  }

  RecordScorerCreationStatus(SCORER_SUCCESS);
  return scorer.release();
}

double ProtobufModelScorer::ComputeScore(const FeatureMap& features) const {
  double logodds = 0.0;
  for (int i = 0; i < model_.rule_size(); ++i) {
    logodds += ComputeRuleScore(model_.rule(i), features);
  }
  return LogOdds2Prob(logodds);
}

void ProtobufModelScorer::GetMatchingVisualTargets(
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

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB) && !BUILDFLAG(IS_CHROMEOS) && \
    !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
void ProtobufModelScorer::ApplyVisualTfLiteModel(
    const SkBitmap& bitmap,
    base::OnceCallback<void(std::vector<double>)> callback) {
  DCHECK(content::RenderThread::IsMainThread());
  if (visual_tflite_model_ && visual_tflite_model_->IsValid()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT, base::WithBaseSyncPrimitives()},
        base::BindOnce(&ApplyVisualTfLiteModelHelper, bitmap,
                       model_.tflite_metadata().input_width(),
                       model_.tflite_metadata().input_height(),
                       std::move(visual_tflite_model_)),
        base::BindOnce(&ProtobufModelScorer::OnVisualTfLiteModelComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    std::move(callback).Run(std::vector<double>());
  }
}
#endif

int ProtobufModelScorer::model_version() const {
  return model_.version();
}

bool Scorer::HasVisualTfLiteModel() const {
  return visual_tflite_model_ && visual_tflite_model_->IsValid();
}

const std::unordered_set<std::string>&
ProtobufModelScorer::get_page_terms_for_test() const {
  return page_terms_;
}

const std::unordered_set<uint32_t>&
ProtobufModelScorer::get_page_words_for_test() const {
  return page_words_;
}

bool ProtobufModelScorer::has_page_term(const std::string& str) const {
  return page_terms_.find(str) != page_terms_.end();
}

base::RepeatingCallback<bool(const std::string&)>
ProtobufModelScorer::find_page_term_callback() const {
  return base::BindRepeating(&ProtobufModelScorer::has_page_term,
                             base::Unretained(this));
}

bool ProtobufModelScorer::has_page_word(uint32_t page_word_hash) const {
  return page_words_.find(page_word_hash) != page_words_.end();
}

base::RepeatingCallback<bool(uint32_t)>
ProtobufModelScorer::find_page_word_callback() const {
  return base::BindRepeating(&ProtobufModelScorer::has_page_word,
                             base::Unretained(this));
}

size_t ProtobufModelScorer::max_words_per_term() const {
  return model_.max_words_per_term();
}

uint32_t ProtobufModelScorer::murmurhash3_seed() const {
  return model_.murmur_hash_seed();
}

size_t ProtobufModelScorer::max_shingles_per_page() const {
  return model_.max_shingles_per_page();
}

size_t ProtobufModelScorer::shingle_size() const {
  return model_.shingle_size();
}

float ProtobufModelScorer::threshold_probability() const {
  return model_.threshold_probability();
}

int ProtobufModelScorer::tflite_model_version() const {
  return model_.tflite_metadata().model_version();
}

const google::protobuf::RepeatedPtrField<TfLiteModelMetadata::Threshold>&
ProtobufModelScorer::tflite_thresholds() const {
  return model_.tflite_metadata().thresholds();
}

double ProtobufModelScorer::ComputeRuleScore(const ClientSideModel::Rule& rule,
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
