// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_controller.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_view.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_request.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/modal_dialog_host.h"
#include "components/web_modal/test_web_contents_modal_dialog_host.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/display/display.h"
#include "ui/display/display_list.h"
#include "ui/display/screen.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/buildflags.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/test_screen.h"
#endif

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

class DrivePickerHostControllerTest : public ChromeViewsTestBase {
 public:
  DrivePickerHostControllerTest() {
    feature_list_.InitAndEnableFeature(omnibox::kOmniboxEverywhere);
  }
  ~DrivePickerHostControllerTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();

    // Ensure the parent window is visible and active for modal dialogs.
    parent_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    parent_widget_->Show();
    parent_widget_->SetBounds(gfx::Rect(0, 0, 1000, 800));

    dialog_host_ = std::make_unique<web_modal::TestWebContentsModalDialogHost>(
        parent_widget_->GetNativeView());
    dialog_host_->set_max_dialog_size(gfx::Size(1000, 800));

    ON_CALL(mock_browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
    ON_CALL(mock_browser_window_interface_,
            GetWebContentsModalDialogHostForWindow())
        .WillByDefault(testing::Return(dialog_host_.get()));

    controller_ = std::make_unique<DrivePickerHostController>(
        profile(), &mock_browser_window_interface_);
  }

  void TearDown() override {
    if (view_waiter_) {
      ResetControllerState();
      view_waiter_->Wait();
      view_waiter_.reset();
    }
    controller_.reset();
    dialog_host_.reset();
    if (parent_widget_) {
      parent_widget_->CloseNow();
      parent_widget_.reset();
    }
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void ShowDrivePickerHost(
      DrivePickerHostView::Style style = DrivePickerHostView::Style::kDefault) {
    auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
        drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi,
        mojo::PendingRemote<
            drive_picker_host::mojom::DrivePickerResultHandler>());
    request->set_style(style);
    controller_->ShowDrivePickerHost(std::move(request));

    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return picker_widget() && picker_view(); }));

    view_waiter_ = std::make_unique<ViewDestroyedWaiter>(picker_view());
  }

  TestingProfile* profile() { return profile_.get(); }
  views::Widget* parent_widget() { return parent_widget_.get(); }
  web_modal::TestWebContentsModalDialogHost* dialog_host() {
    return dialog_host_.get();
  }
  MockBrowserWindowInterface* browser_window_interface() {
    return &mock_browser_window_interface_;
  }

  views::Widget* picker_widget() { return controller_->GetWidgetForTesting(); }
  DrivePickerHostView* picker_view() {
    return controller_->GetViewForTesting();
  }
  void ResetControllerState() { controller_->ResetControllerStateForTesting(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  MockBrowserWindowInterface mock_browser_window_interface_;
  std::unique_ptr<views::Widget> parent_widget_;
  std::unique_ptr<web_modal::TestWebContentsModalDialogHost> dialog_host_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<DrivePickerHostController> controller_;
  std::unique_ptr<ViewDestroyedWaiter> view_waiter_;
};

TEST_F(DrivePickerHostControllerTest, ShowDrivePickerHostRequestsFocus) {
  ShowDrivePickerHost();
  DrivePickerHostView* view = picker_view();
  ASSERT_TRUE(view);

  // Manually update the modal dialog position to trigger layout and sizing
  // in headless environments, so that child views become focusable.
  constrained_window::UpdateWidgetModalDialogPosition(picker_widget(),
                                                      dialog_host());

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

TEST_F(DrivePickerHostControllerTest, FrameViewHasNoShadowAndZeroInsets) {
  ShowDrivePickerHost();
  ASSERT_TRUE(picker_widget());
  auto* frame_view = picker_widget()
                         ->widget_delegate()
                         ->AsDialogDelegate()
                         ->GetBubbleFrameView();
  ASSERT_TRUE(frame_view);
  EXPECT_EQ(frame_view->GetInsets(), gfx::Insets());
  auto* bubble_border = frame_view->bubble_border();
  ASSERT_TRUE(bubble_border);
  EXPECT_EQ(bubble_border->shadow(), views::BubbleBorder::NO_SHADOW);
}

TEST_F(DrivePickerHostControllerTest, PickerCoversBrowserContents) {
  ShowDrivePickerHost();

  // Manually update the modal dialog position to trigger layout and sizing
  // in headless environments.
  constrained_window::UpdateWidgetModalDialogPosition(picker_widget(),
                                                      dialog_host());

  // The widget should be window-modal.
  EXPECT_EQ(picker_widget()->widget_delegate()->GetModalType(),
            ui::mojom::ModalType::kWindow);

  // The view bounds size should match the exact calculated size.
  gfx::Size expected_size = GetExpectedClientViewSize(
      picker_widget(), dialog_host(), picker_view()->GetPreferredSize());
  EXPECT_EQ(picker_view()->bounds().size(), expected_size);
}

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS)
TEST_F(DrivePickerHostControllerTest, PickerResizesWithWindow) {
  ShowDrivePickerHost();

  // Resize the browser window.
  gfx::Rect new_window_bounds(kResizeWindowX, kResizeWindowY,
                              kResizeWindowWidth, kResizeWindowHeight);
  parent_widget()->SetBounds(new_window_bounds);

  // Manually update the modal dialog position to simulate the resize event in
  // tests.
  constrained_window::UpdateWidgetModalDialogPosition(picker_widget(),
                                                      dialog_host());

  // Wait for the positioning and bounds updates to propagate to the child
  // dialog.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    gfx::Size expected_current_size = GetExpectedClientViewSize(
        picker_widget(), dialog_host(), picker_view()->GetPreferredSize());
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

// Verifies Drive Picker host lifecycle when anchored to a widget.
TEST_F(DrivePickerHostControllerTest, ShowDrivePickerHostWithAnchorWidget) {
  // Create an anchor widget to simulate Loomnibox
  views::Widget anchor_widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.context = parent_widget()->GetNativeWindow();
  anchor_widget.Init(std::move(params));
  anchor_widget.Show();

  // Instantiate controller with null BWI, but passing the anchor widget.
  auto anchor_controller = std::make_unique<DrivePickerHostController>(
      profile(), nullptr, &anchor_widget);

  auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
      drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi,
      mojo::PendingRemote<
          drive_picker_host::mojom::DrivePickerResultHandler>());
  anchor_controller->ShowDrivePickerHost(std::move(request));

  views::Widget* picker_widget = anchor_controller->GetWidgetForTesting();
  ASSERT_TRUE(picker_widget);
  EXPECT_TRUE(picker_widget->IsVisible());
  // Non-Glic anchoring keeps the default (non-desktop) widget selection.
  EXPECT_FALSE(picker_widget->widget_delegate()->use_desktop_widget_override());

  // Closing the anchor widget (Loomnibox) should automatically reset the
  // controller and close the picker.
  anchor_widget.CloseNow();
  EXPECT_FALSE(anchor_controller->GetWidgetForTesting());
}

