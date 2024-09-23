// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/toolbar/adaptive_toolbar_bridge.h"

#include "base/run_loop.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using segmentation_platform::MockSegmentationPlatformService;
using testing::_;

namespace adaptive_toolbar {

class AdaptiveToolbarBridgeTest : public ::testing::Test {
 public:
  AdaptiveToolbarBridgeTest(const AdaptiveToolbarBridgeTest&) = delete;
  AdaptiveToolbarBridgeTest& operator=(const AdaptiveToolbarBridgeTest&) =
      delete;

  ~AdaptiveToolbarBridgeTest() override = default;

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<MockSegmentationPlatformService>();
        }));

    profile_ = builder.Build();
  }

  void TearDown() override {
    // Clear default actions for safe teardown.
    testing::Mock::VerifyAndClear(&GetSegmentationPlatformService());
  }

  AdaptiveToolbarBridgeTest() = default;

  MockSegmentationPlatformService& GetSegmentationPlatformService() {
    return *static_cast<MockSegmentationPlatformService*>(
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetForProfile(profile_.get()));
  }

  void BridgeCallback(bool is_ready, std::vector<int> ranked_buttons) {
    callback_buttons_ = ranked_buttons;
    callback_is_ready_ = is_ready;
    run_loop_.Quit();
  }

 protected:
  // Needed for TestingProfile::Builder.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  std::vector<int> callback_buttons_;
  bool callback_is_ready_;
  base::RunLoop run_loop_;
};

TEST_F(AdaptiveToolbarBridgeTest, GetRankedButtons) {
  Profile* profile = profile_.get();

  ON_CALL(GetSegmentationPlatformService(), GetClassificationResult(_, _, _, _))
      .WillByDefault(testing::WithArg<3>(testing::Invoke(
          [](segmentation_platform::ClassificationResultCallback callback) {
            auto result = segmentation_platform::ClassificationResult(
                segmentation_platform::PredictionStatus::kSucceeded);
            // Set segmentation to return a sorted list of labels.
            result.ordered_labels = {
                segmentation_platform::kAdaptiveToolbarModelLabelShare,
                segmentation_platform::kAdaptiveToolbarModelLabelAddToBookmarks,
                segmentation_platform::kAdaptiveToolbarModelLabelTranslate};
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), result));
          })));

  adaptive_toolbar::GetRankedSessionVariantButtons(
      profile, /* use_raw_results= */ false,
      base::BindOnce(&AdaptiveToolbarBridgeTest::BridgeCallback,
                     base::Unretained(this)));
  run_loop_.Run();

  EXPECT_TRUE(callback_is_ready_);
  // The returned enum values should match the order of the segmentation result.
  std::vector<int> expected_buttons = {
      static_cast<int>(AdaptiveToolbarButtonVariant::kShare),
      static_cast<int>(AdaptiveToolbarButtonVariant::kAddToBookmarks),
      static_cast<int>(AdaptiveToolbarButtonVariant::kTranslate)};
  EXPECT_EQ(callback_buttons_, expected_buttons);
}

TEST_F(AdaptiveToolbarBridgeTest, GetRankedButtons_NotReady) {
  Profile* profile = profile_.get();

  ON_CALL(GetSegmentationPlatformService(), GetClassificationResult(_, _, _, _))
      .WillByDefault(testing::WithArg<3>(testing::Invoke(
          [](segmentation_platform::ClassificationResultCallback callback) {
            // Set segmentation to return kNotReady.
            auto result = segmentation_platform::ClassificationResult(
                segmentation_platform::PredictionStatus::kNotReady);
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), result));
          })));

  adaptive_toolbar::GetRankedSessionVariantButtons(
      profile, /* use_raw_results= */ false,
      base::BindOnce(&AdaptiveToolbarBridgeTest::BridgeCallback,
                     base::Unretained(this)));
  run_loop_.Run();

  EXPECT_FALSE(callback_is_ready_);
  std::vector<int> expected_buttons = {
      static_cast<int>(AdaptiveToolbarButtonVariant::kUnknown),
  };
  EXPECT_EQ(callback_buttons_, expected_buttons);
}

