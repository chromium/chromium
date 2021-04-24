// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/resize_toggle_menu.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace arc {

class ResizeToggleMenuTest : public views::ViewsTestBase {
 public:
  // Overridden from test::Test.
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::TYPE_WINDOW);
    resize_toggle_menu_ = std::make_unique<ResizeToggleMenu>(
        widget_.get(), /*pref_delegate=*/nullptr);
  }

  bool IsMenuRunning() {
    return resize_toggle_menu_->menu_runner_->IsRunning();
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ResizeToggleMenu> resize_toggle_menu_;
};

TEST_F(ResizeToggleMenuTest, ConstructDestruct) {
  EXPECT_TRUE(IsMenuRunning());
}

}  // namespace arc
