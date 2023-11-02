// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/layout_shift_normalization.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr auto NEW_SHIFT_BUFFER_WINDOW_DURATION = 5000;

class LayoutShiftNormalizationTest : public testing::Test {
 public:
  LayoutShiftNormalizationTest() = default;

  void AddNewLayoutShifts(
      const std::vector<page_load_metrics::mojom::LayoutShiftPtr>& new_shifts,
      base::TimeTicks current_time) {
    // Update layout shift normalization.
    layout_shift_normalization_.AddNewLayoutShifts(
        new_shifts, current_time, cumulative_layoutshift_score_);
  }

  const page_load_metrics::NormalizedCLSData& normalized_cls_data() const {
    return layout_shift_normalization_.normalized_cls_data();
  }

  void InsertNewLayoutShifts(
      std::vector<page_load_metrics::mojom::LayoutShiftPtr>& new_shifts,
      base::TimeTicks current_time,
      std::vector<std::pair<int, double>> shifts_data) {
    for (auto shift : shifts_data) {
      new_shifts.emplace_back(page_load_metrics::mojom::LayoutShift::New(
          current_time - base::Milliseconds(std::get<0>(shift)),
          std::get<1>(shift)));
      // Update CLS.
      cumulative_layoutshift_score_ += std::get<1>(shift);
    }
  }

 private:
  page_load_metrics::LayoutShiftNormalization layout_shift_normalization_;
  float cumulative_layoutshift_score_ = 0.0;
};

TEST_F(LayoutShiftNormalizationTest, MultipleShifts) {
  base::TimeTicks current_time = base::TimeTicks::Now();
  std::vector<page_load_metrics::mojom::LayoutShiftPtr> new_shifts;
  // Insert new layout shifts. The insertion order matters.
  InsertNewLayoutShifts(new_shifts, current_time,
                        {{2100, 1.5}, {1800, 1.5}, {1300, 1.5}, {1000, 1.5}});
  // Update CLS normalization data.
  AddNewLayoutShifts(new_shifts, current_time);

  EXPECT_EQ(normalized_cls_data().session_windows_gap1000ms_max5000ms_max_cls,
            6.0);
  EXPECT_EQ(normalized_cls_data().data_tainted, false);
}

TEST_F(LayoutShiftNormalizationTest, MultipleShiftsWithOneStaleShift) {
  base::TimeTicks current_time = base::TimeTicks::Now();
  std::vector<page_load_metrics::mojom::LayoutShiftPtr> new_shifts;
  // Insert new layout shifts. The insertion order matters.
  // Insert the stale shift.
  InsertNewLayoutShifts(new_shifts, current_time,
                        {{NEW_SHIFT_BUFFER_WINDOW_DURATION + 100, 1.5}});

  // Insert non-stale shifts
  InsertNewLayoutShifts(new_shifts, current_time,
                        {{1800, 1.5}, {1300, 1.5}, {1000, 1.5}});
  // Update CLS normalization data.
  AddNewLayoutShifts(new_shifts, current_time);
  EXPECT_EQ(normalized_cls_data().session_windows_gap1000ms_max5000ms_max_cls,
            0.0);
  EXPECT_EQ(normalized_cls_data().data_tainted, true);
}

TEST_F(LayoutShiftNormalizationTest, MultipleShiftsFromTwoRenderers) {
  base::TimeTicks current_time = base::TimeTicks::Now();
  // Insert new layout shifts from one renderer. The insertion order matters.
  std::vector<page_load_metrics::mojom::LayoutShiftPtr> new_shifts_1;
  InsertNewLayoutShifts(new_shifts_1, current_time, {{2100, 1.5}, {1800, 1.5}});
  AddNewLayoutShifts(new_shifts_1, current_time);

  // Insert new layout shifts from the other renderer. The insertion order
  // matters.
  std::vector<page_load_metrics::mojom::LayoutShiftPtr> new_shifts_2;
  InsertNewLayoutShifts(new_shifts_2, current_time, {{4100, 4.0}, {1000, 1.5}});
  AddNewLayoutShifts(new_shifts_2, current_time);

  EXPECT_EQ(normalized_cls_data().session_windows_gap1000ms_max5000ms_max_cls,
            4.5);
  EXPECT_EQ(normalized_cls_data().data_tainted, false);
}

TEST_F(LayoutShiftNormalizationTest, MultipleShiftsFromDifferentTimes) {
  base::TimeTicks current_time = base::TimeTicks::Now();
  // Insert the first set of new layout shifts. The insertion order matters.
  auto current_time_1 = current_time - base::Milliseconds(5000);
  std::vector<page_load_metrics::mojom::LayoutShiftPtr> new_shifts_1;
  InsertNewLayoutShifts(new_shifts_1, current_time_1,
                        {{2100, 1.5}, {1800, 1.5}});
  AddNewLayoutShifts(new_shifts_1, current_time_1);

  // Insert the second set of new layout shifts. The insertion order
  // matters.
  auto current_time_2 = current_time;
  std::vector<page_load_metrics::mojom::LayoutShiftPtr> new_shifts_2;
  InsertNewLayoutShifts(new_shifts_2, current_time_2,
                        {{4100, 2.0}, {1000, 1.5}});
  AddNewLayoutShifts(new_shifts_2, current_time);

  // Insert the third set of new layout shifts. The insertion order
  // matters.
  auto current_time_3 = current_time + base::Milliseconds(6000);
  std::vector<page_load_metrics::mojom::LayoutShiftPtr> new_shifts_3;
  InsertNewLayoutShifts(new_shifts_3, current_time_3, {{0, 0.5}});
  AddNewLayoutShifts(new_shifts_3, current_time_3);
  EXPECT_EQ(normalized_cls_data().session_windows_gap1000ms_max5000ms_max_cls,
            3.0);
  EXPECT_EQ(normalized_cls_data().data_tainted, false);
}
