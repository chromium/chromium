// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/companion/visual_search/visual_search_eligibility.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/notreached.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace companion::visual_search {
namespace {
constexpr char kNormalizedPrefix[] = "normalized_";
constexpr char kNormalizeByPrefix[] = "normalize_by_";
constexpr int kMaxNumStored = 200;

// Return true if p1 should be sorted before p2.
bool SortDesc(const std::pair<std::string, double>& p1,
              const std::pair<std::string, double>& p2) {
  return p1.second > p2.second;
}
}  // namespace

EligibilityModule::EligibilityModule(const EligibilitySpec& spec)
    : spec_(spec), have_run_first_pass_(false) {}

EligibilityModule::~EligibilityModule() = default;

std::vector<std::string>
EligibilityModule::RunFirstPassEligibilityAndCacheFeatureValues(
    const SizeF& viewport_image_size,
    const std::vector<SingleImageGeometryFeatures>& images) {
  Clear();
  have_run_first_pass_ = true;
  viewport_width_ = viewport_image_size.width();
  viewport_height_ = viewport_image_size.height();
  ComputeNormalizingFeatures(images);
  std::vector<std::string> eligible_images;
  int count = 0;
  for (const SingleImageGeometryFeatures& image : images) {
    // Ensure that we don't store features for too many images.
    if (count++ > kMaxNumStored) {
      break;
    }

    // First compute the features so that then we can evaluate the rules based
    // on cached feature values.
    ComputeFeaturesForOrOfThresholdingRules(spec_.cheap_pruning_rules(), image);
    if (!IsEligible(spec_.cheap_pruning_rules(), image.image_identifier)) {
      continue;
    }
    eligible_after_first_pass_.insert(image.image_identifier);
    eligible_images.push_back(image.image_identifier);
    ComputeFeaturesForOrOfThresholdingRules(spec_.classifier_score_rules(),
                                            image);
    ComputeFeaturesForOrOfThresholdingRules(spec_.post_renormalization_rules(),
                                            image);
  }

  return eligible_images;
}

std::vector<std::string>
EligibilityModule::RunSecondPassPostClassificationEligibility(
    const base::flat_map<std::string, double>& shopping_classifier_scores,
    const base::flat_map<std::string, double>& sensitivity_classifier_scores) {
  CHECK(have_run_first_pass_);
  have_run_first_pass_ = false;
  // Cache the scores so that they can be looked up when computing the rules.
  for (const auto& each_pair : shopping_classifier_scores) {
    if (image_level_features_[each_pair.first].size() < kMaxNumStored) {
      image_level_features_[each_pair.first]
                           [FeatureLibrary::SHOPPING_CLASSIFIER_SCORE] =
                               each_pair.second;
    }
  }
  for (const auto& each_pair : sensitivity_classifier_scores) {
    if (image_level_features_[each_pair.first].size() < kMaxNumStored) {
      image_level_features_[each_pair.first]
                           [FeatureLibrary::SENS_CLASSIFIER_SCORE] =
                               each_pair.second;
    }
  }

  for (const std::string& image_id : eligible_after_first_pass_) {
    if (IsEligible(spec_.classifier_score_rules(), image_id)) {
      eligible_after_second_pass_.insert(image_id);
    }
  }
  RenormalizeForThirdPass();
  std::vector<std::pair<std::string, double>> eligible_image_ids_with_scores;
  for (const std::string& image_id : eligible_after_second_pass_) {
    if (IsEligible(spec_.post_renormalization_rules(), image_id)) {
      eligible_image_ids_with_scores.push_back(
          std::make_pair(image_id, shopping_classifier_scores.at(image_id)));
    }
  }
  std::sort(eligible_image_ids_with_scores.begin(),
            eligible_image_ids_with_scores.end(), SortDesc);

  std::vector<std::string> eligible_image_ids;
  eligible_image_ids.reserve(eligible_image_ids_with_scores.size());
  for (auto& id_score_pair : eligible_image_ids_with_scores) {
    eligible_image_ids.push_back(std::move(id_score_pair.first));
  }
  return eligible_image_ids;
}

base::flat_map<std::string, double>
EligibilityModule::GetDebugFeatureValuesForImage(const std::string& image_id) {
  base::flat_map<std::string, double> output_map;
  GetDebugFeatureValuesForRules(image_id, spec_.cheap_pruning_rules(),
                                output_map);
  GetDebugFeatureValuesForRules(image_id, spec_.classifier_score_rules(),
                                output_map);
  GetDebugFeatureValuesForRules(image_id, spec_.post_renormalization_rules(),
                                output_map);
  return output_map;
}

