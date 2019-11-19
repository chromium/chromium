// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/star_view.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event_utils.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"

#if defined(OS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

namespace {

class StarViewTest : public extensions::ExtensionBrowserTest {
 public:
  StarViewTest()
      // In order to let a vanilla extension override the bookmark star, we have
      // to enable the switch.
      : enable_override_(
            extensions::FeatureSwitch::enable_override_bookmarks_ui(),
            true) {}
  ~StarViewTest() override = default;

  PageActionIconView* GetStarIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetPageActionIconView(PageActionIconType::kBookmarkStar);
  }

 private:
  extensions::FeatureSwitch::ScopedOverride enable_override_;

  DISALLOW_COPY_AND_ASSIGN(StarViewTest);
};

// Verify that clicking the bookmark star a second time hides the bookmark
// bubble.
IN_PROC_BROWSER_TEST_F(StarViewTest, HideOnSecondClick) {
  views::View* star_icon = GetStarIcon();

  ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent released_event(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);

  // Verify that clicking once shows the bookmark bubble.
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
  star_icon->OnMousePressed(pressed_event);
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
  star_icon->OnMouseReleased(released_event);
  EXPECT_TRUE(BookmarkBubbleView::bookmark_bubble());

  // Verify that clicking again doesn't reshow it.
  star_icon->OnMousePressed(pressed_event);
  // Hide the bubble manually. In the browser this would normally happen during
  // the event processing.
  BookmarkBubbleView::Hide();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
  star_icon->OnMouseReleased(released_event);
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
}

IN_PROC_BROWSER_TEST_F(StarViewTest, InkDropHighlighted) {
  PageActionIconView* star_icon = GetStarIcon();
  views::test::InkDropHostViewTestApi ink_drop_test_api(star_icon);

  if (ink_drop_test_api.HasInkDrop()) {
    GURL url("http://test.com");
    bookmarks::BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    bookmarks::AddIfNotBookmarked(model, url, /*title=*/base::string16());
    browser()->window()->ShowBookmarkBubble(url, false);
    EXPECT_EQ(ink_drop_test_api.GetInkDrop()->GetTargetInkDropState(),
              views::InkDropState::ACTIVATED);
  }
}

// Test that installing an extension that overrides the bookmark star
// successfully hides the star.
IN_PROC_BROWSER_TEST_F(StarViewTest, ExtensionCanOverrideBookmarkStar) {
  // By default, we should show the star.
  EXPECT_TRUE(GetStarIcon()->GetVisible());

  // Create and install an extension that overrides the bookmark star.
  extensions::DictionaryBuilder chrome_ui_overrides;
  chrome_ui_overrides.Set(
      "bookmarks_ui",
      extensions::DictionaryBuilder().Set("remove_button", true).Build());
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetManifest(
              extensions::DictionaryBuilder()
                  .Set("name", "overrides star")
                  .Set("manifest_version", 2)
                  .Set("version", "0.1")
                  .Set("description", "override the star")
                  .Set("chrome_ui_overrides", chrome_ui_overrides.Build())
                  .Build())
          .Build();
  extension_service()->AddExtension(extension.get());

  // The star should now be hidden.
  EXPECT_FALSE(GetStarIcon()->GetVisible());
}

}  // namespace
