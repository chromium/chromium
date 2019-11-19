// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/events/event_utils.h"

typedef extensions::ExtensionApiTest BookmarkOverrideTest;

namespace {
// Bookmark this page keybinding.
#if defined(OS_MACOSX)
const char kBookmarkKeybinding[] = "Command+D";
#else
const char kBookmarkKeybinding[] = "Ctrl+D";
#endif  // defined(OS_MACOSX)
}

// Test that clicking the star brings up the bookmark UI, if no extension
// requests to override ctrl-D and the user has assigned it to an extension.
// Flaky on all platforms: https://crbug.com/448956.
IN_PROC_BROWSER_TEST_F(BookmarkOverrideTest, DISABLED_NonOverrideStarClick) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/basics")) << message_;
  const extensions::Extension* extension = GetSingleLoadedExtension();

  extensions::CommandService* command_service =
      extensions::CommandService::Get(browser()->profile());

  // Simulate the user setting the keybinding to Ctrl+D.
  command_service->UpdateKeybindingPrefs(
      extension->id(), extensions::manifest_values::kBrowserActionCommandEvent,
      kBookmarkKeybinding);

  // Check that the BookmarkBubbleView is shown when clicking on the star.
  BrowserView* browser_view = reinterpret_cast<BrowserView*>(
      browser()->window());
  views::View* star_view =
      browser_view->toolbar_button_provider()->GetPageActionIconView(
          PageActionIconType::kBookmarkStar);

  ui::MouseEvent pressed_event(
      ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent released_event(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);

  // Verify that clicking once shows the bookmark bubble.
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
  star_view->OnMousePressed(pressed_event);
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
  star_view->OnMouseReleased(released_event);
  EXPECT_TRUE(BookmarkBubbleView::bookmark_bubble());
}

// Test that invoking the IDC_BOOKMARK_THIS_TAB command (as done by the app
// menu) brings up the bookmark UI, if no extension requests to override ctrl-D
// and the user has assigned it to an extension. Flaky on all platforms:
// https://crbug.com/448956.
IN_PROC_BROWSER_TEST_F(BookmarkOverrideTest, DISABLED_NonOverrideBookmarkTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/basics")) << message_;
  const extensions::Extension* extension = GetSingleLoadedExtension();

  extensions::CommandService* command_service =
      extensions::CommandService::Get(browser()->profile());

  // Simulate the user setting the keybinding to Ctrl+D.
  command_service->UpdateKeybindingPrefs(
      extension->id(), extensions::manifest_values::kBrowserActionCommandEvent,
      kBookmarkKeybinding);

  // Check that the BookmarkBubbleView is shown when executing
  // IDC_BOOKMARK_THIS_TAB.
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
  chrome::ExecuteCommand(browser(), IDC_BOOKMARK_THIS_TAB);
  EXPECT_TRUE(BookmarkBubbleView::bookmark_bubble());
}