// Verifies that when `omnibox::kOmniboxEverywhere` is enabled and
// `features::kGlicDrivePicker` is disabled, a request with `Style::kGlic`
// anchors to the browser window's `dialog_host` with standard non-Glic sizing
// and widget behavior.
TEST_F(DrivePickerHostControllerTest,
       GlicFlagDisabledWithOmniboxEverywhereUsesNonGlicBehavior) {
  base::test::ScopedFeatureList disable_glic_drive_picker;
  disable_glic_drive_picker.InitAndDisableFeature(features::kGlicDrivePicker);

  views::Widget anchor_widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.context = parent_widget()->GetNativeWindow();
  anchor_widget.Init(std::move(params));
  anchor_widget.Show();

  auto anchor_controller = std::make_unique<DrivePickerHostController>(
      profile(), browser_window_interface(), &anchor_widget);

  auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
      drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi,
      mojo::PendingRemote<
          drive_picker_host::mojom::DrivePickerResultHandler>());
  request->set_style(drive_picker_host::DrivePickerHostRequest::Style::kGlic);
  anchor_controller->ShowDrivePickerHost(std::move(request));

  views::Widget* picker_widget = anchor_controller->GetWidgetForTesting();
  ASSERT_TRUE(picker_widget);
  EXPECT_EQ(picker_widget->parent(), parent_widget());
  EXPECT_FALSE(picker_widget->widget_delegate()->use_desktop_widget_override());
  EXPECT_EQ(anchor_controller->GetViewForTesting()->GetPreferredSize(),
            DrivePickerHostView::kPickerDialogSize);

  ViewDestroyedWaiter waiter(anchor_controller->GetViewForTesting());
  anchor_controller->ResetControllerStateForTesting();
  waiter.Wait();
  anchor_widget.CloseNow();
}

