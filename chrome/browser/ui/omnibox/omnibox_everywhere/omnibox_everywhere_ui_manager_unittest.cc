// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/display/test/test_screen.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

class TestWebUIContentsWrapper : public WebUIContentsWrapper {
 public:
  explicit TestWebUIContentsWrapper(Profile* profile)
      : WebUIContentsWrapper(GURL(""), profile, 0, true, true, true, "Test") {}
  ~TestWebUIContentsWrapper() override = default;

  void ReloadWebContents() override {}

  base::WeakPtr<WebUIContentsWrapper> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestWebUIContentsWrapper> weak_ptr_factory_{this};
};

}  // namespace

class OmniboxEverywhereUIManagerTest : public ChromeViewsTestBase {
 public:
  OmniboxEverywhereUIManagerTest() = default;
  ~OmniboxEverywhereUIManagerTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(omnibox::kOmniboxEverywhere);
    set_native_widget_type(NativeWidgetType::kDesktop);
    ChromeViewsTestBase::SetUp();
  }

  std::unique_ptr<omnibox_everywhere::OmniboxEverywhereUIManager>
  CreateUIManager() {
    return std::make_unique<omnibox_everywhere::OmniboxEverywhereUIManager>(
        base::BindRepeating(
            [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
              return std::make_unique<TestWebUIContentsWrapper>(profile);
            }));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
};

TEST_F(OmniboxEverywhereUIManagerTest, ShowAndCloseWidget) {
  auto ui_manager = CreateUIManager();

  // Initially, no widget should exist.
  EXPECT_FALSE(ui_manager->widget());

  // Showing the UI manager for a profile should instantiate and display a
  // widget.
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Closing the UI manager should trigger widget closure.
  views::test::WidgetDestroyedWaiter waiter(widget);
  ui_manager->Close();
  waiter.Wait();

  EXPECT_FALSE(ui_manager->widget());
}

TEST_F(OmniboxEverywhereUIManagerTest, ShowWhileWidgetIsClosing) {
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* first_widget = ui_manager->widget();
  ASSERT_TRUE(first_widget);

  // Close the widget.
  ui_manager->Close();

  // Showing it again immediately should successfully clean up the closing
  // widget and create a new visible widget.
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* second_widget = ui_manager->widget();
  ASSERT_TRUE(second_widget);
  EXPECT_TRUE(second_widget->IsVisible());

  // Clean up.
  ui_manager->Close();
  EXPECT_FALSE(ui_manager->widget());
}

TEST_F(OmniboxEverywhereUIManagerTest, FileChooserStateTracking) {
  auto ui_manager = CreateUIManager();
  EXPECT_FALSE(ui_manager->is_file_chooser_open_for_testing());

  ui_manager->OnFileChooserOpened();
  EXPECT_TRUE(ui_manager->is_file_chooser_open_for_testing());

  ui_manager->OnFileChooserClosed();
  EXPECT_FALSE(ui_manager->is_file_chooser_open_for_testing());
}

TEST_F(OmniboxEverywhereUIManagerTest, DismissOnDeactivation) {
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Simulating deactivation (active = false) should close the widget.
  views::test::WidgetDestroyedWaiter waiter(widget);
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  waiter.Wait();
  EXPECT_FALSE(ui_manager->widget());
}

TEST_F(OmniboxEverywhereUIManagerTest, DismissBypassedDuringFileChooser) {
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Mark file chooser as open.
  ui_manager->OnFileChooserOpened();
  EXPECT_TRUE(ui_manager->is_file_chooser_open_for_testing());

  // Simulating deactivation while a file chooser is open should NOT close the
  // widget.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(widget->IsVisible());

  // Clean up: closing file chooser and triggering deactivation should close the
  // widget.
  views::test::WidgetDestroyedWaiter waiter2(widget);
  ui_manager->OnFileChooserClosed();
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  waiter2.Wait();
  EXPECT_FALSE(ui_manager->widget());
}

