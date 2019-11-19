// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_stats_recorder.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::HistogramTester;
using content::WebContents;

class TabStripModelStatsRecorderTest : public ChromeRenderViewHostTestHarness {
};

TEST_F(TabStripModelStatsRecorderTest, BasicTabLifecycle) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());

  TabStripModelStatsRecorder recorder;
  tabstrip.AddObserver(&recorder);

  HistogramTester tester;

  // Insert the first tab.
  std::unique_ptr<WebContents> contents1 = CreateTestWebContents();
  WebContents* raw_contents1 = contents1.get();
  tabstrip.InsertWebContentsAt(0, std::move(contents1),
                               TabStripModel::ADD_ACTIVE);

  // Deactivate the first tab by inserting new tab.
  std::unique_ptr<WebContents> contents2 = CreateTestWebContents();
  WebContents* raw_contents2 = contents2.get();
  tabstrip.InsertWebContentsAt(1, std::move(contents2),
                               TabStripModel::ADD_ACTIVE);

  tester.ExpectUniqueSample(
      "Tabs.StateTransfer.Target_Active",
      static_cast<int>(TabStripModelStatsRecorder::TabState::INACTIVE), 1);

  // Reactivate the first tab.
  tabstrip.ActivateTabAt(tabstrip.GetIndexOfWebContents(raw_contents1),
                         {TabStripModel::GestureType::kOther});

  tester.ExpectUniqueSample(
      "Tabs.StateTransfer.Target_Active",
      static_cast<int>(TabStripModelStatsRecorder::TabState::INACTIVE), 2);
  tester.ExpectUniqueSample(
      "Tabs.StateTransfer.Target_Inactive",
      static_cast<int>(TabStripModelStatsRecorder::TabState::ACTIVE), 1);
  tester.ExpectUniqueSample(
      "Tabs.StateTransfer.NumberOfOtherTabsActivatedBeforeMadeActive", 1, 1);

  // Replace the contents of the first tab.
  // TabStripModeStatsRecorder should follow WebContents change.
  std::unique_ptr<WebContents> contents3 = CreateTestWebContents();
  tabstrip.ReplaceWebContentsAt(0, std::move(contents3));

  // Close the inactive second tab.
  tabstrip.CloseWebContentsAt(tabstrip.GetIndexOfWebContents(raw_contents2),
                              TabStripModel::CLOSE_USER_GESTURE |
                                  TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);

  tester.ExpectBucketCount(
      "Tabs.StateTransfer.Target_Inactive",
      static_cast<int>(TabStripModelStatsRecorder::TabState::CLOSED), 1);

  // Close the active first tab.
  tabstrip.CloseSelectedTabs();
  tester.ExpectBucketCount(
      "Tabs.StateTransfer.Target_Active",
      static_cast<int>(TabStripModelStatsRecorder::TabState::CLOSED), 1);

  tabstrip.RemoveObserver(&recorder);

  tabstrip.CloseAllTabs();
}

TEST_F(TabStripModelStatsRecorderTest, ObserveMultipleTabStrips) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip1(&delegate, profile());
  TabStripModel tabstrip2(&delegate, profile());

  TabStripModelStatsRecorder recorder;
  tabstrip1.AddObserver(&recorder);
  tabstrip2.AddObserver(&recorder);

  HistogramTester tester;

  // Create a tab in strip 1.
  tabstrip1.InsertWebContentsAt(0, CreateTestWebContents(),
                                TabStripModel::ADD_ACTIVE);

  // Create a tab in strip 2.
  tabstrip2.InsertWebContentsAt(0, CreateTestWebContents(),
                                TabStripModel::ADD_ACTIVE);

  // Create another tab in strip 1.
  tabstrip1.InsertWebContentsAt(1, CreateTestWebContents(),
                                TabStripModel::ADD_ACTIVE);

  tester.ExpectUniqueSample(
      "Tabs.StateTransfer.Target_Active",
      static_cast<int>(TabStripModelStatsRecorder::TabState::INACTIVE), 1);

  // Create another tab in strip 2.
  tabstrip2.InsertWebContentsAt(1, CreateTestWebContents(),
                                TabStripModel::ADD_ACTIVE);

  tester.ExpectUniqueSample(
      "Tabs.StateTransfer.Target_Active",
      static_cast<int>(TabStripModelStatsRecorder::TabState::INACTIVE), 2);

  // Move the first tab in strip 1 to strip 2
  tabstrip2.InsertWebContentsAt(2, tabstrip1.DetachWebContentsAt(0),
                                TabStripModel::ADD_ACTIVE);

  tester.ExpectUniqueSample(
      "Tabs.StateTransfer.Target_Active",
      static_cast<int>(TabStripModelStatsRecorder::TabState::INACTIVE), 3);
  tester.ExpectUniqueSample(
      "Tabs.StateTransfer.Target_Inactive",
      static_cast<int>(TabStripModelStatsRecorder::TabState::ACTIVE), 1);

  // Switch to the first tab in strip 2.
  tabstrip2.ActivateTabAt(0, {TabStripModel::GestureType::kOther});
  tester.ExpectUniqueSample(
      "Tabs.StateTransfer.Target_Active",
      static_cast<int>(TabStripModelStatsRecorder::TabState::INACTIVE), 4);
  tester.ExpectUniqueSample(
      "Tabs.StateTransfer.Target_Inactive",
      static_cast<int>(TabStripModelStatsRecorder::TabState::ACTIVE), 2);

  // Close the first tab in strip 2.
  tabstrip2.CloseSelectedTabs();
  tester.ExpectBucketCount(
      "Tabs.StateTransfer.Target_Active",
      static_cast<int>(TabStripModelStatsRecorder::TabState::CLOSED), 1);

  tabstrip1.RemoveObserver(&recorder);
  tabstrip2.RemoveObserver(&recorder);

  tabstrip1.CloseAllTabs();
  tabstrip2.CloseAllTabs();
}