// Private methods.
void EligibilityModule::Clear() {
  image_level_features_.clear();
  max_value_features_.clear();
  eligible_after_first_pass_.clear();
  eligible_after_second_pass_.clear();
  have_run_first_pass_ = false;
}

void EligibilityModule::ComputeNormalizingFeatures(
    const std::vector<SingleImageGeometryFeatures>& images) {
  const bool second_pass_only = false;
  for (const auto& eligibility_rule : spec_.cheap_pruning_rules()) {
    for (const auto& thresholding_rule : eligibility_rule.rules()) {
      if (thresholding_rule.has_normalizing_op()) {
        ComputeAndGetNormalizingFeatureValue(thresholding_rule.feature_name(),
                                             thresholding_rule.normalizing_op(),
                                             images, second_pass_only);
      }
    }
  }

  for (const auto& second_pass_rule : spec_.classifier_score_rules()) {
    for (const auto& thresholding_rule : second_pass_rule.rules()) {
      if (thresholding_rule.has_normalizing_op()) {
        ComputeAndGetNormalizingFeatureValue(thresholding_rule.feature_name(),
                                             thresholding_rule.normalizing_op(),
                                             images, second_pass_only);
      }
    }
  }

  for (const auto& third_pass_rule : spec_.post_renormalization_rules()) {
    for (const auto& thresholding_rule : third_pass_rule.rules()) {
      if (thresholding_rule.has_normalizing_op()) {
        ComputeAndGetNormalizingFeatureValue(thresholding_rule.feature_name(),
                                             thresholding_rule.normalizing_op(),
                                             images, second_pass_only);
      }
    }
  }
}

bool EligibilityModule::IsEligible(
    const google::protobuf::RepeatedPtrField<OrOfThresholdingRules>& rules,
    const std::string& image_id) {
  for (const auto& rule : rules) {
    if (!EvaluateEligibilityRule(rule, image_id)) {
      return false;
    }
  }
  return true;
}

bool EligibilityModule::EvaluateEligibilityRule(
    const OrOfThresholdingRules& eligibility_rule,
    const std::string& image_id) {
  // Compute the OR of the thresholding rules.
  for (const auto& thresholding_rule : eligibility_rule.rules()) {
    if (EvaluateThresholdingRule(thresholding_rule, image_id)) {
      return true;
    }
  }
  return false;
}

bool EligibilityModule::EvaluateThresholdingRule(
    const ThresholdingRule& thresholding_rule,
    const std::string& image_id) {
  double feature_value =
      RetrieveImageFeatureOrDie(thresholding_rule.feature_name(), image_id);
  if (thresholding_rule.has_normalizing_op()) {
    const double normalizing_feature = RetrieveNormalizingFeatureOrDie(
        thresholding_rule.feature_name(), thresholding_rule.normalizing_op());
    if (normalizing_feature != 0) {
      feature_value = feature_value / normalizing_feature;
    } else {
      feature_value = 0;
    }
  }
  if (thresholding_rule.thresholding_op() == FeatureLibrary::GT) {
    return feature_value > thresholding_rule.threshold();
  } else if (thresholding_rule.thresholding_op() == FeatureLibrary::LT) {
    return feature_value < thresholding_rule.threshold();
  } else {
    NOTREACHED();
  }
  return false;
}

void EligibilityModule::ComputeFeaturesForOrOfThresholdingRules(
    const google::protobuf::RepeatedPtrField<OrOfThresholdingRules>& rules,
    const SingleImageGeometryFeatures& image) {
  for (const auto& rule : rules) {
    for (const auto& thresholding_rule : rule.rules()) {
      const auto feature_name = thresholding_rule.feature_name();
      if (feature_name != FeatureLibrary::SHOPPING_CLASSIFIER_SCORE &&
          feature_name != FeatureLibrary::SENS_CLASSIFIER_SCORE) {
        GetImageFeatureValue(thresholding_rule.feature_name(), image);
      }
    }
  }
}

double EligibilityModule::GetMaxFeatureValue(
    FeatureLibrary::ImageLevelFeatureName feature_name,
    const std::vector<SingleImageGeometryFeatures>& images) {
  if (const auto it = max_value_features_.find(feature_name);
      it != max_value_features_.end()) {
    return it->second;
  }
  double max_value = 0.0;
  int count = 0;
  for (const auto& image : images) {
    // Don't let the size of cached features grow too much.
    if (count++ > kMaxNumStored) {
      break;
    }
    const double value = GetImageFeatureValue(feature_name, image);
    if (value > max_value) {
      max_value = value;
    }
  }
  if (max_value_features_.size() < kMaxNumStored) {
    max_value_features_[feature_name] = max_value;
  }
  return max_value;
}