// Verifies that when only `features::kGlicDrivePicker` is enabled (and
// `omnibox::kOmniboxEverywhere` is disabled), a Glic request with no normal
// browser window anchors to `anchor_widget` as a top-level desktop widget.
TEST_F(DrivePickerHostControllerTest,
       GlicPickerAnchorsToFloatyWhenNoBrowserWindow) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({features::kGlicDrivePicker},
                            {omnibox::kOmniboxEverywhere});

  views::Widget floaty_widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.context = parent_widget()->GetNativeWindow();
  floaty_widget.Init(std::move(params));
  floaty_widget.Show();

  auto glic_controller = std::make_unique<DrivePickerHostController>(
      profile(), /*browser_window_interface=*/nullptr, &floaty_widget);

  auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
      drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi,
      mojo::PendingRemote<
          drive_picker_host::mojom::DrivePickerResultHandler>());
  request->set_style(drive_picker_host::DrivePickerHostRequest::Style::kGlic);
  glic_controller->ShowDrivePickerHost(std::move(request));

  views::Widget* picker_widget = glic_controller->GetWidgetForTesting();
  ASSERT_TRUE(picker_widget);
  EXPECT_TRUE(picker_widget->IsVisible());
  // The picker must be a top-level desktop widget rather than a child of the
  // (much smaller) floaty window, otherwise it would be clipped to its bounds.
  EXPECT_TRUE(picker_widget->widget_delegate()->use_desktop_widget_override());
  EXPECT_EQ(glic_controller->GetViewForTesting()->GetPreferredSize(),
            DrivePickerHostView::kGlicPickerDialogSize);

  ViewDestroyedWaiter waiter(glic_controller->GetViewForTesting());
  floaty_widget.CloseNow();
  waiter.Wait();
  EXPECT_FALSE(glic_controller->GetWidgetForTesting());
}

// Verifies that enabling `features::kGlicDrivePicker` alone (with
// `omnibox::kOmniboxEverywhere` disabled) does not allow non-Glic requests to
// open without a browser window.
TEST_F(DrivePickerHostControllerTest,
       NonGlicPickerRejectedWithoutBrowserWindowWhenOnlyGlicFeatureEnabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({features::kGlicDrivePicker},
                            {omnibox::kOmniboxEverywhere});

  views::Widget anchor_widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.context = parent_widget()->GetNativeWindow();
  anchor_widget.Init(std::move(params));
  anchor_widget.Show();

  auto anchor_controller = std::make_unique<DrivePickerHostController>(
      profile(), /*browser_window_interface=*/nullptr, &anchor_widget);

  auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
      drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi,
      mojo::PendingRemote<
          drive_picker_host::mojom::DrivePickerResultHandler>());
  anchor_controller->ShowDrivePickerHost(std::move(request));

  EXPECT_FALSE(anchor_controller->GetWidgetForTesting());
  anchor_widget.CloseNow();
}

