// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/companion/visual_query/visual_query_eligibility.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace companion::visual_query {
namespace {
constexpr char kNormalizedPrefix[] = "normalized_";
constexpr char kNormalizeByPrefix[] = "normalize_by_";
constexpr int kMaxNumStored = 200;

// Return true if p1 should be sorted before p2.
bool SortDesc(const std::pair<std::string, double>& p1,
              const std::pair<std::string, double>& p2) {
  return p1.second > p2.second;
}
bool SortAsc(const std::pair<std::string, double>& p1,
             const std::pair<std::string, double>& p2) {
  return p1.second < p2.second;
}
bool SortDescImages(const std::pair<int, int>& p1,
                    const std::pair<int, int>& p2) {
  return p1.second > p2.second;
}

double ComputeDistanceToViewPortCenter(const Rect& onpage_image_rect,
                                       float viewport_width,
                                       float viewport_height) {
  const double viewport_ctr_x = viewport_width / 2;
  const double viewport_ctr_y = viewport_height / 2;

  const double image_ctr_x = onpage_image_rect.x() +
                             static_cast<double>(onpage_image_rect.width()) / 2;
  const double image_ctr_y =
      onpage_image_rect.y() +
      static_cast<double>(onpage_image_rect.height()) / 2;

  const double x_diff = image_ctr_x - viewport_ctr_x;
  const double y_diff = image_ctr_y - viewport_ctr_y;

  return sqrt(x_diff * x_diff + y_diff * y_diff);
}

// Returns the fraction of image2 area that is covered by image1.
double ComputeFractionCover(const Rect& image1_onpage_rect,
                            const Rect& image2_onpage_rect) {
  Rect copy_rect = image1_onpage_rect;
  copy_rect.Intersect(image2_onpage_rect);
  const int intersection_area = copy_rect.height() * copy_rect.width();
  const int image2_area =
      image2_onpage_rect.height() * image2_onpage_rect.width();
  if (image2_area == 0) {
    // Trivially perfect overlap.
    return 1.0;
  }
  return static_cast<double>(intersection_area) / image2_area;
}

}  // namespace

EligibilityModule::EligibilityModule(const EligibilitySpec& spec)
    : spec_(spec),
      have_run_first_pass_(false),
      num_shoppy_images_(0),
      num_sensitive_images_(0),
      most_shoppy_id_(""),
      most_shoppy_shopping_score_(0.0),
      most_shoppy_sens_score_(1.0) {}

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
  }
  base::UmaHistogramCounts100(
      "Companion.VisualQuery.EligibilityStatus.NumImages",
      eligible_after_first_pass_.size());

  RunAdditionalCheapPruning(images);

  // Cache features for eligible images.
  std::vector<std::string> eligible_images;
  for (const SingleImageGeometryFeatures& image : images) {
    if (eligible_after_first_pass_.contains(image.image_identifier)) {
      ComputeFeaturesForOrOfThresholdingRules(spec_.classifier_score_rules(),
                                              image);
      ComputeFeaturesForOrOfThresholdingRules(
          spec_.post_renormalization_rules(), image);
      ComputeFeaturesForSortingClauses(image);
      eligible_images.push_back(image.image_identifier);
    }
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

      // Scale up the decimal scores by a factor of 100 for the sake of integer
      // histogram values.
      base::UmaHistogramCounts100(
          "Companion.VisualQuery.MaybeShoppy.ShoppingClassificationScore",
          100 * each_pair.second);
    }
  }
  for (const auto& each_pair : sensitivity_classifier_scores) {
    if (image_level_features_[each_pair.first].size() < kMaxNumStored) {
      image_level_features_[each_pair.first]
                           [FeatureLibrary::SENS_CLASSIFIER_SCORE] =
                               each_pair.second;

      base::UmaHistogramCounts100(
          "Companion.VisualQuery.MaybeSensitive.SensitivityClassificationScore",
          100 * each_pair.second);
    }
  }

  for (const std::string& image_id : eligible_after_first_pass_) {
    if (IsEligible(spec_.classifier_score_rules(), image_id)) {
      eligible_after_second_pass_.insert(image_id);
    }
  }
  RenormalizeForThirdPass();
  std::vector<std::pair<std::string, double>> images_with_feature_values;
  for (const std::string& image_id : eligible_after_second_pass_) {
    if (IsEligible(spec_.post_renormalization_rules(), image_id)) {
      images_with_feature_values.emplace_back(image_id, 0.0);
    }
  }

  SortImages(&images_with_feature_values);

  std::vector<std::string> eligible_image_ids;
  eligible_image_ids.reserve(images_with_feature_values.size());
  for (auto& id_score_pair : images_with_feature_values) {
    eligible_image_ids.push_back(std::move(id_score_pair.first));
  }

  if (eligible_image_ids.size() > 0) {
    // Scale up the decimal scores by a factor of 100 for the sake of integer
    // histogram values.
    int winning_image_shopping_score =
        100 * shopping_classifier_scores.find(eligible_image_ids[0])->second;
    int winning_image_sens_score =
        100 * sensitivity_classifier_scores.find(eligible_image_ids[0])->second;

    base::UmaHistogramCounts100(
        "Companion.VisualQuery.MostShoppyNotSensitive."
        "ShoppingClassificationScore",
        winning_image_shopping_score);
    base::UmaHistogramCounts100(
        "Companion.VisualQuery.MostShoppyNotSensitive."
        "SensitivityClassificationScore",
        winning_image_sens_score);
    base::UmaHistogramCounts100(
        "Companion.VisualQuery.MostShoppy.ShoppingClassificationScore",
        100 * most_shoppy_shopping_score_);
    base::UmaHistogramCounts100(
        "Companion.VisualQuery.MostShoppy.SensitivityClassificationScore",
        100 * most_shoppy_sens_score_);
  }

  // Image counts for funnel metrics
  base::UmaHistogramCounts100(
      "Companion.VisualQuery.EligibilityStatus.NumShoppy", num_shoppy_images_);
  base::UmaHistogramCounts100(
      "Companion.VisualQuery.EligibilityStatus.NumSensitive",
      num_sensitive_images_);
  base::UmaHistogramCounts100(
      "Companion.VisualQuery.EligibilityStatus.NumShoppyNotSensitive",
      eligible_after_second_pass_.size());

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
  num_shoppy_images_ = 0;
  num_sensitive_images_ = 0;
  most_shoppy_id_ = "";
  most_shoppy_shopping_score_ = 0.0;
  most_shoppy_sens_score_ = 1.0;
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
      if (thresholding_rule.feature_name() ==
          FeatureLibrary::SHOPPING_CLASSIFIER_SCORE) {
        num_shoppy_images_ += 1;
      }
      return true;
    } else if (thresholding_rule.feature_name() ==
               FeatureLibrary::SENS_CLASSIFIER_SCORE) {
      num_sensitive_images_ += 1;
    }
  }
  return false;
}