double EligibilityModule::MaxFeatureValueAfterSecondPass(
    FeatureLibrary::ImageLevelFeatureName image_feature_name) {
  double max_value = 0.0;
  for (const std::string& image_id : eligible_after_second_pass_) {
    const double value =
        RetrieveImageFeatureOrDie(image_feature_name, image_id);
    if (value > max_value) {
      max_value = value;
    }
  }
  return max_value;
}

double EligibilityModule::GetImageFeatureValue(
    FeatureLibrary::ImageLevelFeatureName feature_name,
    const SingleImageGeometryFeatures& image) {
  // See if we have cached it.
  absl::optional<double> feature_opt =
      RetrieveImageFeatureIfPresent(feature_name, image.image_identifier);
  if (feature_opt.has_value()) {
    return feature_opt.value();
  }

  // Else we need to compute.
  double feature_value = 0;
  double height = 0;
  double width = 0;
  Rect viewport_rect;
  switch (feature_name) {
    case FeatureLibrary::IMAGE_ONPAGE_AREA:
      // Corresponding methods in Chrome are height() and width().
      feature_value = static_cast<double>(image.onpage_rect.height()) *
                      static_cast<double>(image.onpage_rect.width());
      break;
    case FeatureLibrary::IMAGE_ONPAGE_ASPECT_RATIO:
      // Corresponding methods in Chrome are height() and width().
      height = static_cast<double>(image.onpage_rect.height());
      width = static_cast<double>(image.onpage_rect.width());
      if (height != 0.0 && width != 0.0) {
        feature_value = std::max(height, width) / std::min(height, width);
      }
      break;
    case FeatureLibrary::IMAGE_ORIGINAL_AREA:
      feature_value = image.original_image_size.Area64();
      break;
    case FeatureLibrary::IMAGE_ORIGINAL_ASPECT_RATIO:
      height = static_cast<double>(image.original_image_size.height());
      width = static_cast<double>(image.original_image_size.width());
      if (height != 0.0 && width != 0.0) {
        feature_value = std::max(height, width) / std::min(height, width);
      }
      break;
    case FeatureLibrary::IMAGE_VISIBLE_AREA:
      viewport_rect = Rect(0, 0, static_cast<int>(viewport_width_),
                           static_cast<int>(viewport_height_));
      viewport_rect.Intersect(image.onpage_rect);
      feature_value = static_cast<double>(viewport_rect.height()) *
                      static_cast<double>(viewport_rect.width());
      break;
    case FeatureLibrary::IMAGE_FRACTION_VISIBLE:
      if (GetImageFeatureValue(FeatureLibrary::IMAGE_ONPAGE_AREA, image) == 0) {
        feature_value = 0;
      } else {
        feature_value =
            GetImageFeatureValue(FeatureLibrary::IMAGE_VISIBLE_AREA, image) /
            GetImageFeatureValue(FeatureLibrary::IMAGE_ONPAGE_AREA, image);
      }
      break;
    case FeatureLibrary::IMAGE_ORIGINAL_HEIGHT:
      feature_value = static_cast<double>(image.original_image_size.height());
      break;
    case FeatureLibrary::IMAGE_ORIGINAL_WIDTH:
      feature_value = static_cast<double>(image.original_image_size.width());
      break;
    case FeatureLibrary::IMAGE_ONPAGE_HEIGHT:
      feature_value = static_cast<double>(image.onpage_rect.height());
      break;
    case FeatureLibrary::IMAGE_ONPAGE_WIDTH:
      feature_value = static_cast<double>(image.onpage_rect.width());
      break;
    case FeatureLibrary::IMAGE_LEVEL_UNSPECIFIED:
    case FeatureLibrary::SHOPPING_CLASSIFIER_SCORE:
    case FeatureLibrary::SENS_CLASSIFIER_SCORE:
      NOTREACHED();
      break;
  }
  // Cache it and return.
  if (image_level_features_[image.image_identifier].size() < kMaxNumStored) {
    image_level_features_[image.image_identifier][feature_name] = feature_value;
  }
  return feature_value;
}

absl::optional<double> EligibilityModule::RetrieveImageFeatureIfPresent(
    FeatureLibrary::ImageLevelFeatureName feature_name,
    const std::string& image_id) {
  if (const auto& feature_to_value_it = image_level_features_.find(image_id);
      feature_to_value_it != image_level_features_.end()) {
    if (const auto& value_it = feature_to_value_it->second.find(feature_name);
        value_it != feature_to_value_it->second.end()) {
      return value_it->second;
    }
  }
  return {};
}