TEST_F(AdaptiveToolbarBridgeTest, GetRankedButtons_RawResults) {
  Profile* profile = profile_.get();

  ON_CALL(GetSegmentationPlatformService(),
          GetAnnotatedNumericResult(_, _, _, _))
      .WillByDefault(testing::WithArg<3>(testing::Invoke(
          [](segmentation_platform::AnnotatedNumericResultCallback callback) {
            segmentation_platform::AnnotatedNumericResult result(
                segmentation_platform::PredictionStatus::kSucceeded);
            // Set segmentation to result an annotated numeric result, this
            // includes a list of labels on the model's config and a list of
            // scores. Both lists are parallel, the first score belongs to the
            // first label.
            auto* result_classifier = result.result.mutable_output_config()
                                          ->mutable_predictor()
                                          ->mutable_multi_class_classifier();

            result_classifier->add_class_labels(
                segmentation_platform::kAdaptiveToolbarModelLabelNewTab);
            result_classifier->add_class_labels(
                segmentation_platform::kAdaptiveToolbarModelLabelVoice);
            result_classifier->add_class_labels(
                segmentation_platform::kAdaptiveToolbarModelLabelShare);
            result_classifier->add_class_labels(
                segmentation_platform::kAdaptiveToolbarModelLabelTranslate);
            result_classifier->add_class_labels(
                segmentation_platform::
                    kAdaptiveToolbarModelLabelAddToBookmarks);

            result.result.add_result(0.2);
            result.result.add_result(0.3);
            result.result.add_result(0.7);
            result.result.add_result(0.9);
            result.result.add_result(0.4);

            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), result));
          })));

  adaptive_toolbar::GetRankedSessionVariantButtons(
      profile, /* use_raw_results= */ true,
      base::BindOnce(&AdaptiveToolbarBridgeTest::BridgeCallback,
                     base::Unretained(this)));
  run_loop_.Run();

  EXPECT_TRUE(callback_is_ready_);
  // The returned enum values should be sorted by score.
  std::vector<int> expected_buttons = {
      static_cast<int>(AdaptiveToolbarButtonVariant::kTranslate),
      static_cast<int>(AdaptiveToolbarButtonVariant::kShare),
      static_cast<int>(AdaptiveToolbarButtonVariant::kAddToBookmarks),
      static_cast<int>(AdaptiveToolbarButtonVariant::kVoice),
      static_cast<int>(AdaptiveToolbarButtonVariant::kNewTab),
  };
  EXPECT_EQ(callback_buttons_, expected_buttons);
}

TEST_F(AdaptiveToolbarBridgeTest, GetRankedButtons_RawResults_NotReady) {
  Profile* profile = profile_.get();

  ON_CALL(GetSegmentationPlatformService(),
          GetAnnotatedNumericResult(_, _, _, _))
      .WillByDefault(testing::WithArg<3>(testing::Invoke(
          [](segmentation_platform::AnnotatedNumericResultCallback callback) {
            segmentation_platform::AnnotatedNumericResult result(
                segmentation_platform::PredictionStatus::kNotReady);

            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), result));
          })));

  adaptive_toolbar::GetRankedSessionVariantButtons(
      profile, /* use_raw_results= */ true,
      base::BindOnce(&AdaptiveToolbarBridgeTest::BridgeCallback,
                     base::Unretained(this)));
  run_loop_.Run();

  EXPECT_FALSE(callback_is_ready_);
  std::vector<int> expected_buttons = {
      static_cast<int>(AdaptiveToolbarButtonVariant::kUnknown),
  };
  EXPECT_EQ(callback_buttons_, expected_buttons);
}

}  // namespace adaptive_toolbar