TEST_F(OmniboxEverywhereUIManagerTest, MultiProfileSwapping) {
  TestingProfile profile2;
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  EXPECT_EQ(ui_manager->profile(), &profile_);
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  // Swapping to profile2 should update the active profile on the same UIManager
  // shell.
  ui_manager->ShowForProfile(&profile2, GetContext());
  EXPECT_EQ(ui_manager->profile(), &profile2);
  EXPECT_TRUE(ui_manager->widget());

  // Clean up.
  ui_manager->Close();
  EXPECT_FALSE(ui_manager->widget());
}

TEST_F(OmniboxEverywhereUIManagerTest,
       ShowForProfileReactivatesExistingWidget) {
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // ShowForProfile when already visible for the same profile should NOT close
  // the widget.
  ui_manager->ShowForProfile(&profile_, GetContext());
  EXPECT_EQ(ui_manager->widget(), widget);
  EXPECT_TRUE(widget->IsVisible());

  ui_manager->Close();
}

TEST_F(OmniboxEverywhereUIManagerTest, ShutdownSynchronouslyDestroysResources) {
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  ASSERT_TRUE(ui_manager->contents_wrapper_for_testing());

  ui_manager->Shutdown();

  EXPECT_FALSE(ui_manager->widget());
  EXPECT_FALSE(ui_manager->contents_wrapper_for_testing());
  EXPECT_EQ(ui_manager->profile(), nullptr);
  EXPECT_FALSE(ui_manager->is_file_chooser_open_for_testing());
}