// Verifies that when both a browser window (`dialog_host`) and `anchor_widget`
// are present (matching how `GlicWebClientHandler` always passes both), a Glic
// picker anchors to `dialog_host` rather than using floaty desktop-widget mode,
// even with `omnibox::kOmniboxEverywhere` enabled (the base fixture default).
TEST_F(DrivePickerHostControllerTest,
       GlicPickerPrefersBrowserDialogHostOverAnchorWidget) {
  base::test::ScopedFeatureList enable_glic_drive_picker(
      features::kGlicDrivePicker);

  views::Widget floaty_widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.context = parent_widget()->GetNativeWindow();
  floaty_widget.Init(std::move(params));
  floaty_widget.Show();

  auto glic_controller = std::make_unique<DrivePickerHostController>(
      profile(), browser_window_interface(), &floaty_widget);

  auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
      drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi,
      mojo::PendingRemote<
          drive_picker_host::mojom::DrivePickerResultHandler>());
  request->set_style(drive_picker_host::DrivePickerHostRequest::Style::kGlic);
  glic_controller->ShowDrivePickerHost(std::move(request));

  views::Widget* picker_widget = glic_controller->GetWidgetForTesting();
  ASSERT_TRUE(picker_widget);
  EXPECT_TRUE(picker_widget->IsVisible());
  EXPECT_EQ(picker_widget->parent(), parent_widget());
  EXPECT_FALSE(picker_widget->widget_delegate()->use_desktop_widget_override());
  EXPECT_EQ(glic_controller->GetViewForTesting()->GetPreferredSize(),
            DrivePickerHostView::kGlicPickerDialogSize);

  // Closing the floaty while the picker is modal to the browser window must
  // leave the picker open without leaving a dangling `anchor_widget_`.
  floaty_widget.CloseNow();
  EXPECT_TRUE(picker_widget->IsVisible());

  ViewDestroyedWaiter waiter(glic_controller->GetViewForTesting());
  glic_controller->ResetControllerStateForTesting();
  waiter.Wait();
}

