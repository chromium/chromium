// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_usage_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "url/gurl.h"

class FileSystemAccessUsageBubbleInteractiveUiTest : public DialogBrowserTest {
 public:
  FileSystemAccessUsageBubbleInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPageActionsMigration,
        {{features::kPageActionsMigrationManagePasswords.name, "true"}});
  }

  ~FileSystemAccessUsageBubbleInteractiveUiTest() override = default;

  FileSystemAccessUsageBubbleInteractiveUiTest(
      const FileSystemAccessUsageBubbleInteractiveUiTest&) = delete;
  FileSystemAccessUsageBubbleInteractiveUiTest& operator=(
      const FileSystemAccessUsageBubbleInteractiveUiTest&) = delete;

  void ShowUi(const std::string& name) override {
    FileSystemAccessUsageBubbleView::Usage usage;
    usage.writable_files.emplace_back(
        FILE_PATH_LITERAL("/foo/bar/Shapes.sketch"));
    FileSystemAccessUsageBubbleView::ShowBubble(
        browser()->tab_strip_model()->GetActiveWebContents(), kTestOrigin,
        std::move(usage));
  }

  void CloseAndVerifyBubble() {
    views::Widget* bubble_widget =
        FileSystemAccessUsageBubbleView::GetBubble()->GetWidget();
    ASSERT_NE(bubble_widget, nullptr);
    views::test::WidgetDestroyedWaiter waiter(bubble_widget);
    bubble_widget->Close();
    waiter.Wait();
    ASSERT_EQ(FileSystemAccessUsageBubbleView::GetBubble(), nullptr);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

 protected:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
};

// Tests that the bubble closes on tab switch and can be reopened by clicking
// the location bar icon.
IN_PROC_BROWSER_TEST_F(FileSystemAccessUsageBubbleInteractiveUiTest,
                       TabSwitchAndReopenBubble) {
  // Show the File System Access bubble and verify it is visible.
  ShowUi("default");
  ASSERT_NE(FileSystemAccessUsageBubbleView::GetBubble(), nullptr);
  content::WebContents* const initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Add a new tab and make sure it does not have the same active web contents.
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);
  ASSERT_NE(initial_tab, browser()->tab_strip_model()->GetActiveWebContents());

  // Switch back to the original tab.
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(initial_tab));
  ASSERT_EQ(initial_tab, browser()->tab_strip_model()->GetActiveWebContents());

  // Invoke the bubble again.
  ShowUi("default");

  // Verify that the bubble is displayed again.
  ASSERT_NE(FileSystemAccessUsageBubbleView::GetBubble(), nullptr);
  EXPECT_TRUE(
      FileSystemAccessUsageBubbleView::GetBubble()->GetWidget()->IsVisible());

  // Close the bubble and verify it's gone.
  CloseAndVerifyBubble();
}

// Tests the scenario where permission is granted show the bubble for file
// system access when icon is clicked and then permission are revoked closing
// the bubble. Then the permission is granted again to make sure the bubble
// shows.
IN_PROC_BROWSER_TEST_F(FileSystemAccessUsageBubbleInteractiveUiTest,
                       BubbleReappearWhenPermissionChanges) {
  // Show the File System Access bubble.
  ShowUi("default");
  ASSERT_NE(FileSystemAccessUsageBubbleView::GetBubble(), nullptr);

  // Close the bubble.
  CloseAndVerifyBubble();

  // Revoke the permission.
  auto* permission_context =
      FileSystemAccessPermissionContextFactory::GetForProfile(
          browser()->profile());
  permission_context->RevokeGrants(kTestOrigin);

  // Invoke the bubble again, which simulates re-granting access.
  ShowUi("default");

  // Make sure bubble is visible after permissions are granted again.
  ASSERT_NE(FileSystemAccessUsageBubbleView::GetBubble(), nullptr);
  EXPECT_TRUE(
      FileSystemAccessUsageBubbleView::GetBubble()->GetWidget()->IsVisible());

  // Close the bubble again to clean up.
  CloseAndVerifyBubble();
}