TEST_F(TabStripModelStatsRecorderTest,
       NumberOfOtherTabsActivatedBeforeMadeActive) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());

  TabStripModelStatsRecorder recorder;
  tabstrip.AddObserver(&recorder);

  HistogramTester tester;

  // Create first tab
  std::unique_ptr<WebContents> contents0 = CreateTestWebContents();
  WebContents* raw_contents0 = contents0.get();
  tabstrip.InsertWebContentsAt(0, std::move(contents0),
                               TabStripModel::ADD_ACTIVE);

  // Add 9 more tabs and activate them
  for (int i = 1; i < 10; ++i) {
    tabstrip.InsertWebContentsAt(1, CreateTestWebContents(),
                                 TabStripModel::ADD_ACTIVE);
  }

  // Reactivate the first tab
  tabstrip.ActivateTabAt(tabstrip.GetIndexOfWebContents(raw_contents0),
                         {TabStripModel::GestureType::kOther});

  tester.ExpectUniqueSample(
      "Tabs.StateTransfer.NumberOfOtherTabsActivatedBeforeMadeActive", 9, 1);

  tabstrip.RemoveObserver(&recorder);
  tabstrip.CloseAllTabs();
}

TEST_F(TabStripModelStatsRecorderTest,
       NumberOfOtherTabsActivatedBeforeMadeActive_CycleTabs) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());

  TabStripModelStatsRecorder recorder;
  tabstrip.AddObserver(&recorder);

  HistogramTester tester;

  // Create tab 0, 1, 2
  std::unique_ptr<WebContents> contents0 = CreateTestWebContents();
  WebContents* raw_contents0 = contents0.get();
  std::unique_ptr<WebContents> contents1 = CreateTestWebContents();
  WebContents* raw_contents1 = contents1.get();
  std::unique_ptr<WebContents> contents2 = CreateTestWebContents();
  WebContents* raw_contents2 = contents2.get();
  tabstrip.InsertWebContentsAt(0, std::move(contents0),
                               TabStripModel::ADD_ACTIVE);
  tabstrip.InsertWebContentsAt(1, std::move(contents1),
                               TabStripModel::ADD_ACTIVE);
  tabstrip.InsertWebContentsAt(2, std::move(contents2),
                               TabStripModel::ADD_ACTIVE);

  // Switch between tabs {0,1} for 5 times, then switch to tab 2
  for (int i = 0; i < 5; ++i) {
    tabstrip.ActivateTabAt(tabstrip.GetIndexOfWebContents(raw_contents0),
                           {TabStripModel::GestureType::kOther});
    tabstrip.ActivateTabAt(tabstrip.GetIndexOfWebContents(raw_contents1),
                           {TabStripModel::GestureType::kOther});
  }
  tabstrip.ActivateTabAt(tabstrip.GetIndexOfWebContents(raw_contents2),
                         {TabStripModel::GestureType::kOther});

  EXPECT_THAT(
      tester.GetAllSamples(
          "Tabs.StateTransfer.NumberOfOtherTabsActivatedBeforeMadeActive"),
      testing::ElementsAre(base::Bucket(1, 8), base::Bucket(2, 2),
                           base::Bucket(10, 1)));

  tabstrip.RemoveObserver(&recorder);
  tabstrip.CloseAllTabs();
}
