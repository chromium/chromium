// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/companion/visual_search/visual_search_classification_and_eligibility.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace companion::visual_search {

class VisualClassificationAndEligibilityTest : public testing::Test {
 public:
  VisualClassificationAndEligibilityTest() = default;
  ~VisualClassificationAndEligibilityTest() override = default;
  void SetUp() override {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    base::FilePath model_file_path =
        source_root_dir.AppendASCII("chrome")
            .AppendASCII("test")
            .AppendASCII("data")
            .AppendASCII("companion_visual_search")
            .AppendASCII("test-model-quantized.tflite");
    ASSERT_TRUE(base::ReadFileToString(model_file_path, &model_bytes_));
  }

  const SkBitmap CreateBitmap(int width, int height, int r, int g, int b) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    bitmap.eraseARGB(255, r, g, b);
    return bitmap;
  }

 protected:
  std::string model_bytes_;
};

TEST_F(VisualClassificationAndEligibilityTest, TestCreateAndRun) {
  EligibilitySpec spec;
  auto* rules = spec.add_cheap_pruning_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::IMAGE_ONPAGE_AREA);
  rules->set_thresholding_op(FeatureLibrary::GT);
  rules->set_threshold(100);
  rules = spec.add_classifier_score_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::SHOPPING_CLASSIFIER_SCORE);
  rules->set_thresholding_op(FeatureLibrary::GT);
  rules->set_threshold(0.33);
  rules = spec.add_classifier_score_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::SENS_CLASSIFIER_SCORE);
  rules->set_thresholding_op(FeatureLibrary::LT);
  rules->set_threshold(0.5);

  std::unique_ptr<VisualClassificationAndEligibility> created =
      VisualClassificationAndEligibility::Create(model_bytes_, spec);
  ASSERT_TRUE(created != nullptr);

  // Test classifier shoppy score = 0.375; sens score = 0.434.
  SingleImageFeaturesAndBytes features_and_bytes1;
  features_and_bytes1.features.image_identifier = "blue_image";
  features_and_bytes1.image_contents = CreateBitmap(1000, 1000, 0, 0, 225);
  features_and_bytes1.features.onpage_rect = Rect(0, 0, 50, 10);
  base::flat_map<ImageId, SingleImageFeaturesAndBytes> image_map;
  image_map[features_and_bytes1.features.image_identifier] =
      features_and_bytes1;

  // Test classifier shoppy score = 0.375; sens score = 0.434.
  SingleImageFeaturesAndBytes features_and_bytes2;
  features_and_bytes2.features.image_identifier = "small_blue_image";
  features_and_bytes2.image_contents = CreateBitmap(1000, 1000, 0, 0, 225);
  features_and_bytes2.features.onpage_rect = Rect(0, 0, 5, 1);
  image_map[features_and_bytes2.features.image_identifier] =
      features_and_bytes2;

  // Test classifier shoppy score = 0.38; sens score = 0.346.
  SingleImageFeaturesAndBytes features_and_bytes3;
  features_and_bytes3.features.image_identifier = "red_image";
  features_and_bytes3.image_contents = CreateBitmap(1000, 1000, 225, 0, 0);
  features_and_bytes3.features.onpage_rect = Rect(0, 0, 50, 10);
  image_map[features_and_bytes3.features.image_identifier] =
      features_and_bytes3;

  // Test classifier shoppy score = 0.319; sens score = 0.539.
  SingleImageFeaturesAndBytes features_and_bytes4;
  features_and_bytes4.features.image_identifier = "green_image";
  features_and_bytes4.image_contents = CreateBitmap(1000, 1000, 0, 225, 0);
  features_and_bytes4.features.onpage_rect = Rect(0, 0, 50, 10);
  image_map[features_and_bytes4.features.image_identifier] =
      features_and_bytes4;

  SizeF viewport_size(1000.0, 500.0);
  const std::vector<ImageId> result =
      created->RunClassificationAndEligibility(image_map, viewport_size);
  ASSERT_EQ(result.size(), 2U);
  EXPECT_THAT(result, testing::UnorderedElementsAre("blue_image", "red_image"));
}