double EligibilityModule::RetrieveImageFeatureOrDie(
    FeatureLibrary::ImageLevelFeatureName feature_name,
    const std::string& image_id) {
  absl::optional<double> feature_opt =
      RetrieveImageFeatureIfPresent(feature_name, image_id);
  CHECK(feature_opt.has_value()) << "Did not find image feature.";
  return feature_opt.value();
}

double EligibilityModule::RetrieveNormalizingFeatureOrDie(
    FeatureLibrary::ImageLevelFeatureName feature_name,
    FeatureLibrary::NormalizingOp normalizing_op) {
  if (normalizing_op == FeatureLibrary::BY_VIEWPORT_AREA) {
    return viewport_width_ * viewport_height_;
  }
  if (normalizing_op == FeatureLibrary::BY_MAX_VALUE) {
    if (const auto it = max_value_features_.find(feature_name);
        it != max_value_features_.end()) {
      return it->second;
    }
    CHECK(false) << "Did not find normalizing feature.";
  }
  NOTREACHED();
  return 1;
}

double EligibilityModule::ComputeAndGetNormalizingFeatureValue(
    FeatureLibrary::ImageLevelFeatureName feature_name,
    FeatureLibrary::NormalizingOp normalizing_op,
    const std::vector<SingleImageGeometryFeatures>& images,
    bool limit_to_second_pass_eligible) {
  if (normalizing_op == FeatureLibrary::BY_VIEWPORT_AREA) {
    return viewport_width_ * viewport_height_;
  }
  if (normalizing_op == FeatureLibrary::BY_MAX_VALUE) {
    if (!limit_to_second_pass_eligible) {
      return GetMaxFeatureValue(feature_name, images);
    } else {
      return MaxFeatureValueAfterSecondPass(feature_name);
    }
  }
  NOTREACHED();
  return 1;
}

void EligibilityModule::GetDebugFeatureValuesForRules(
    const std::string& image_id,
    const google::protobuf::RepeatedPtrField<OrOfThresholdingRules>& rules,
    base::flat_map<std::string, double>& output_map) {
  for (const auto& rule : rules) {
    for (const auto& ored_rule : rule.rules()) {
      const FeatureLibrary::ImageLevelFeatureName feature_name =
          ored_rule.feature_name();
      if (feature_name == FeatureLibrary::SHOPPING_CLASSIFIER_SCORE ||
          feature_name == FeatureLibrary::SENS_CLASSIFIER_SCORE) {
        continue;
      }
      const double feature_value =
          RetrieveImageFeatureOrDie(feature_name, image_id);
      output_map[FeatureLibrary::ImageLevelFeatureName_Name(feature_name)] =
          feature_value;
      if (ored_rule.has_normalizing_op()) {
        const auto normalizing_op = ored_rule.normalizing_op();
        const double normalizing_value =
            RetrieveNormalizingFeatureOrDie(feature_name, normalizing_op);
        if (normalizing_op == FeatureLibrary::BY_MAX_VALUE) {
          output_map[kNormalizeByPrefix +
                     FeatureLibrary::ImageLevelFeatureName_Name(feature_name)] =
              normalizing_value;
        } else {
          output_map[kNormalizeByPrefix +
                     FeatureLibrary::NormalizingOp_Name(normalizing_op)] =
              normalizing_value;
        }
        if (normalizing_value != 0) {
          output_map[kNormalizedPrefix +
                     FeatureLibrary::ImageLevelFeatureName_Name(feature_name)] =
              feature_value / normalizing_value;
        }
      }
    }
  }
}

void EligibilityModule::RenormalizeForThirdPass() {
  for (const auto& third_pass_rule : spec_.post_renormalization_rules()) {
    for (const auto& thresholding_rule : third_pass_rule.rules()) {
      if (thresholding_rule.has_normalizing_op() &&
          thresholding_rule.normalizing_op() == FeatureLibrary::BY_MAX_VALUE) {
        const auto feature_name = thresholding_rule.feature_name();
        if (max_value_features_.size() < kMaxNumStored ||
            max_value_features_.contains(feature_name)) {
          max_value_features_[feature_name] =
              ComputeAndGetNormalizingFeatureValue(
                  feature_name, FeatureLibrary::BY_MAX_VALUE, {}, true);
        }
      }
    }
  }
}
}  // namespace companion::visual_search