bool EligibilityModule::IsImageShoppyForMetrics(const std::string& image_id) {
  for (const auto& classifier_rules : spec_.classifier_score_rules()) {
    for (const auto& thresholding_rule : classifier_rules.rules()) {
      if (thresholding_rule.feature_name() ==
          FeatureLibrary::SHOPPING_CLASSIFIER_SCORE) {
        return EvaluateThresholdingRule(thresholding_rule, image_id);
      }
    }
  }
  return false;
}

bool EligibilityModule::IsImageSensitiveForMetrics(
    const std::string& image_id) {
  for (const auto& classifier_rules : spec_.classifier_score_rules()) {
    for (const auto& thresholding_rule : classifier_rules.rules()) {
      if (thresholding_rule.feature_name() ==
          FeatureLibrary::SENS_CLASSIFIER_SCORE) {
        return !EvaluateThresholdingRule(thresholding_rule, image_id);
      }
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
    // Update the most shoppy image id + shopping score seen so far if the
    // current image is shoppier
    if (thresholding_rule.feature_name() ==
            FeatureLibrary::SHOPPING_CLASSIFIER_SCORE &&
        feature_value > most_shoppy_shopping_score_) {
      most_shoppy_shopping_score_ = feature_value;
      most_shoppy_id_ = image_id;
    }
    return feature_value > thresholding_rule.threshold();
  } else if (thresholding_rule.thresholding_op() == FeatureLibrary::LT) {
    // Update the most shoppy image sensitivity score if the current image is
    // the shoppiest so far.
    if (thresholding_rule.feature_name() ==
            FeatureLibrary::SENS_CLASSIFIER_SCORE &&
        image_id.compare(most_shoppy_id_)) {
      most_shoppy_sens_score_ = feature_value;
    }
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
        GetImageFeatureValue(feature_name, image);
      }
    }
  }
}

