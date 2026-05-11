// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_sessions/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabsFromOtherDevicesSidePanelMetricsTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(TabsFromOtherDevicesSidePanelMetricsTest, RecordEvents_List) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(sync_sessions::kSyncTabScreenshots);

  base::HistogramTester histogram_tester;
  TabsFromOtherDevicesSidePanelMetrics metrics;

  histogram_tester.ExpectBucketCount(
      "Sync.TabsFromOtherDevicesSidePanel.List.Events",
      TabsFromOtherDevicesSidePanelMetrics::Event::kStartup, 1);

  metrics.OnEntryShown(nullptr);
  histogram_tester.ExpectBucketCount(
      "Sync.TabsFromOtherDevicesSidePanel.List.Events",
      TabsFromOtherDevicesSidePanelMetrics::Event::kOpened, 1);

  metrics.RecordTabOpened();
  histogram_tester.ExpectBucketCount(
      "Sync.TabsFromOtherDevicesSidePanel.List.Events",
      TabsFromOtherDevicesSidePanelMetrics::Event::kTabOpened, 1);

  metrics.OnEntryHidden(nullptr);
  histogram_tester.ExpectBucketCount(
      "Sync.TabsFromOtherDevicesSidePanel.List.Events",
      TabsFromOtherDevicesSidePanelMetrics::Event::kClosed, 1);
}

TEST_F(TabsFromOtherDevicesSidePanelMetricsTest, RecordEvents_Screenshot) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(sync_sessions::kSyncTabScreenshots);

  base::HistogramTester histogram_tester;
  TabsFromOtherDevicesSidePanelMetrics metrics;

  histogram_tester.ExpectBucketCount(
      "Sync.TabsFromOtherDevicesSidePanel.Screenshot.Events",
      TabsFromOtherDevicesSidePanelMetrics::Event::kStartup, 1);

  metrics.OnEntryShown(nullptr);
  histogram_tester.ExpectBucketCount(
      "Sync.TabsFromOtherDevicesSidePanel.Screenshot.Events",
      TabsFromOtherDevicesSidePanelMetrics::Event::kOpened, 1);

  metrics.RecordTabOpened();
  histogram_tester.ExpectBucketCount(
      "Sync.TabsFromOtherDevicesSidePanel.Screenshot.Events",
      TabsFromOtherDevicesSidePanelMetrics::Event::kTabOpened, 1);

  metrics.OnEntryHidden(nullptr);
  histogram_tester.ExpectBucketCount(
      "Sync.TabsFromOtherDevicesSidePanel.Screenshot.Events",
      TabsFromOtherDevicesSidePanelMetrics::Event::kClosed, 1);
}

TEST_F(TabsFromOtherDevicesSidePanelMetricsTest,
       RecordTimeToFirstTab_OnlyOnce) {
  base::HistogramTester histogram_tester;
  TabsFromOtherDevicesSidePanelMetrics metrics;

  metrics.OnEntryShown(nullptr);

  metrics.RecordTabOpened();
  histogram_tester.ExpectTotalCount(
      "Sync.TabsFromOtherDevicesSidePanel.List.TimeToFirstTab", 1);

  // Second call should not record TimeToFirstTab again.
  metrics.RecordTabOpened();
  histogram_tester.ExpectTotalCount(
      "Sync.TabsFromOtherDevicesSidePanel.List.TimeToFirstTab", 1);
}

TEST_F(TabsFromOtherDevicesSidePanelMetricsTest, RecordTimeSpentOpen) {
  base::HistogramTester histogram_tester;
  TabsFromOtherDevicesSidePanelMetrics metrics;

  metrics.OnEntryShown(nullptr);
  metrics.OnEntryHidden(nullptr);

  histogram_tester.ExpectTotalCount(
      "Sync.TabsFromOtherDevicesSidePanel.List.TimeSpentOpen", 1);
}
