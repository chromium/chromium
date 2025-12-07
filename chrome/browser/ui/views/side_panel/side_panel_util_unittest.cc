// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_util.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "testing/gtest/include/gtest/gtest.h"

class SidePanelUtilTest : public testing::Test {
 public:
  void SetUp() override {
    task_environment_ = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  }

 protected:
  base::HistogramTester histogram_tester_;
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
};

TEST_F(SidePanelUtilTest, RecordDuration_ForReadAnythingEntry) {
  const base::TimeTicks shown_timestamp = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Hours(1);
  task_environment_->FastForwardBy(duration);

  SidePanelUtil::RecordEntryHiddenMetrics(SidePanelEntry::PanelType::kContent,
                                          SidePanelEntryId::kReadAnything,
                                          shown_timestamp);

  histogram_tester_.ExpectTimeBucketCount(
      "SidePanel.ReadAnything.ShownDuration", duration, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "SidePanel.ReadAnything.ShownDurationMax1Day", duration, 1);
}

TEST_F(SidePanelUtilTest, RecordsMaxCap_WhenDurationExceedsOneDay) {
  const base::TimeTicks shown_timestamp = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Hours(25);
  task_environment_->FastForwardBy(duration);

  SidePanelUtil::RecordEntryHiddenMetrics(SidePanelEntry::PanelType::kContent,
                                          SidePanelEntryId::kReadAnything,
                                          shown_timestamp);

  histogram_tester_.ExpectTimeBucketCount(
      "SidePanel.ReadAnything.ShownDurationMax1Day", base::Hours(24), 1);
}