TEST_F(OmniboxEverywhereUIManagerTest, NavigationAndActivationStateTracking) {
  auto ui_manager = CreateUIManager();

  EXPECT_FALSE(ui_manager->IsNavigating());

  ui_manager->ShowForProfile(&profile_, GetContext());
  EXPECT_FALSE(ui_manager->IsNavigating());

  ui_manager->SetIsNavigating(true);
  EXPECT_TRUE(ui_manager->IsNavigating());

  ui_manager->Close();
  EXPECT_FALSE(ui_manager->IsNavigating());
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ShowPositionsOnTargetDisplay DISABLED_ShowPositionsOnTargetDisplay
#else
#define MAYBE_ShowPositionsOnTargetDisplay ShowPositionsOnTargetDisplay
#endif
TEST_F(OmniboxEverywhereUIManagerTest, MAYBE_ShowPositionsOnTargetDisplay) {
  // Create and set up a TestScreen with two displays.
  // Display 1: 0, 0, 800, 600 (Primary)
  // Display 2: 800, 0, 1024, 768 (Secondary)
  display::test::TestScreen test_screen(/*create_display=*/false,
                                        /*register_screen=*/false);
  display::Screen* old_screen = display::Screen::SetScreenInstance(nullptr);
  display::Screen::SetScreenInstance(&test_screen);

  display::Display display1(1, gfx::Rect(0, 0, 800, 600));
  display::Display display2(2, gfx::Rect(800, 0, 1024, 768));
  test_screen.display_list().AddDisplay(display1,
                                        display::DisplayList::Type::PRIMARY);
  test_screen.display_list().AddDisplay(
      display2, display::DisplayList::Type::NOT_PRIMARY);

  // Set the fake cursor on the second display.
  test_screen.set_cursor_screen_point(gfx::Point(1200, 300));

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();

  // Verify that the widget was positioned on the secondary display (nearest to
  // cursor).
  display::Display current_display =
      display::Screen::Get()->GetDisplayMatching(widget_bounds);
  EXPECT_NE(current_display.id(),
            display::Screen::Get()->GetPrimaryDisplay().id());

  ui_manager->Close();
  display::Screen::SetScreenInstance(nullptr);
  display::Screen::SetScreenInstance(old_screen);
}

TEST_F(OmniboxEverywhereUIManagerTest, DrivePickerStateTracking) {
  auto ui_manager = CreateUIManager();
  EXPECT_FALSE(ui_manager->is_drive_picker_open_for_testing());

  ui_manager->OnDrivePickerOpened();
  EXPECT_TRUE(ui_manager->is_drive_picker_open_for_testing());

  ui_manager->OnDrivePickerClosed();
  EXPECT_FALSE(ui_manager->is_drive_picker_open_for_testing());
}

TEST_F(OmniboxEverywhereUIManagerTest, DismissBypassedDuringDrivePicker) {
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Mark drive picker as open.
  ui_manager->OnDrivePickerOpened();
  EXPECT_TRUE(ui_manager->is_drive_picker_open_for_testing());

  // Simulating deactivation while drive picker is open should NOT close the
  // widget.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(widget->IsVisible());

  // Clean up: closing drive picker and triggering deactivation should close the
  // widget.
  views::test::WidgetDestroyedWaiter waiter(widget);
  ui_manager->OnDrivePickerClosed();
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  waiter.Wait();
  EXPECT_FALSE(ui_manager->widget());
}

TEST_F(OmniboxEverywhereUIManagerTest,
       BrowserCollectionObserverNoCrashWhenNull) {
  auto ui_manager = CreateUIManager();
  MockBrowserWindowInterface mock_browser;
  ui_manager->OnBrowserActivated(&mock_browser);
  ui_manager->OnBrowserClosed(&mock_browser);
}

TEST_F(OmniboxEverywhereUIManagerTest,
       DraggableRegionsChangedAndEventHandling) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget_delegate());

  std::vector<blink::mojom::DraggableRegionPtr> regions;

  auto drag_region = blink::mojom::DraggableRegion::New();
  drag_region->bounds = gfx::Rect(0, 0, 800, 600);
  drag_region->draggable = true;
  regions.push_back(std::move(drag_region));

  auto no_drag_input_region = blink::mojom::DraggableRegion::New();
  no_drag_input_region->bounds = gfx::Rect(100, 30, 400, 50);
  no_drag_input_region->draggable = false;
  regions.push_back(std::move(no_drag_input_region));

  ui_manager->DraggableRegionsChanged(regions, nullptr);

  // Background point (draggable region) -> should NOT descend into child (claim
  // for drag).
  EXPECT_FALSE(
      ui_manager->widget_delegate()->ShouldDescendIntoChildForEventHandling(
          gfx::NativeView(), gfx::Point(10, 10)));
  EXPECT_FALSE(
      ui_manager->widget_delegate()->ShouldDescendIntoChildForEventHandling(
          gfx::NativeView(), gfx::Point(600, 40)));

  // Points inside search input (non-draggable region) -> SHOULD descend into
  // child for button/input clicks.
  EXPECT_TRUE(
      ui_manager->widget_delegate()->ShouldDescendIntoChildForEventHandling(
          gfx::NativeView(), gfx::Point(200, 40)));
  EXPECT_TRUE(
      ui_manager->widget_delegate()->ShouldDescendIntoChildForEventHandling(
          gfx::NativeView(), gfx::Point(400, 50)));
}

TEST_F(OmniboxEverywhereUIManagerTest, EarlyDraggableRegionsChangedPreserved) {
  auto ui_manager = CreateUIManager();
  std::vector<blink::mojom::DraggableRegionPtr> regions;

  auto drag_region = blink::mojom::DraggableRegion::New();
  drag_region->bounds = gfx::Rect(0, 0, 800, 600);
  drag_region->draggable = true;
  regions.push_back(std::move(drag_region));

  // Trigger region update BEFORE ShowUI / widget creation.
  ui_manager->DraggableRegionsChanged(regions, nullptr);

  // Now create widget via ShowForProfile.
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget_delegate());

  // Cached region should be applied to widget_delegate_.
  EXPECT_FALSE(
      ui_manager->widget_delegate()->ShouldDescendIntoChildForEventHandling(
          gfx::NativeView(), gfx::Point(10, 10)));
}
