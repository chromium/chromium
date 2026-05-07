// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_controller.h"

#include "base/run_loop.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/test/base/testing_profile.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

class DrivePickerHostControllerTest : public TestWithBrowserView {
 public:
  DrivePickerHostControllerTest() = default;
  ~DrivePickerHostControllerTest() override = default;

  void SetUp() override {
    TestWithBrowserView::SetUp();
    // Ensure the browser window is visible and active, which is often required
    // for modal dialogs to be correctly parented and displayed in tests.
    browser_view()->GetWidget()->Show();

    AddTab(browser(), GURL("about:blank"));
    controller_ = std::make_unique<DrivePickerHostController>(browser());
  }

  void TearDown() override {
    controller_.reset();
    TestWithBrowserView::TearDown();
  }

 protected:
  std::unique_ptr<DrivePickerHostController> controller_;
};

TEST_F(DrivePickerHostControllerTest, ShowDrivePickerHostCreatesView) {
  mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
      remote;
  controller_->ShowDrivePickerHost(std::move(remote));

  // Process any pending tasks (like widget showing or initial WebUI setup).
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller_->widget_);
  DrivePickerHostView* view = views::AsViewClass<DrivePickerHostView>(
      controller_->widget_->widget_delegate()->GetContentsView());
  ASSERT_TRUE(view);
  EXPECT_EQ(controller_->web_contents(), view->GetWebContents());
}

TEST_F(DrivePickerHostControllerTest, WidgetCloseResetsState) {
  mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
      remote;
  controller_->ShowDrivePickerHost(std::move(remote));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller_->widget_);
  views::Widget* widget_ptr = controller_->widget_.get();

  // Close the widget, which should trigger ResetControllerState via the
  // MakeCloseSynchronous callback.
  widget_ptr->Close();

  // ResetControllerState should have been called, clearing the widget.
  EXPECT_FALSE(controller_->widget_);
  EXPECT_FALSE(controller_->is_picker_document_loaded_);
}
