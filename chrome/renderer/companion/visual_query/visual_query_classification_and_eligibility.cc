// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/companion/visual_query/visual_query_classification_and_eligibility.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/builtin_op_kernels.h"
#include "third_party/tflite/src/tensorflow/lite/op_resolver.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/vision/image_classifier.h"

namespace companion::visual_query {

namespace {
using ::tflite::task::vision::ImageClassifier;
constexpr char kPosition[] = "position";
constexpr char kStatic[] = "static";
constexpr char kZIndex[] = "z-index";

// TODO(b/284645622): This info should be contained in the image metadata. See
// if we can get it out.
constexpr int kImageSize = 224;
constexpr char kNegativeShoppingLabel[] = "shopping_intent:negative";
constexpr char kNegativeSensitivityLabel[] = "sensitive:negative";

bool OrOfThresholdingRuleLooksGood(const OrOfThresholdingRules& rules) {
  for (const auto& rule : rules.rules()) {
    if (!rule.has_feature_name() ||
        rule.feature_name() == FeatureLibrary::IMAGE_LEVEL_UNSPECIFIED) {
      return false;
    }
    if (rule.has_normalizing_op() &&
        rule.normalizing_op() == FeatureLibrary::NORMALIZE_UNSPECIFIED) {
      return false;
    }
    if (!rule.has_thresholding_op() ||
        rule.thresholding_op() == FeatureLibrary::THRESHOLDING_UNSPECIFIED) {
      return false;
    }
  }
  return true;
}

ClassificationMetrics CalculateClassificatonMetrics(
    EligibilityModule& eligibility_module,
    const std::vector<ImageId>& first_pass_images,
    const std::vector<ImageId>& second_pass_images) {
  ClassificationMetrics metrics;
  uint32_t shoppy_count = 0, sensitive_count = 0, shoppy_nonsensitive_count = 0;
  for (const auto& eligible_image_id : first_pass_images) {
    const bool is_shoppy =
        eligibility_module.IsImageShoppyForMetrics(eligible_image_id);
    const bool is_sensitive =
        eligibility_module.IsImageSensitiveForMetrics(eligible_image_id);

    if (is_shoppy) {
      ++shoppy_count;
    }
    if (is_sensitive) {
      ++sensitive_count;
    }

    if (is_shoppy && !is_sensitive) {
      ++shoppy_nonsensitive_count;
    }
  }
  // Store important metrics needed for logging.
  metrics.eligible_count = first_pass_images.size();
  metrics.shoppy_count = shoppy_count;
  metrics.sensitive_count = sensitive_count;
  metrics.shoppy_nonsensitive_count = shoppy_nonsensitive_count;
  metrics.result_count = second_pass_images.size();
  return metrics;
}

// TODO(b/284645622): this really belongs in the eligibility module code.
bool EligibilitySpecLooksGood(const EligibilitySpec& eligibility_spec) {
  bool all_good = true;
  for (const auto& or_of_thresholding_rule :
       eligibility_spec.cheap_pruning_rules()) {
    if (!OrOfThresholdingRuleLooksGood(or_of_thresholding_rule)) {
      all_good = false;
      VLOG(1) << "Invalid rule among the cheap pruning rules.";
    }
  }
  for (const auto& or_of_thresholding_rule :
       eligibility_spec.classifier_score_rules()) {
    if (!OrOfThresholdingRuleLooksGood(or_of_thresholding_rule)) {
      all_good = false;
      VLOG(1) << "Invalid rule among the classifier score rules.";
    }
  }
  for (const auto& or_of_thresholding_rule :
       eligibility_spec.post_renormalization_rules()) {
    if (!OrOfThresholdingRuleLooksGood(or_of_thresholding_rule)) {
      all_good = false;
      VLOG(1) << "Invalid rule among the post renormalization rules.";
    }
  }
  return all_good;
}

std::unique_ptr<tflite::MutableOpResolver> CreateOpResolver() {
  tflite::MutableOpResolver resolver;
  // The minimal set of OPs required to run the visual model.
  resolver.AddBuiltin(tflite::BuiltinOperator_DEQUANTIZE,
                      tflite::ops::builtin::Register_DEQUANTIZE(),
                      /* min_version = */ 1,
                      /* max_version = */ 4);
  resolver.AddBuiltin(tflite::BuiltinOperator_CONV_2D,
                      tflite::ops::builtin::Register_CONV_2D(),
                      /* min_version = */ 1,
                      /* max_version = */ 5);
  resolver.AddBuiltin(tflite::BuiltinOperator_HARD_SWISH,
                      tflite::ops::builtin::Register_HARD_SWISH(),
                      /* min_version = */ 1,
                      /* max_version = */ 2);
  resolver.AddBuiltin(tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
                      tflite::ops::builtin::Register_DEPTHWISE_CONV_2D(),
                      /* min_version = */ 1,
                      /* max_version = */ 6);
  resolver.AddBuiltin(tflite::BuiltinOperator_MEAN,
                      tflite::ops::builtin::Register_MEAN(),
                      /* min_version = */ 1,
                      /* max_version = */ 2);
  resolver.AddBuiltin(tflite::BuiltinOperator_MUL,
                      tflite::ops::builtin::Register_MUL(),
                      /* min_version = */ 1,
                      /* max_version = */ 2);
  resolver.AddBuiltin(tflite::BuiltinOperator_ADD,
                      tflite::ops::builtin::Register_ADD(),
                      /* min_version = */ 1,
                      /* max_version = */ 2);
  resolver.AddBuiltin(tflite::BuiltinOperator_AVERAGE_POOL_2D,
                      tflite::ops::builtin::Register_AVERAGE_POOL_2D(),
                      /* min_version = */ 1,
                      /* max_version = */ 2);
  resolver.AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED,
                      tflite::ops::builtin::Register_FULLY_CONNECTED(),
                      /* min_version = */ 1,
                      /* max_version = */ 9);
  resolver.AddBuiltin(tflite::BuiltinOperator_SOFTMAX,
                      tflite::ops::builtin::Register_SOFTMAX(),
                      /* min_version = */ 1,
                      /* max_version = */ 3);
  resolver.AddBuiltin(tflite::BuiltinOperator_QUANTIZE,
                      tflite::ops::builtin::Register_QUANTIZE(),
                      /* min_version = */ 1,
                      /* max_version = */ 2);
  return std::make_unique<tflite::MutableOpResolver>(resolver);
}

std::unique_ptr<ImageClassifier> CreateClassifier(std::string model_data) {
  tflite::task::vision::ImageClassifierOptions options;
  tflite::task::core::BaseOptions* base_options =
      options.mutable_base_options();
  base_options->mutable_model_file()->set_file_content(std::move(model_data));
  base_options->mutable_compute_settings()
      ->mutable_tflite_settings()
      ->mutable_cpu_settings()
      ->set_num_threads(1);
  auto statusor_classifier =
      ImageClassifier::CreateFromOptions(options, CreateOpResolver());
  if (!statusor_classifier.ok()) {
    VLOG(1) << statusor_classifier.status().ToString();
    return nullptr;
  }

  return std::move(*statusor_classifier);
}

std::string GetModelInput(const SkBitmap& bitmap, int width, int height) {
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

// Returns the score of the first class in the classification that does not have
// the provided avoid_name.
double GetScoreOfFirstClassThatDoesNotHaveName(
    const std::string& avoid_name,
    const tflite::task::vision::Classifications& classification) {
  // First run a sanity check that the avoid_name is actually in there.
  bool found_avoid_name = false;
  for (const auto& each_class : classification.classes()) {
    if (each_class.class_name() == avoid_name) {
      found_avoid_name = true;
      break;
    }
  }
  CHECK(found_avoid_name) << "None of the classes have avoid_name "
                          << avoid_name;

  // Now return the score of the first class that doesn't have avoid_name.
  for (const auto& each_class : classification.classes()) {
    if (each_class.class_name() != avoid_name) {
      return each_class.score();
    }
  }
  NOTREACHED();  // Because there should be at least 2 predicted classes.
  return -1;
}
}  // namespace

SingleImageGeometryFeatures
VisualClassificationAndEligibility::ExtractFeaturesForEligibility(
    const ImageId& image_identifier,
    blink::WebElement& element) {
  SingleImageGeometryFeatures geometry_features;
  geometry_features.image_identifier = image_identifier;
  geometry_features.original_image_size = element.GetImageSize();
  geometry_features.onpage_rect = element.BoundsInWidget();
  const auto position_value = element.GetComputedValue(kPosition);
  // The z index does not have an effect when the position is static,
  // which is the default value.
  if (!position_value.IsNull() && position_value.Ascii() != kStatic) {
    const auto z_index_value = element.GetComputedValue(kZIndex);
    if (!z_index_value.IsNull()) {
      int z_index;
      if (absl::SimpleAtoi(z_index_value.Ascii(), &z_index)) {
        geometry_features.z_index = z_index;
      } else {
        geometry_features.z_index = 0;
      }
    }
  }
  return geometry_features;
}

std::unique_ptr<VisualClassificationAndEligibility>
VisualClassificationAndEligibility::Create(
    const std::string& model_bytes,
    const EligibilitySpec& eligibility_spec) {
  if (!EligibilitySpecLooksGood(eligibility_spec)) {
    return nullptr;
  }

  std::unique_ptr<VisualClassificationAndEligibility> created =
      base::WrapUnique(new VisualClassificationAndEligibility());
  created->classifier_ = CreateClassifier(model_bytes);
  if (created->classifier_ == nullptr) {
    return nullptr;
  }

  created->eligibility_module_ =
      std::make_unique<EligibilityModule>(eligibility_spec);

  return created;
}

std::vector<ImageId>
VisualClassificationAndEligibility::RunClassificationAndEligibility(
    base::flat_map<ImageId, SingleImageFeaturesAndBytes>& images,
    const gfx::SizeF& viewport_size) {
  std::vector<SingleImageGeometryFeatures> geometry_features;
  geometry_features.reserve(images.size());
  for (const auto& image : images) {
    geometry_features.push_back(image.second.features);
  }
  const std::vector<ImageId> first_pass_eligible =
      eligibility_module_->RunFirstPassEligibilityAndCacheFeatureValues(
          viewport_size, geometry_features);

  base::flat_map<std::string, double> shopping_classifier_scores;
  base::flat_map<std::string, double> sens_classifier_scores;

  for (const ImageId& eligible_image_id : first_pass_eligible) {
    const std::pair<double, double> scores =
        ClassifyImage(images[eligible_image_id].image_contents);
    // Set default values to minimum shoppiness and maximum sensitivity, which
    // is safest in case classification failed.
    double shoppy_score = 0.0;
    double sens_score = 1.0;
    if (scores.first != -1 && scores.second != -1) {
      shoppy_score = scores.first;
      sens_score = scores.second;
    }
    shopping_classifier_scores[eligible_image_id] = shoppy_score;
    sens_classifier_scores[eligible_image_id] = sens_score;
  }

  auto second_pass_eligible =
      eligibility_module_->RunSecondPassPostClassificationEligibility(
          shopping_classifier_scores, sens_classifier_scores);
  metrics_ = CalculateClassificatonMetrics(
      *eligibility_module_, first_pass_eligible, second_pass_eligible);
  return second_pass_eligible;
}

VisualClassificationAndEligibility::~VisualClassificationAndEligibility() =
    default;

std::pair<double, double> VisualClassificationAndEligibility::ClassifyImage(
    const SkBitmap& bitmap) {
  const int input_width = kImageSize;
  const int input_height = kImageSize;
  std::pair<double, double> result = {-1.0, -1.0};
  const std::string model_input =
      GetModelInput(bitmap, input_width, input_height);
  tflite::task::vision::FrameBuffer::Plane plane{
      reinterpret_cast<const tflite::uint8*>(model_input.data()),
      {3 * input_width, 3}};
  auto frame_buffer = tflite::task::vision::FrameBuffer::Create(
      {plane}, {input_width, input_height},
      tflite::task::vision::FrameBuffer::Format::kRGB,
      tflite::task::vision::FrameBuffer::Orientation::kTopLeft);
  auto statusor_result = classifier_->Classify(*frame_buffer);
  if (!statusor_result.ok()) {
    VLOG(1) << "classifier failed " << statusor_result.status().ToString();
    return result;
  }

  for (tflite::task::vision::Classifications classification :
       statusor_result->classifications()) {
    // index = 0 is shopping classification
    if (classification.has_head_index() && classification.head_index() == 0) {
      result.first = GetScoreOfFirstClassThatDoesNotHaveName(
          kNegativeShoppingLabel, classification);
      // index = 1 is sensitivity classification
    } else if (classification.has_head_index() &&
               classification.head_index() == 1) {
      result.second = GetScoreOfFirstClassThatDoesNotHaveName(
          kNegativeSensitivityLabel, classification);
    } else {
      NOTREACHED();
    }
  }
  return result;
}

VisualClassificationAndEligibility::VisualClassificationAndEligibility() =
    default;
}  // namespace companion::visual_query
