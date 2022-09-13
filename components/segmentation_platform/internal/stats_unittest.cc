// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/stats.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
using proto::SignalType;
namespace stats {

TEST(StatsTest, ModelExecutionZeroValuePercent) {
  base::HistogramTester tester;
  std::vector<float> empty{};
  std::vector<float> single_zero{0};
  std::vector<float> single_non_zero{1};
  std::vector<float> all_zeroes{0, 0, 0};
  std::vector<float> one_non_zero{0, 2, 0};
  std::vector<float> all_non_zero{1, 2, 3};

  RecordModelExecutionZeroValuePercent(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, empty);
  EXPECT_EQ(
      1, tester.GetBucketCount(
             "SegmentationPlatform.ModelExecution.ZeroValuePercent.NewTab", 0));

  RecordModelExecutionZeroValuePercent(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, single_zero);
  EXPECT_EQ(
      1,
      tester.GetBucketCount(
          "SegmentationPlatform.ModelExecution.ZeroValuePercent.NewTab", 100));

  RecordModelExecutionZeroValuePercent(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, single_non_zero);
  EXPECT_EQ(
      2, tester.GetBucketCount(
             "SegmentationPlatform.ModelExecution.ZeroValuePercent.NewTab", 0));

  RecordModelExecutionZeroValuePercent(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, all_zeroes);
  EXPECT_EQ(
      2,
      tester.GetBucketCount(
          "SegmentationPlatform.ModelExecution.ZeroValuePercent.NewTab", 100));

  RecordModelExecutionZeroValuePercent(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, one_non_zero);
  EXPECT_EQ(
      1,
      tester.GetBucketCount(
          "SegmentationPlatform.ModelExecution.ZeroValuePercent.NewTab", 66));

  RecordModelExecutionZeroValuePercent(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, all_non_zero);
  EXPECT_EQ(
      3, tester.GetBucketCount(
             "SegmentationPlatform.ModelExecution.ZeroValuePercent.NewTab", 0));
}

TEST(StatsTest, AdaptiveToolbarSegmentSwitch) {
  std::string histogram("SegmentationPlatform.AdaptiveToolbar.SegmentSwitched");
  base::HistogramTester tester;

  // Share -> New tab.
  RecordSegmentSelectionComputed(
      kAdaptiveToolbarSegmentationKey,
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);

  // None -> Share.
  RecordSegmentSelectionComputed(
      kAdaptiveToolbarSegmentationKey,
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE, absl::nullopt);

  // Share -> Share.
  RecordSegmentSelectionComputed(
      kAdaptiveToolbarSegmentationKey,
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
  tester.ExpectTotalCount(histogram, 2);

  EXPECT_THAT(
      tester.GetAllSamples(histogram),
      testing::ElementsAre(
          base::Bucket(
              static_cast<int>(AdaptiveToolbarSegmentSwitch::kNoneToShare), 1),
          base::Bucket(
              static_cast<int>(AdaptiveToolbarSegmentSwitch::kShareToNewTab),
              1)));
  tester.ExpectTotalCount(
      "SegmentationPlatform.AdaptiveToolbar.SegmentSelection.Computed", 3);
}

TEST(StatsTest, BooleanSegmentSwitch) {
  std::string histogram(
      "SegmentationPlatform.ChromeStartAndroid.SegmentSwitched");
  base::HistogramTester tester;

  // Start to none.
  RecordSegmentSelectionComputed(
      kChromeStartAndroidSegmentationKey,
      SegmentId::OPTIMIZATION_TARGET_UNKNOWN,
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID);

  tester.ExpectTotalCount(histogram, 1);
  EXPECT_THAT(tester.GetAllSamples(histogram),
              testing::ElementsAre(base::Bucket(
                  static_cast<int>(BooleanSegmentSwitch::kEnabledToNone), 1)));
  // None to start.
  RecordSegmentSelectionComputed(
      kChromeStartAndroidSegmentationKey,
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID,
      absl::nullopt);

  tester.ExpectTotalCount(histogram, 2);

  EXPECT_THAT(
      tester.GetAllSamples(histogram),
      testing::ElementsAre(
          base::Bucket(static_cast<int>(BooleanSegmentSwitch::kNoneToEnabled),
                       1),
          base::Bucket(static_cast<int>(BooleanSegmentSwitch::kEnabledToNone),
                       1)));
  tester.ExpectTotalCount(
      "SegmentationPlatform.ChromeStartAndroid.SegmentSelection.Computed2", 2);
}

TEST(StatsTest, SignalsListeningCount) {
  base::HistogramTester tester;
  std::set<uint64_t> user_actions{1, 2, 3, 4};
  std::set<std::pair<std::string, proto::SignalType>> histograms;
  histograms.insert(std::make_pair("hist1", SignalType::HISTOGRAM_ENUM));
  histograms.insert(std::make_pair("hist2", SignalType::HISTOGRAM_ENUM));
  histograms.insert(std::make_pair("hist3", SignalType::HISTOGRAM_ENUM));
  histograms.insert(std::make_pair("hist4", SignalType::HISTOGRAM_VALUE));
  histograms.insert(std::make_pair("hist5", SignalType::HISTOGRAM_VALUE));

  RecordSignalsListeningCount(user_actions, histograms);

  EXPECT_EQ(1,
            tester.GetBucketCount(
                "SegmentationPlatform.Signals.ListeningCount.UserAction", 4));
  EXPECT_EQ(
      1, tester.GetBucketCount(
             "SegmentationPlatform.Signals.ListeningCount.HistogramEnum", 3));
  EXPECT_EQ(
      1, tester.GetBucketCount(
             "SegmentationPlatform.Signals.ListeningCount.HistogramValue", 2));
}

TEST(StatsTest, TrainingDataCollectionEvent) {
  base::HistogramTester tester;
  RecordTrainingDataCollectionEvent(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
      TrainingDataCollectionEvent::kImmediateCollectionStart);
  EXPECT_EQ(1,
            tester.GetBucketCount(
                "SegmentationPlatform.TrainingDataCollectionEvents.Share", 0));
}

}  // namespace stats
}  // namespace segmentation_platform
