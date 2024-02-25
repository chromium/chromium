// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"

#include "base/test/task_environment.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class SegmentationResultPrefsTest : public testing::Test {
 public:
  SegmentationResultPrefsTest() = default;
  ~SegmentationResultPrefsTest() override = default;

  void SetUp() override {
    result_prefs_ = std::make_unique<SegmentationResultPrefs>(&pref_service_);
    pref_service_.registry()->RegisterDictionaryPref(kSegmentationResultPref);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<SegmentationResultPrefs> result_prefs_;
};

TEST_F(SegmentationResultPrefsTest, WriteResultAndRead) {
  std::string result_key = "some_key";
  // Start test with no result.
  std::optional<SelectedSegment> current_result =
      result_prefs_->ReadSegmentationResultFromPref(result_key);
  EXPECT_FALSE(current_result.has_value());

  // Save a result. Verify by reading the result back.
  SegmentId segment_id = SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  SelectedSegment selected_segment(segment_id, std::nullopt);
  result_prefs_->SaveSegmentationResultToPref(result_key, selected_segment);
  current_result = result_prefs_->ReadSegmentationResultFromPref(result_key);
  EXPECT_TRUE(current_result.has_value());
  EXPECT_EQ(segment_id, current_result->segment_id);
  EXPECT_FALSE(current_result->rank);
  EXPECT_FALSE(current_result->in_use);
  EXPECT_EQ(base::Time(), current_result->selection_time);

  // Overwrite the result with a new segment.
  selected_segment.segment_id =
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
  selected_segment.in_use = true;
  base::Time now = base::Time::Now();
  selected_segment.selection_time = now;
  selected_segment.rank = 10;
  result_prefs_->SaveSegmentationResultToPref(result_key, selected_segment);
  current_result = result_prefs_->ReadSegmentationResultFromPref(result_key);
  EXPECT_TRUE(current_result.has_value());
  EXPECT_EQ(selected_segment.segment_id, current_result->segment_id);
  ASSERT_TRUE(current_result->rank);
  EXPECT_EQ(10, *current_result->rank);
  EXPECT_TRUE(current_result->in_use);
  EXPECT_EQ(now, current_result->selection_time);

  // Write another result with a different key. This shouldn't overwrite the
  // first key.
  std::string result_key2 = "some_key2";
  selected_segment.segment_id =
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE;
  selected_segment.rank = 20;
  result_prefs_->SaveSegmentationResultToPref(result_key2, selected_segment);
  current_result = result_prefs_->ReadSegmentationResultFromPref(result_key2);
  EXPECT_TRUE(current_result.has_value());
  EXPECT_EQ(selected_segment.segment_id, current_result->segment_id);
  EXPECT_EQ(20, *current_result->rank);

  current_result = result_prefs_->ReadSegmentationResultFromPref(result_key);
  EXPECT_TRUE(current_result.has_value());
  EXPECT_EQ(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
            current_result->segment_id);
  EXPECT_EQ(10, *current_result->rank);

  // Save empty result. It should delete the current result.
  result_prefs_->SaveSegmentationResultToPref(result_key, std::nullopt);
  current_result = result_prefs_->ReadSegmentationResultFromPref(result_key);
  EXPECT_FALSE(current_result.has_value());
}

TEST_F(SegmentationResultPrefsTest, CorruptedValue) {
  std::string result_key = "some_key";
  SelectedSegment selected_segment(static_cast<SegmentId>(100), 1);
  result_prefs_->SaveSegmentationResultToPref(result_key, selected_segment);
  std::optional<SelectedSegment> current_result =
      result_prefs_->ReadSegmentationResultFromPref(result_key);
  EXPECT_TRUE(current_result.has_value());
  EXPECT_EQ(100, current_result->segment_id);
}
}  // namespace segmentation_platform