TEST_F(VisualClassificationAndEligibilityTest, TestShoppyScoreIsCorrect) {
  EligibilitySpec spec;
  auto* rules = spec.add_cheap_pruning_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::IMAGE_ONPAGE_AREA);
  rules->set_thresholding_op(FeatureLibrary::GT);
  rules->set_threshold(100);
  // Counter-intuitive rule designed to check that sensitivity and shoppy
  // scores are not swapped or messed up in some other way.
  rules = spec.add_classifier_score_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::SHOPPING_CLASSIFIER_SCORE);
  rules->set_thresholding_op(FeatureLibrary::LT);
  rules->set_threshold(0.4);

  std::unique_ptr<VisualClassificationAndEligibility> created =
      VisualClassificationAndEligibility::Create(model_bytes_, spec);
  ASSERT_TRUE(created != nullptr);

  // Test classifier shoppy score = 0.375; sens score = 0.434.
  SingleImageFeaturesAndBytes features_and_bytes1;
  features_and_bytes1.features.image_identifier = "blue_image";
  features_and_bytes1.image_contents = CreateBitmap(1000, 1000, 0, 0, 225);
  features_and_bytes1.features.onpage_rect = Rect(0, 0, 50, 10);
  base::flat_map<ImageId, SingleImageFeaturesAndBytes> image_map;
  image_map[features_and_bytes1.features.image_identifier] =
      features_and_bytes1;

  SizeF viewport_size(1000.0, 500.0);
  const std::vector<ImageId> result =
      created->RunClassificationAndEligibility(image_map, viewport_size);
  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result.at(0), "blue_image");
}

TEST_F(VisualClassificationAndEligibilityTest, TestSensScoreIsCorrect) {
  EligibilitySpec spec;
  auto* rules = spec.add_cheap_pruning_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::IMAGE_ONPAGE_AREA);
  rules->set_thresholding_op(FeatureLibrary::GT);
  rules->set_threshold(100);
  // Counter-intuitive rule designed to check that sensitivity and shoppy
  // scores are not swapped or messed up in some other way.
  rules = spec.add_classifier_score_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::SENS_CLASSIFIER_SCORE);
  rules->set_thresholding_op(FeatureLibrary::GT);
  rules->set_threshold(0.4);

  std::unique_ptr<VisualClassificationAndEligibility> created =
      VisualClassificationAndEligibility::Create(model_bytes_, spec);
  ASSERT_TRUE(created != nullptr);

  // Test classifier shoppy score = 0.375; sens score = 0.434.
  SingleImageFeaturesAndBytes features_and_bytes1;
  features_and_bytes1.features.image_identifier = "blue_image";
  features_and_bytes1.image_contents = CreateBitmap(1000, 1000, 0, 0, 225);
  features_and_bytes1.features.onpage_rect = Rect(0, 0, 50, 10);
  base::flat_map<ImageId, SingleImageFeaturesAndBytes> image_map;
  image_map[features_and_bytes1.features.image_identifier] =
      features_and_bytes1;

  SizeF viewport_size(1000.0, 500.0);
  const std::vector<ImageId> result =
      created->RunClassificationAndEligibility(image_map, viewport_size);
  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result.at(0), "blue_image");
}

TEST_F(VisualClassificationAndEligibilityTest, TestInvalidCreation) {
  {
    // Spec with missing feature name in cheap pruning rules.
    EligibilitySpec spec;
    auto* rules = spec.add_cheap_pruning_rules()->add_rules();
    rules->set_thresholding_op(FeatureLibrary::GT);
    rules->set_threshold(100);
    std::unique_ptr<VisualClassificationAndEligibility> created =
        VisualClassificationAndEligibility::Create(model_bytes_, spec);
    EXPECT_EQ(created, nullptr);
  }
  {
    // Spec with missing feature op in classifier score rules.
    EligibilitySpec spec;
    auto* rules = spec.add_classifier_score_rules()->add_rules();
    rules->set_feature_name(FeatureLibrary::SHOPPING_CLASSIFIER_SCORE);
    rules->set_threshold(100);
    std::unique_ptr<VisualClassificationAndEligibility> created =
        VisualClassificationAndEligibility::Create(model_bytes_, spec);
    EXPECT_EQ(created, nullptr);
  }
  {
    // Spec in which the normalizing_feature_name is set to unspecified.
    EligibilitySpec spec;
    auto* rules = spec.add_post_renormalization_rules()->add_rules();
    rules->set_feature_name(FeatureLibrary::IMAGE_ONPAGE_AREA);
    rules->set_normalizing_op(FeatureLibrary::NORMALIZE_UNSPECIFIED);
    rules->set_threshold(100);
    std::unique_ptr<VisualClassificationAndEligibility> created =
        VisualClassificationAndEligibility::Create(model_bytes_, spec);
    EXPECT_EQ(created, nullptr);
  }
  {
    // Good spec, but we send junk for model_bytes.
    EligibilitySpec spec;
    auto* rules = spec.add_cheap_pruning_rules()->add_rules();
    rules->set_feature_name(FeatureLibrary::IMAGE_ONPAGE_AREA);
    rules->set_thresholding_op(FeatureLibrary::GT);
    rules->set_threshold(100);
    std::unique_ptr<VisualClassificationAndEligibility> created =
        VisualClassificationAndEligibility::Create(
            "model_bytes_or_something_idk", spec);
    EXPECT_EQ(created, nullptr);
  }
}
}  // namespace companion::visual_search
