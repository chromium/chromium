// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_stats_recorder.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

class TabStripModelStatsRecorderTest : public ChromeRenderViewHostTestHarness {
  tabs::PreventTabFeatureInitialization prevent_;
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
