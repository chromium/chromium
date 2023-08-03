// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFICATION_AND_ELIGIBILITY_H_
#define CHROME_RENDERER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFICATION_AND_ELIGIBILITY_H_

#include "chrome/renderer/companion/visual_search/visual_search_eligibility.h"
#include "components/optimization_guide/proto/visual_search_model_metadata.pb.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/vision/image_classifier.h"
#include "ui/gfx/geometry/size_f.h"

namespace companion::visual_search {

using optimization_guide::proto::EligibilitySpec;
using optimization_guide::proto::FeatureLibrary;
using optimization_guide::proto::OrOfThresholdingRules;
using optimization_guide::proto::ThresholdingRule;

using ImageId = std::string;

struct SingleImageFeaturesAndBytes {
  SingleImageGeometryFeatures features;
  SkBitmap image_contents;
  ~SingleImageFeaturesAndBytes() = default;
};

// Structure for classification metrics about the page being processed.
struct ClassificationMetrics {
  // Count for number of images that meet the eligibility specs for
  // classification.
  uint32_t eligible_count;

  // Count for number of eligible images that pass the sensitivity threshold.
  uint32_t sensitive_count;

  // Count for number of eligible images that pass the shoppy threshold.
  uint32_t shoppy_count;

  // Count for number of eligible images that pass the shoppy threshold but
  // does not trigger the sensitivity threshold.
  uint32_t shoppy_nonsensitive_count;

  // Count for number of images that pass all of our eligibility and
  // classification thresholds.
  uint32_t result_count;

  ~ClassificationMetrics() = default;
};

class VisualClassificationAndEligibility {
 public:
  // Extract the SingleImageGeometryFeatures needed by the eligibility
  // module.
  // TODO: move this function outside of this class.
  static SingleImageGeometryFeatures ExtractFeaturesForEligibility(
      const ImageId& image_identifier,
      blink::WebElement& element);

  // Create a VisualClassificationAndEligibility Object that can then be
  // used to run classification and eligibility. Returns a nullptr if
  // there was any error.
  static std::unique_ptr<VisualClassificationAndEligibility> Create(
      const std::string& model_bytes,
      const EligibilitySpec& eligibility_spec);

  // Run through classification and eligibility.
  std::vector<ImageId> RunClassificationAndEligibility(
      base::flat_map<ImageId, SingleImageFeaturesAndBytes>& images,
      const gfx::SizeF& viewport_size);

  const ClassificationMetrics& classification_metrics() { return metrics_; }

  VisualClassificationAndEligibility(
      const VisualClassificationAndEligibility&) = delete;
  VisualClassificationAndEligibility& operator=(
      const VisualClassificationAndEligibility&) = delete;
  ~VisualClassificationAndEligibility();

 private:
  VisualClassificationAndEligibility();
  std::pair<double, double> ClassifyImage(const SkBitmap& bitmap);

  std::unique_ptr<tflite::task::vision::ImageClassifier> classifier_;
  std::unique_ptr<EligibilityModule> eligibility_module_;
  ClassificationMetrics metrics_;
};
}  // namespace companion::visual_search
#endif
