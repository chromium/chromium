// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/mahi/mahi_menu_controller.h"

#include <memory>
#include <string>

#include "chrome/browser/chromeos/mahi/test/fake_mahi_web_contents_manager.h"
#include "chrome/browser/chromeos/mahi/test/scoped_mahi_web_contents_manager_for_testing.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/gfx/geometry/rect.h"

namespace chromeos::mahi {

class MahiMenuControllerTest : public ChromeViewsTestBase {
 public:
  MahiMenuControllerTest() {
    menu_controller_ = std::make_unique<MahiMenuController>();

    scoped_mahi_web_contents_manager_ =
        std::make_unique<::mahi::ScopedMahiWebContentsManagerForTesting>(
            &fake_mahi_web_contents_manager_);
    // Sets the focused page's distillability to true so that it does not block
    // the menu widget's display.
    ChangePageDistillability(true);
  }

  MahiMenuControllerTest(const MahiMenuControllerTest&) = delete;
  MahiMenuControllerTest& operator=(const MahiMenuControllerTest&) = delete;

  ~MahiMenuControllerTest() override = default;

  MahiMenuController* menu_controller() { return menu_controller_.get(); }

  void ChangePageDistillability(bool value) {
    fake_mahi_web_contents_manager_.set_focused_web_content_is_distillable(
        value);
  }

 private:
  std::unique_ptr<MahiMenuController> menu_controller_;

  ::mahi::FakeMahiWebContentsManager fake_mahi_web_contents_manager_;
  std::unique_ptr<::mahi::ScopedMahiWebContentsManagerForTesting>
      scoped_mahi_web_contents_manager_;
};

TEST_F(MahiMenuControllerTest, Widget) {
  EXPECT_FALSE(menu_controller()->menu_widget_for_test());

  // Menu widget should show when text is displayed.
  menu_controller()->OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                     /*selected_text=*/"",
                                     /*surrounding_text=*/"");

  EXPECT_TRUE(menu_controller()->menu_widget_for_test());

  // Menu widget should hide when dismissed.
  menu_controller()->OnDismiss(/*is_other_command_executed=*/false);
  EXPECT_FALSE(menu_controller()->menu_widget_for_test());

  // If page is not distillable, then menu widget should not be triggered.
  ChangePageDistillability(false);
  menu_controller()->OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                     /*selected_text=*/"",
                                     /*surrounding_text=*/"");

  EXPECT_FALSE(menu_controller()->menu_widget_for_test());
}

TEST_F(MahiMenuControllerTest, BoundsChanged) {
  EXPECT_FALSE(menu_controller()->menu_widget_for_test());

  gfx::Rect anchor_bounds = gfx::Rect(50, 50, 25, 100);
  menu_controller()->OnTextAvailable(anchor_bounds,
                                     /*selected_text=*/"",
                                     /*surrounding_text=*/"");
  auto* widget = menu_controller()->menu_widget_for_test();
  EXPECT_TRUE(widget);

  EXPECT_EQ(editor_menu::GetEditorMenuBounds(anchor_bounds,
                                             widget->GetContentsView()),
            widget->GetRestoredBounds());

  anchor_bounds = gfx::Rect(0, 50, 55, 80);

  // Widget should change bounds accordingly.
  menu_controller()->OnAnchorBoundsChanged(anchor_bounds);
  EXPECT_EQ(editor_menu::GetEditorMenuBounds(anchor_bounds,
                                             widget->GetContentsView()),
            widget->GetRestoredBounds());
}

}  // namespace chromeos::mahi