// Verifies that a Glic request with no browser window is rejected when
// `features::kGlicDrivePicker` is disabled, even with
// `omnibox::kOmniboxEverywhere` enabled (the base fixture default).
TEST_F(DrivePickerHostControllerTest,
       GlicPickerFloatyAnchoringRequiresGlicDrivePickerFeature) {
  base::test::ScopedFeatureList disable_glic_drive_picker;
  disable_glic_drive_picker.InitAndDisableFeature(features::kGlicDrivePicker);

  views::Widget floaty_widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.context = parent_widget()->GetNativeWindow();
  floaty_widget.Init(std::move(params));
  floaty_widget.Show();

  auto glic_controller = std::make_unique<DrivePickerHostController>(
      profile(), /*browser_window_interface=*/nullptr, &floaty_widget);

  auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
      drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi,
      mojo::PendingRemote<
          drive_picker_host::mojom::DrivePickerResultHandler>());
  request->set_style(drive_picker_host::DrivePickerHostRequest::Style::kGlic);
  glic_controller->ShowDrivePickerHost(std::move(request));

  EXPECT_FALSE(glic_controller->GetWidgetForTesting());
  floaty_widget.CloseNow();
}

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
// Verifies that when a top-level desktop picker widget (`1050x650`) is anchored
// to a smaller detached floaty near the edges of the display work area, the
// picker is clamped inside the work area on open, when the floaty moves
// (`OnWidgetBoundsChanged`), and across view size transitions.
TEST_F(DrivePickerHostControllerTest,
       GlicPickerAnchoredToEdgeFloatyStaysWithinDisplayWorkArea) {
  base::test::ScopedFeatureList enable_glic_drive_picker(
      features::kGlicDrivePicker);

  // In production, `ChromeViewsDelegate` selects `DesktopNativeWidgetAura` when
  // `use_desktop_widget_override()` is set. Mirror that in `TestViewsDelegate`.
  test_views_delegate()->set_use_desktop_native_widgets(true);

  // `AuraTestHelper` initializes an 800x600 `aura::TestScreen`, which is
  // smaller than `kGlicPickerDialogSize` (1050x650). Update `TestScreen`'s
  // primary display directly to 1920x1080 (resizing the root `WindowTreeHost`
  // instead would go through `WinWindow`'s `WS_OVERLAPPEDWINDOW` on Windows and
  // get clamped to `SM_CXMAXTRACK` on 1024x768 Swarming bots).
  aura::TestScreen* test_screen =
      aura::test::AuraTestHelper::GetInstance()->GetTestScreen();
  display::Display display = test_screen->GetPrimaryDisplay();
  display.SetScaleAndBounds(1.0f, gfx::Rect(0, 0, 1920, 1080));
  test_screen->display_list().UpdateDisplay(display);

  const gfx::Rect work_area =
      display::Screen::Get()->GetPrimaryDisplay().work_area();
  ASSERT_GE(work_area.width(),
            DrivePickerHostView::kGlicPickerDialogSize.width())
      << work_area.ToString();
  ASSERT_GE(work_area.height(),
            DrivePickerHostView::kGlicPickerDialogSize.height())
      << work_area.ToString();

  views::Widget floaty_widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  constexpr gfx::Size kFloatySize{400, 400};
  params.bounds = gfx::Rect(work_area.right() - kFloatySize.width(),
                            work_area.bottom() - kFloatySize.height(),
                            kFloatySize.width(), kFloatySize.height());
  floaty_widget.Init(std::move(params));
  floaty_widget.Show();
  base::RunLoop().RunUntilIdle();

  auto glic_controller = std::make_unique<DrivePickerHostController>(
      profile(), /*browser_window_interface=*/nullptr, &floaty_widget);

  auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
      drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi,
      mojo::PendingRemote<
          drive_picker_host::mojom::DrivePickerResultHandler>());
  request->set_style(drive_picker_host::DrivePickerHostRequest::Style::kGlic);
  glic_controller->ShowDrivePickerHost(std::move(request));
  base::RunLoop().RunUntilIdle();

  views::Widget* picker_widget = glic_controller->GetWidgetForTesting();
  DrivePickerHostView* picker_view = glic_controller->GetViewForTesting();
  ASSERT_TRUE(picker_widget);
  ASSERT_TRUE(picker_view);

  EXPECT_TRUE(work_area.Contains(picker_widget->GetWindowBoundsInScreen()))
      << "work_area=" << work_area.ToString()
      << " picker=" << picker_widget->GetWindowBoundsInScreen().ToString();

  // Moving the floaty to the top-left corner triggers `OnWidgetBoundsChanged`
  // and must keep the picker inside `work_area`.
  floaty_widget.SetBounds(gfx::Rect(work_area.origin(), kFloatySize));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(work_area.Contains(picker_widget->GetWindowBoundsInScreen()))
      << "work_area=" << work_area.ToString()
      << " picker=" << picker_widget->GetWindowBoundsInScreen().ToString();

  // View UI transitions (`CenterInWidget`) near the bottom-right corner must
  // also stay clamped within `work_area`.
  floaty_widget.SetBounds(gfx::Rect(work_area.right() - kFloatySize.width(),
                                    work_area.bottom() - kFloatySize.height(),
                                    kFloatySize.width(), kFloatySize.height()));
  picker_view->OnTransitionToError();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(work_area.Contains(picker_widget->GetWindowBoundsInScreen()))
      << "work_area=" << work_area.ToString()
      << " picker=" << picker_widget->GetWindowBoundsInScreen().ToString();

  picker_view->OnTransitionToConsent();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(work_area.Contains(picker_widget->GetWindowBoundsInScreen()))
      << "work_area=" << work_area.ToString()
      << " picker=" << picker_widget->GetWindowBoundsInScreen().ToString();

  picker_view->OnTransitionToPicker();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(work_area.Contains(picker_widget->GetWindowBoundsInScreen()))
      << "work_area=" << work_area.ToString()
      << " picker=" << picker_widget->GetWindowBoundsInScreen().ToString();

  ViewDestroyedWaiter waiter(picker_view);
  floaty_widget.CloseNow();
  waiter.Wait();
}
#endif  // BUILDFLAG(ENABLE_DESKTOP_AURA)