void EligibilityModule::ComputeFeaturesForSortingClauses(
    const SingleImageGeometryFeatures& image) {
  for (const auto& sorting_clause : spec_.sorting_clauses()) {
    const auto feature_name = sorting_clause.feature_name();
    if (feature_name != FeatureLibrary::SHOPPING_CLASSIFIER_SCORE &&
        feature_name != FeatureLibrary::SENS_CLASSIFIER_SCORE) {
      GetImageFeatureValue(feature_name, image);
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
    case FeatureLibrary::IMAGE_DISTANCE_TO_VIEWPORT_CENTER:
      feature_value = ComputeDistanceToViewPortCenter(
          image.onpage_rect, viewport_width_, viewport_height_);
      break;
    case FeatureLibrary::IMAGE_LEVEL_UNSPECIFIED:
    case FeatureLibrary::SHOPPING_CLASSIFIER_SCORE:
    case FeatureLibrary::SENS_CLASSIFIER_SCORE:
    // TODO(b/314789511): Implement these after setting server-side
    case FeatureLibrary::NAT_WORLD_CLASSIFIER_SCORE:
    case FeatureLibrary::PUB_FIGURES_CLASSIFIER_SCORE:
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

void EligibilityModule::SortImages(
    std::vector<std::pair<std::string, double>>* images_with_feature_values) {
  for (const auto& sorting_clause : spec_.sorting_clauses()) {
    // For each sorting clause, populate with the feature name that it sorts by
    // and then sort.
    for (auto& pair : *images_with_feature_values) {
      pair.second =
          RetrieveImageFeatureOrDie(sorting_clause.feature_name(), pair.first);
    }
    if (sorting_clause.sorting_order() == FeatureLibrary::SORT_ASCENDING) {
      std::stable_sort(images_with_feature_values->begin(),
                       images_with_feature_values->end(), SortAsc);
    } else if (sorting_clause.sorting_order() ==
               FeatureLibrary::SORT_DESCENDING) {
      std::stable_sort(images_with_feature_values->begin(),
                       images_with_feature_values->end(), SortDesc);
    } else {
      NOTREACHED();
    }
  }
}

void EligibilityModule::RunAdditionalCheapPruning(
    const std::vector<SingleImageGeometryFeatures>& images) {
  if (!spec_.additional_cheap_pruning_options()
           .has_z_index_overlap_fraction()) {
    return;
  }
  const double cover_threshold =
      spec_.additional_cheap_pruning_options().z_index_overlap_fraction();
  if (cover_threshold <= 0) {
    return;
  }

  // Put the images that are eligible so far in a vector of int pairs where the
  // first element of the pair is the index of the image in the images vector
  // and the second element of the pair is the z-index and sort by z-index
  // desc.
  std::vector<std::pair<int, int>> image_ptrs_vector;
  // Count how many different z indices there are (need at least two for
  // meaningful comparison).
  base::flat_set<int> different_zs;
  for (size_t image_idx = 0; image_idx < images.size(); ++image_idx) {
    const SingleImageGeometryFeatures& image = images.at(image_idx);
    if (!image.z_index ||
        !eligible_after_first_pass_.contains(image.image_identifier)) {
      continue;
    }
    const int z_index = *(image.z_index);
    image_ptrs_vector.emplace_back(image_idx, z_index);
    different_zs.insert(z_index);
  }
  if (different_zs.size() < 2) {
    return;
  }

  // Starting from the image with the largest z index, check if we can
  // eliminate any images that are (almost) fully covered by it.
  std::stable_sort(image_ptrs_vector.begin(), image_ptrs_vector.end(),
                   SortDescImages);
  const size_t num_images = image_ptrs_vector.size();
  std::vector<bool> filter_images_by_z_idx(num_images);
  for (size_t i = 0; i < num_images - 1; ++i) {
    if (filter_images_by_z_idx[i]) {
      continue;
    }
    for (size_t j = i + 1; j < num_images; ++j) {
      if (filter_images_by_z_idx[j]) {
        continue;
      }
      //  If the z-index values are the same, we are not going to filter even if
      //  the images are overlapping. The j-th z-value must be strictly smaller
      //  for us to filter.
      if (image_ptrs_vector[i].second == image_ptrs_vector[j].second) {
        continue;
      }
      const double fraction_cover = ComputeFractionCover(
          images.at(image_ptrs_vector.at(i).first).onpage_rect,
          images.at(image_ptrs_vector.at(j).first).onpage_rect);
      if (fraction_cover >= cover_threshold) {
        filter_images_by_z_idx[j] = true;
      }
    }
  }
  for (size_t i = 0; i < filter_images_by_z_idx.size(); ++i) {
    if (filter_images_by_z_idx.at(i)) {
      eligible_after_first_pass_.erase(
          images.at(image_ptrs_vector.at(i).first).image_identifier);
    }
  }
}
}  // namespace companion::visual_query
