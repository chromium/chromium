// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_toolbar_ui_controller.h"

#include "base/files/file_path.h"
#include "chrome/browser/download/download_browsertest_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"

// This tests the download button view in the toolbar.
// TODO(chlily): Add more tests to cover all functionality.
class DownloadToolbarUIControllerBrowserTest : public DownloadTestBase {
 public:
  DownloadToolbarUIControllerBrowserTest() = default;

  DownloadToolbarUIController* controller() {
    return browser()->GetFeatures().download_toolbar_ui_controller();
  }

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kToolbarPinning, features::kPinnableDownloadsButton}, {});
    DownloadTestBase::SetUp();
  }

  PinnedToolbarActionsContainer* toolbar_container() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->pinned_toolbar_actions_container();
  }

  ToolbarButton* toolbar_button() {
    auto* container = toolbar_container();
    return container ? container->GetButtonFor(kActionShowDownloads) : nullptr;
  }

 protected:
  void ClickButton(views::Button* button) {
    button->OnMousePressed(
        ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    button->OnMouseReleased(ui::MouseEvent(
        ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  base::test::ScopedFeatureList feature_list_;
};

// DownloadToolbarUIController and downloads toolbar button do not exist for
// ChromeOS Ash. See https://crbug.com/1323505.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest, ShowHide) {
  EXPECT_EQ(toolbar_button(), nullptr);
  controller()->Show();
  views::test::WaitForAnimatingLayoutManager(toolbar_container());
  EXPECT_NE(toolbar_button(), nullptr);
  EXPECT_TRUE(toolbar_button()->GetVisible());
  controller()->Hide();
  views::test::WaitForAnimatingLayoutManager(toolbar_container());
  EXPECT_EQ(toolbar_button(), nullptr);
}

IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       HideDoesNotRemoveButtonIfPinned) {
  EXPECT_EQ(toolbar_button(), nullptr);
  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser()->profile());
  actions_model->UpdatePinnedState(kActionShowDownloads, true);
  views::test::WaitForAnimatingLayoutManager(toolbar_container());
  EXPECT_NE(toolbar_button(), nullptr);
  EXPECT_TRUE(toolbar_button()->GetVisible());
  // Verify calling Hide does not change the visibility of the button when
  // pinned.
  controller()->Hide();
  EXPECT_NE(toolbar_button(), nullptr);
  EXPECT_TRUE(toolbar_button()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       ControllerUpdatesButtonEnabledState) {
  // Pin downloads to the toolbar.
  EXPECT_EQ(toolbar_button(), nullptr);
  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser()->profile());
  actions_model->UpdatePinnedState(kActionShowDownloads, true);
  views::test::WaitForAnimatingLayoutManager(toolbar_container());
  EXPECT_NE(toolbar_button(), nullptr);
  EXPECT_TRUE(toolbar_button()->GetVisible());
  EXPECT_TRUE(toolbar_button()->GetEnabled());
  // Disable the via the controller and verify the button's enabled state.
  controller()->Disable();
  EXPECT_FALSE(toolbar_button()->GetEnabled());
  // Enable the via the controller and verify the button's enabled state.
  controller()->Enable();
  EXPECT_TRUE(toolbar_button()->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       ButtonShowsForDownloadingItems) {
  EXPECT_EQ(toolbar_button(), nullptr);
  // Download a file and verify the download button appears after the download.
  ui_test_utils::DownloadURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath().AppendASCII("downloads"),
                     base::FilePath().AppendASCII("a_zip_file.zip")));
  views::test::WaitForAnimatingLayoutManager(toolbar_container());
  EXPECT_NE(toolbar_button(), nullptr);
  EXPECT_TRUE(toolbar_button()->GetVisible());
  // Verify calling Hide does change the visibility of the button.
  controller()->Hide();
  EXPECT_EQ(toolbar_button(), nullptr);
}

IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       ButtonPressWithNoRecentDownloads) {
  // Pin the downloads button so it is available to press.
  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser()->profile());
  actions_model->UpdatePinnedState(kActionShowDownloads, true);
  views::test::WaitForAnimatingLayoutManager(toolbar_container());
  EXPECT_NE(toolbar_button(), nullptr);
  EXPECT_TRUE(toolbar_button()->GetVisible());
  //   ClickButton(toolbar_button());
  {
    content::LoadStopObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    ClickButton(toolbar_button());
    observer.Wait();
  }
  EXPECT_EQ(GURL(chrome::kChromeUIDownloadsURL),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}
#endif
