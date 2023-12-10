// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_COMPANION_VISUAL_QUERY_VISUAL_QUERY_ELIGIBILITY_H_
#define CHROME_RENDERER_COMPANION_VISUAL_QUERY_VISUAL_QUERY_ELIGIBILITY_H_

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "components/optimization_guide/proto/visual_search_model_metadata.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

namespace companion::visual_query {

using ::gfx::Rect;
using ::gfx::Size;
using ::gfx::SizeF;

using optimization_guide::proto::EligibilitySpec;
using optimization_guide::proto::FeatureLibrary;
using optimization_guide::proto::OrOfThresholdingRules;
using optimization_guide::proto::ThresholdingRule;

// Stores the raw features of a single image.
struct SingleImageGeometryFeatures {
  std::string image_identifier;
  Size original_image_size;
  Rect onpage_rect = Rect(0, 0, 0, 0);
  // Used for filtering of overlapping images using the z index.
  absl::optional<int> z_index;
  ~SingleImageGeometryFeatures() = default;
};

// Stores image features, bytes, and alt-text.
struct SingleImageFeaturesAndBytes {
  SingleImageGeometryFeatures features;
  SkBitmap image_contents;
  std::string alt_text;
  ~SingleImageFeaturesAndBytes() = default;
};

// This class is used to determine which images are eligible to be surfaced in
// the CSC side bar according to settings set in the config proto.
class EligibilityModule {
 public:
  // Create the module using a spec.
  explicit EligibilityModule(const EligibilitySpec& spec);

  // Applies the cheap_pruning_rules from the eligibility spec. Outputs a list
  // of image identifiers that pass eligibility in no particular order. Caches
  // the values of all features that are needed across all rule sets in the
  // spec to avoid having to pass them throughout.
  std::vector<std::string> RunFirstPassEligibilityAndCacheFeatureValues(
      const SizeF& viewport_image_size,
      const std::vector<SingleImageGeometryFeatures>& images);

  // Applies the classifier_score_rules and post_renormalization_rules from the
  // eligibility spec and outputs the list of image identifiers that pass,
  // sorted by descending shoppy score. Should be run after
  // RunFirstPassEligibility above is run and only if the image geometry
  // features have not changed since that method was called.
  std::vector<std::string> RunSecondPassPostClassificationEligibility(
      const base::flat_map<std::string, double>& shopping_classifier_scores,
      const base::flat_map<std::string, double>& sensitivity_classifier_scores);

  // Returns a map from formatted-as-string feature name to feature value for
  // the given image_identifier.
  base::flat_map<std::string, double> GetDebugFeatureValuesForImage(
      const std::string& image_id);

  // These methods are mainly used to calculate metrics for logging.
  bool IsImageShoppyForMetrics(const std::string& image_id);
  bool IsImageSensitiveForMetrics(const std::string& image_id);

  EligibilityModule(const EligibilityModule&) = delete;
  EligibilityModule& operator=(const EligibilityModule&) = delete;
  ~EligibilityModule();

 private:
  FRIEND_TEST_ALL_PREFIXES(EligibilityModuleTest, TestImageFeatureComputation);
  FRIEND_TEST_ALL_PREFIXES(EligibilityModuleTest, TestPageFeatureComputation);
  void Clear();
  void ComputeNormalizingFeatures(
      const std::vector<SingleImageGeometryFeatures>& images);
  void RenormalizeForThirdPass();
  void ComputeFeaturesForOrOfThresholdingRules(
      const google::protobuf::RepeatedPtrField<OrOfThresholdingRules>& rules,
      const SingleImageGeometryFeatures& image);
  void ComputeFeaturesForSortingClauses(
      const SingleImageGeometryFeatures& image);
  // Eligibility evaluation methods.
  bool IsEligible(
      const google::protobuf::RepeatedPtrField<OrOfThresholdingRules>& rules,
      const std::string& image_id);
  bool EvaluateEligibilityRule(const OrOfThresholdingRules& eligibility_rule,
                               const std::string& image_id);
  bool EvaluateThresholdingRule(const ThresholdingRule& thresholding_rule,
                                const std::string& image_id);
  // Convenient methods for getting and caching feature values.
  double GetImageFeatureValue(
      FeatureLibrary::ImageLevelFeatureName feature_name,
      const SingleImageGeometryFeatures& image);
  absl::optional<double> RetrieveImageFeatureIfPresent(
      FeatureLibrary::ImageLevelFeatureName feature_name,
      const std::string& image_id);
  double RetrieveImageFeatureOrDie(
      FeatureLibrary::ImageLevelFeatureName feature_name,
      const std::string& image_id);
  double RetrieveNormalizingFeatureOrDie(
      FeatureLibrary::ImageLevelFeatureName feature_name,
      FeatureLibrary::NormalizingOp normalizing_op);
  double ComputeAndGetNormalizingFeatureValue(
      FeatureLibrary::ImageLevelFeatureName feature_name,
      FeatureLibrary::NormalizingOp normalizing_op,
      const std::vector<SingleImageGeometryFeatures>& images,
      bool limit_to_second_pass_eligible);
  double GetMaxFeatureValue(
      FeatureLibrary::ImageLevelFeatureName feature_name,
      const std::vector<SingleImageGeometryFeatures>& images);
  double MaxFeatureValueAfterSecondPass(
      FeatureLibrary::ImageLevelFeatureName image_feature_name);
  void GetDebugFeatureValuesForRules(
      const std::string& image_id,
      const google::protobuf::RepeatedPtrField<OrOfThresholdingRules>& rules,
      base::flat_map<std::string, double>& output_map);
  // Apply the sorting clauses of the spec before returning the results.
  void SortImages(
      std::vector<std::pair<std::string, double>>* images_with_feature_values);
  void RunAdditionalCheapPruning(
      const std::vector<SingleImageGeometryFeatures>& images);

  EligibilitySpec spec_;
  // Cache for features that are computed individually for each image.
  // TODO(lilymihal): Add metrics about the size of these flat_map and sets.
  base::flat_map<std::string,
                 base::flat_map<FeatureLibrary::ImageLevelFeatureName, double>>
      image_level_features_;
  // Cache for the max value of image-level features, with the max taken over
  // all the images on the page.
  base::flat_map<FeatureLibrary::ImageLevelFeatureName, double>
      max_value_features_;
  // Keep track of what images were eligible after the first and second passes.
  base::flat_set<std::string> eligible_after_first_pass_;
  base::flat_set<std::string> eligible_after_second_pass_;

  // Cache the viewport size so we don't have to pass it around. This gets set
  // in RunFirstPassEligibilityAndCacheFeatureValues.
  float viewport_width_;
  float viewport_height_;

  // Keeps track of whether the first pass has run since the last time we ran
  // the second pass.
  bool have_run_first_pass_;

  // Counters incremented during eligibility rule evaluation if the image meets
  // the appropriate thresholding rule for shoppy or sensitivity classification.
  int num_shoppy_images_;
  int num_sensitive_images_;

  // The id and classification scores of the eligible image with the highest
  // shopping score. The associated sensitivity score may or may not be below
  // the threshold value.
  std::string most_shoppy_id_;
  float most_shoppy_shopping_score_;
  float most_shoppy_sens_score_;
};
}  // namespace companion::visual_query
#endif  // CHROME_RENDERER_COMPANION_VISUAL_QUERY_VISUAL_QUERY_ELIGIBILITY_H_
