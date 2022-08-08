// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/side_panel_toolbar_button.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/read_later/read_later_test_utils.h"
#include "chrome/browser/ui/read_later/reading_list_model_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/reading_list/core/reading_list_model.h"
#include "ui/events/event_utils.h"
#include "ui/views/test/button_test_api.h"

class SidePanelToolbarButtonTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    TestWithBrowserView::SetUp();

    model_ = ReadingListModelFactory::GetForBrowserContext(profile());
    test::ReadingListLoadObserver(model_).Wait();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories =
        TestWithBrowserView::GetTestingFactories();
    factories.emplace_back(
        ReadingListModelFactory::GetInstance(),
        ReadingListModelFactory::GetDefaultFactoryForTesting());
    return factories;
  }

  SidePanelToolbarButton* GetSidePanelToolbarButton() {
    return browser_view()->toolbar()->side_panel_button();
  }

  ReadingListModel* model() { return model_; }

 private:
  raw_ptr<ReadingListModel> model_;
};

TEST_F(SidePanelToolbarButtonTest, DotIndicatorVisibleWithUnreadItems) {
  if (browser_view()->side_panel_coordinator()) {
    GTEST_SKIP() << "The unified side panel doesn't use the dot indicator so "
                    "this test shouldn't run";
  }
  // Verify the dot indicator is seen when there is an unseen entry.
  model()->AddEntry(GURL("http://foo/1"), "Tab 1",
                    reading_list::EntrySource::ADDED_VIA_CURRENT_APP);
  SidePanelToolbarButton* const side_panel_button = GetSidePanelToolbarButton();
  ASSERT_TRUE(side_panel_button->GetDotIndicatorVisibilityForTesting());

  // Verify the dot indicator is hidden once the toolbar button is clicked.
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(side_panel_button);
  test_api.NotifyClick(e);
  ASSERT_FALSE(side_panel_button->GetDotIndicatorVisibilityForTesting());

  // Verify the dot indicator is hidden when entries are added while the panel
  // is open.
  model()->AddEntry(GURL("http://foo/2"), "Tab 2",
                    reading_list::EntrySource::ADDED_VIA_CURRENT_APP);
  ASSERT_FALSE(side_panel_button->GetDotIndicatorVisibilityForTesting());
}
