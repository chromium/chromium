// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/mahi/mahi_menu_controller.h"

#include <memory>
#include <string>

#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/gfx/geometry/rect.h"

namespace chromeos::mahi {

class MahiMenuControllerTest : public ChromeViewsTestBase {
 public:
  MahiMenuControllerTest() {
    menu_controller_ = std::make_unique<MahiMenuController>();
  }

  MahiMenuControllerTest(const MahiMenuControllerTest&) = delete;
  MahiMenuControllerTest& operator=(const MahiMenuControllerTest&) = delete;

  ~MahiMenuControllerTest() override = default;

  MahiMenuController* menu_controller() { return menu_controller_.get(); }

 private:
  std::unique_ptr<MahiMenuController> menu_controller_;
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
}

}  // namespace chromeos::mahi
