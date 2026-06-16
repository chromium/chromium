// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_controller.h"

#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_request.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/modal_dialog_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "url/gurl.h"

namespace {

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS)
// Test dimensions for resizing the browser window.
constexpr int kResizeWindowX = 10;
constexpr int kResizeWindowY = 10;
constexpr int kResizeWindowWidth = 800;
constexpr int kResizeWindowHeight = 600;
#endif
// Calculates the expected client view size by mirroring the constrained
// window position and sizing capping logic dynamically.
gfx::Size GetExpectedClientViewSize(
    views::Widget* widget,
    web_modal::WebContentsModalDialogHost* dialog_host,
    const gfx::Size& preferred_size) {
#if BUILDFLAG(IS_CHROMEOS)
  views::Widget* host_widget =
      views::Widget::GetWidgetForNativeView(dialog_host->GetHostView());
  gfx::Size max_size = host_widget->GetWindowBoundsInScreen().size();

  gfx::Insets insets = widget->non_client_view()->frame_view()->GetInsets();

  gfx::Size expected_widget_size = preferred_size;
  expected_widget_size.Enlarge(insets.width(), insets.height());
  expected_widget_size.SetToMin(max_size);

  gfx::Size expected_client_size = expected_widget_size;
  expected_client_size.Enlarge(-insets.width(), -insets.height());
  return expected_client_size;
#else
  return preferred_size;
#endif
}

// Helper class to wait until a `View` is destroyed/deleting.
class ViewDestroyedWaiter : public views::ViewObserver {
 public:
  explicit ViewDestroyedWaiter(views::View* view) {
    observation_.Observe(view);
  }
  ~ViewDestroyedWaiter() override = default;

  void Wait() { run_loop_.Run(); }

  // `views::ViewObserver`:
  void OnViewIsDeleting(views::View* observed_view) override {
    observation_.Reset();
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

}  // namespace

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
    if (view_waiter_) {
      ResetControllerState();
      view_waiter_->Wait();
      view_waiter_.reset();
    }
    controller_.reset();
    TestWithBrowserView::TearDown();
  }

  void ShowDrivePickerHost() {
    auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
        drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi,
        mojo::PendingRemote<
            drive_picker_host::mojom::DrivePickerResultHandler>());
    controller_->ShowDrivePickerHost(std::move(request));

    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return picker_widget() && picker_view(); }));

    view_waiter_ = std::make_unique<ViewDestroyedWaiter>(picker_view());
  }

  views::Widget* picker_widget() { return controller_->GetWidgetForTesting(); }
  DrivePickerHostView* picker_view() {
    return controller_->GetViewForTesting();
  }
  void ResetControllerState() { controller_->ResetControllerStateForTesting(); }

 protected:
  std::unique_ptr<DrivePickerHostController> controller_;
  std::unique_ptr<ViewDestroyedWaiter> view_waiter_;
};

TEST_F(DrivePickerHostControllerTest, ShowDrivePickerHostRequestsFocus) {
  ShowDrivePickerHost();
  DrivePickerHostView* view = picker_view();
  ASSERT_TRUE(view);

  // Manually update the modal dialog position to trigger layout and sizing
  // in headless environments, so that child views become focusable.
  constrained_window::UpdateWidgetModalDialogPosition(
      picker_widget(), browser_view()->GetWebContentsModalDialogHost());

  // The hosted `WebView` inside the view should have requested focus.
  ASSERT_FALSE(view->children().empty());
  views::View* web_view = view->children().front();
  EXPECT_TRUE(web_view->HasFocus());
}

TEST_F(DrivePickerHostControllerTest, ShowDrivePickerHostCreatesView) {
  ShowDrivePickerHost();
  DrivePickerHostView* view = picker_view();
  ASSERT_TRUE(view);
  EXPECT_EQ(picker_widget(), view->GetWidget());
  EXPECT_EQ(controller_->web_contents(), view->GetWebContents());
}

TEST_F(DrivePickerHostControllerTest, PickerCoversBrowserContents) {
  ShowDrivePickerHost();

  // Manually update the modal dialog position to trigger layout and sizing
  // in headless environments.
  constrained_window::UpdateWidgetModalDialogPosition(
      picker_widget(), browser_view()->GetWebContentsModalDialogHost());

  // The widget should be window-modal.
  EXPECT_EQ(picker_widget()->widget_delegate()->GetModalType(),
            ui::mojom::ModalType::kWindow);

  // The view bounds size should match the exact calculated size.
  gfx::Size expected_size = GetExpectedClientViewSize(
      picker_widget(), browser_view()->GetWebContentsModalDialogHost(),
      picker_view()->GetPreferredSize());
  EXPECT_EQ(picker_view()->bounds().size(), expected_size);
}

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS)
TEST_F(DrivePickerHostControllerTest, PickerResizesWithWindow) {
  ShowDrivePickerHost();

  // Resize the browser window.
  gfx::Rect new_window_bounds(kResizeWindowX, kResizeWindowY,
                              kResizeWindowWidth, kResizeWindowHeight);
  browser_view()->GetWidget()->SetBounds(new_window_bounds);

  // Manually update the modal dialog position to simulate the resize event in
  // tests.
  constrained_window::UpdateWidgetModalDialogPosition(
      picker_widget(), browser_view()->GetWebContentsModalDialogHost());

  // Wait for the positioning and bounds updates to propagate to the child
  // dialog.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    gfx::Size expected_current_size = GetExpectedClientViewSize(
        picker_widget(), browser_view()->GetWebContentsModalDialogHost(),
        picker_view()->GetPreferredSize());
    return picker_view()->bounds().size() == expected_current_size;
  }));
}
#endif

TEST_F(DrivePickerHostControllerTest, ResetControllerStateClearsView) {
  ShowDrivePickerHost();

  ViewDestroyedWaiter waiter(picker_view());

  ResetControllerState();

  waiter.Wait();

  // `ResetControllerState` should have cleared both view and widget.
  EXPECT_FALSE(picker_widget());
  EXPECT_FALSE(picker_view());
}
