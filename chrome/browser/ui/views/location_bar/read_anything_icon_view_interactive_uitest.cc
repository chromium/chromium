// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/read_anything_icon_view.h"

#include "chrome/browser/ui/side_panel/read_anything/read_anything_side_panel_controller_utils.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/events/event_utils.h"

namespace {

class ReadAnythingIconViewTest : public InProcessBrowserTest {
 public:
  ReadAnythingIconViewTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnything, features::kReadAnythingOmniboxIcon}, {});
  }

  ReadAnythingIconViewTest(const ReadAnythingIconViewTest&) = delete;
  ReadAnythingIconViewTest& operator=(const ReadAnythingIconViewTest&) = delete;

  ~ReadAnythingIconViewTest() override = default;

  PageActionIconView* GetReadAnythingOmniboxIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetPageActionIconView(PageActionIconType::kReadAnything);
  }

  void ClickReadAnythingOmniboxIcon(PageActionIconView* icon) {
    ui::MouseEvent pressed_event(
        ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    ui::MouseEvent released_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON);

    static_cast<views::View*>(icon)->OnMousePressed(pressed_event);
    static_cast<views::View*>(icon)->OnMouseReleased(released_event);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Clicking the icon opens reading mode in the side panel.
IN_PROC_BROWSER_TEST_F(ReadAnythingIconViewTest, OpensReadingModeOnClick) {
  PageActionIconView* icon = GetReadAnythingOmniboxIcon();
  EXPECT_FALSE(IsReadAnythingEntryShowing(browser()));
  ClickReadAnythingOmniboxIcon(icon);
  EXPECT_TRUE(IsReadAnythingEntryShowing(browser()));
}

}  // namespace
