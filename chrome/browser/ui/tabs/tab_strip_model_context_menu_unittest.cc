// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabStripModelContextMenuTest : public testing::Test {
 public:
  TabStripModelContextMenuTest() : profile_(new TestingProfile()) {}
  TabStripModelContextMenuTest(const TabStripModelContextMenuTest&) = delete;
  TabStripModelContextMenuTest& operator=(const TabStripModelContextMenuTest&) =
      delete;

  void SetUp() override {
    testing::Test::SetUp();
    tab_strip_model_ =
        std::make_unique<TabStripModel>(&delegate_, profile_.get());
  }

  TabStripModel* tab_strip_model() { return tab_strip_model_.get(); }

  TestingProfile* profile() { return profile_.get(); }

  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    return content::WebContentsTester::CreateTestWebContents(profile(),
                                                             nullptr);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  const std::unique_ptr<TestingProfile> profile_;
  const tabs::TabModel::PreventFeatureInitializationForTesting
      prevent_feature_init_for_testing_;
  TestTabStripModelDelegate delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
};

TEST_F(TabStripModelContextMenuTest, CommandCloseTabLogsHistograms) {
  base::HistogramTester histogram_tester;
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);

  // Select a single tab and close it.
  tab_strip_model()->SelectTabAt(0);
  tab_strip_model()->ExecuteContextMenuCommand(0,
                                               TabStripModel::CommandCloseTab);
  histogram_tester.ExpectUniqueSample(
      "Tab.ContextMenu.CloseTab.SelectedTabsCount", 1, 1);

  // Select two tabs and close them.
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->SelectTabAt(0);
  tab_strip_model()->SelectTabAt(1);
  tab_strip_model()->ExecuteContextMenuCommand(0,
                                               TabStripModel::CommandCloseTab);
  histogram_tester.ExpectBucketCount(
      "Tab.ContextMenu.CloseTab.SelectedTabsCount", 2, 1);
}

TEST_F(TabStripModelContextMenuTest, CommandCloseOtherTabsLogsHistograms) {
  base::HistogramTester histogram_tester;
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);

  // Select a single tab and close others.
  tab_strip_model()->SelectTabAt(0);
  tab_strip_model()->ExecuteContextMenuCommand(
      0, TabStripModel::CommandCloseOtherTabs);
  histogram_tester.ExpectUniqueSample(
      "Tab.ContextMenu.CloseOtherTabs.SelectedTabsCount", 1, 1);
}

TEST_F(TabStripModelContextMenuTest, CommandCloseTabsToRightLogsHistograms) {
  base::HistogramTester histogram_tester;
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);

  // Select a single tab and close tabs to the right.
  tab_strip_model()->SelectTabAt(0);
  tab_strip_model()->ExecuteContextMenuCommand(
      0, TabStripModel::CommandCloseTabsToRight);
  histogram_tester.ExpectUniqueSample(
      "Tab.ContextMenu.CloseTabsToRight.SelectedTabsCount", 1, 1);
}

TEST_F(TabStripModelContextMenuTest, CommandTogglePinnedLogsHistograms) {
  base::HistogramTester histogram_tester;
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);

  // Select a single tab and pin it.
  tab_strip_model()->SelectTabAt(0);
  tab_strip_model()->ExecuteContextMenuCommand(
      0, TabStripModel::CommandTogglePinned);
  histogram_tester.ExpectUniqueSample(
      "Tab.ContextMenu.TogglePinned.SelectedTabsCount", 1, 1);
}
