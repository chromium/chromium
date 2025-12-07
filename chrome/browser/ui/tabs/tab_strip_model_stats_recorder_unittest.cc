// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_stats_recorder.h"

#include <memory>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

class TabStripModelStatsRecorderTest : public ChromeRenderViewHostTestHarness {
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
};

TEST_F(TabStripModelStatsRecorderTest,
       NumberOfOtherTabsActivatedBeforeMadeActive) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());

  TabStripModelStatsRecorder recorder;
  tabstrip.AddObserver(&recorder);

  // Create first tab
  std::unique_ptr<WebContents> contents0 = CreateTestWebContents();
  WebContents* raw_contents0 = contents0.get();
  tabstrip.InsertWebContentsAt(0, std::move(contents0),
                               AddTabTypes::ADD_ACTIVE);

  // Add 9 more tabs and activate them
  for (int i = 1; i < 10; ++i) {
    tabstrip.InsertWebContentsAt(1, CreateTestWebContents(),
                                 AddTabTypes::ADD_ACTIVE);
  }

  // Reactivate the first tab
  tabstrip.ActivateTabAt(tabstrip.GetIndexOfWebContents(raw_contents0),
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));

  tabstrip.RemoveObserver(&recorder);
  tabstrip.CloseAllTabs();
}

TEST_F(TabStripModelStatsRecorderTest,
       NumberOfOtherTabsActivatedBeforeMadeActive_CycleTabs) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());

  TabStripModelStatsRecorder recorder;
  tabstrip.AddObserver(&recorder);

  // Create tab 0, 1, 2
  std::unique_ptr<WebContents> contents0 = CreateTestWebContents();
  WebContents* raw_contents0 = contents0.get();
  std::unique_ptr<WebContents> contents1 = CreateTestWebContents();
  WebContents* raw_contents1 = contents1.get();
  std::unique_ptr<WebContents> contents2 = CreateTestWebContents();
  WebContents* raw_contents2 = contents2.get();
  tabstrip.InsertWebContentsAt(0, std::move(contents0),
                               AddTabTypes::ADD_ACTIVE);
  tabstrip.InsertWebContentsAt(1, std::move(contents1),
                               AddTabTypes::ADD_ACTIVE);
  tabstrip.InsertWebContentsAt(2, std::move(contents2),
                               AddTabTypes::ADD_ACTIVE);

  // Switch between tabs {0,1} for 5 times, then switch to tab 2
  for (int i = 0; i < 5; ++i) {
    tabstrip.ActivateTabAt(
        tabstrip.GetIndexOfWebContents(raw_contents0),
        TabStripUserGestureDetails(
            TabStripUserGestureDetails::GestureType::kOther));
    tabstrip.ActivateTabAt(
        tabstrip.GetIndexOfWebContents(raw_contents1),
        TabStripUserGestureDetails(
            TabStripUserGestureDetails::GestureType::kOther));
  }
  tabstrip.ActivateTabAt(tabstrip.GetIndexOfWebContents(raw_contents2),
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));

  tabstrip.RemoveObserver(&recorder);
  tabstrip.CloseAllTabs();
}

// This histogram is not present in ChromeOS. For more information:
// crbug.com/457294205
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(TabStripModelStatsRecorderTest, TabSelectionCount) {
  base::HistogramTester histogram_tester;
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());

  TabStripModelStatsRecorder recorder;
  tabstrip.AddObserver(&recorder);

  // Create first tab
  std::unique_ptr<WebContents> contents0 = CreateTestWebContents();
  tabstrip.InsertWebContentsAt(0, std::move(contents0),
                               AddTabTypes::ADD_ACTIVE);
  histogram_tester.ExpectBucketCount("Tabs.Selections.Count", 1, 1);

  // Add 2 more tabs.
  for (int i = 1; i < 3; ++i) {
    tabstrip.InsertWebContentsAt(i, CreateTestWebContents(),
                                 AddTabTypes::ADD_NONE);
  }

  // Select all three tabs.
  ui::ListSelectionModel selection_model;
  selection_model.set_active(0);
  selection_model.AddIndexToSelection(0);
  selection_model.AddIndexToSelection(1);
  selection_model.AddIndexToSelection(2);
  tabstrip.SetSelectionFromModel(selection_model);
  histogram_tester.ExpectBucketCount("Tabs.Selections.Count", 3, 1);

  tabstrip.RemoveObserver(&recorder);
  tabstrip.CloseAllTabs();
}
#endif
