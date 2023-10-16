// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/views/compose/compose_dialog_controller.h"

#include <memory>
#include "base/test/mock_callback.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget_observer.h"

class ComposeDialogControllerTest : public ChromeViewsTestBase {
 public:
  ComposeDialogControllerTest() = default;

 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // Create an anchor for testing the dialog.
    anchor_widget_ = CreateTestWidget(views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Show();
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);

    std::unique_ptr<ComposeDialogController> controller(
        new ComposeDialogController(web_contents_.get()));
    controller_ = controller.get();
    web_contents_->SetUserData(ComposeDialogController::UserDataKey(),
                               std::move(controller));
  }

  void TearDown() override {
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  TestingProfile profile_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<ComposeDialogController> controller_;
};

TEST_F(ComposeDialogControllerTest, ShowComposeDialog) {
  EXPECT_THAT(controller_->GetComposeDialog(), testing::IsNull());

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey(),
                                       "ComposeDialogView");
  gfx::RectF bounds = gfx::RectF();
  controller_->ShowComposeDialog(anchor_widget_->GetContentsView(), bounds);

  waiter.WaitIfNeededAndGet();
  EXPECT_THAT(controller_->GetComposeDialog(), testing::NotNull());

  controller_->CloseDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(controller_->GetComposeDialog(), testing::IsNull());
}

TEST_F(ComposeDialogControllerTest, ComputeCenteredBounds) {
  const gfx::Size input_dialog_size(200, 100);
  const gfx::RectF element_bounds(0, 0, 400, 200);
  const gfx::Rect expected_dialog_bounds(100, 50, 200, 100);

  gfx::Rect computed_dialog_bounds =
      controller_->ComputeCenteredDialogBoundsInScreen(input_dialog_size,
                                                       element_bounds);
  EXPECT_EQ(expected_dialog_bounds, computed_dialog_bounds);
}
