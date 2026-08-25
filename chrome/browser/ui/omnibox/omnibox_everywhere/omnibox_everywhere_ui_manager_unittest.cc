// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/context_menu_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "ui/display/screen.h"
#include "ui/display/test/test_screen.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_runner_handler.h"
#include "ui/views/test/menu_runner_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

class ScopedScreenOverride {
 public:
  explicit ScopedScreenOverride(display::Screen* new_screen)
      : old_screen_(display::Screen::SetScreenInstance(nullptr)) {
    display::Screen::SetScreenInstance(new_screen);
  }
  ~ScopedScreenOverride() {
    display::Screen::SetScreenInstance(nullptr);
    if (old_screen_) {
      display::Screen::SetScreenInstance(old_screen_);
    }
  }

 private:
  raw_ptr<display::Screen> old_screen_;
};

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

class TestMenuRunnerHandler : public views::MenuRunnerHandler {
 public:
  TestMenuRunnerHandler() = default;
  ~TestMenuRunnerHandler() override = default;

  void RunMenuAt(views::Widget* parent,
                 views::MenuButtonController* button_controller,
                 const gfx::Rect& bounds,
                 views::MenuAnchorPosition anchor,
                 ui::mojom::MenuSourceType source_type,
                 int32_t types) override {}
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
    auto ui_manager =
        std::make_unique<omnibox_everywhere::OmniboxEverywhereUIManager>(
            base::BindRepeating(
                [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
                  return std::make_unique<TestWebUIContentsWrapper>(profile);
                }));
    ui_manager->SetMenuRunnerFactoryForTesting(base::BindRepeating(
        [](ui::MenuModel* model, base::RepeatingClosure on_closed) {
          auto runner = std::make_unique<views::MenuRunner>(
              model,
              views::MenuRunner::HAS_MNEMONICS |
                  views::MenuRunner::CONTEXT_MENU,
              std::move(on_closed));
          views::test::MenuRunnerTestAPI(runner.get())
              .SetMenuRunnerHandler(std::make_unique<TestMenuRunnerHandler>());
          return runner;
        }));
    return ui_manager;
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

  // Closing the UI manager should trigger hiding the widget.
  ui_manager->Close();
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_TRUE(ui_manager->widget());

  // Shutdown should destroy the widget.
  ui_manager->Shutdown();
  EXPECT_FALSE(ui_manager->widget());
}

TEST_F(OmniboxEverywhereUIManagerTest, ShowWhileWidgetIsHidden) {
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* first_widget = ui_manager->widget();
  ASSERT_TRUE(first_widget);

  // Close (hide) the widget.
  ui_manager->Close();
  EXPECT_FALSE(first_widget->IsVisible());

  // Showing it again immediately should reactivate the existing hidden widget.
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* second_widget = ui_manager->widget();
  ASSERT_TRUE(second_widget);
  EXPECT_TRUE(second_widget->IsVisible());
  EXPECT_EQ(first_widget, second_widget);

  // Clean up.
  ui_manager->Shutdown();
  EXPECT_FALSE(ui_manager->widget());
}

// Verifies that when the widget is first shown on a standard display, its
// initial bounds are centered and initialized with the default resting height
// (kDefaultRestingHeight) and fixed width (kPopupFixedWidth) to prevent visual
// resize flashing before WebUI auto-resize occurs.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_InitialBoundsMatchRestingHeight \
  DISABLED_InitialBoundsMatchRestingHeight
#else
#define MAYBE_InitialBoundsMatchRestingHeight InitialBoundsMatchRestingHeight
#endif
TEST_F(OmniboxEverywhereUIManagerTest, MAYBE_InitialBoundsMatchRestingHeight) {
  display::test::TestScreen test_screen(/*create_display=*/false,
                                        /*register_screen=*/false);
  ScopedScreenOverride screen_override(&test_screen);

  display::Display display1(1, gfx::Rect(0, 0, 1920, 1080));
  test_screen.display_list().AddDisplay(display1,
                                        display::DisplayList::Type::PRIMARY);

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  EXPECT_EQ(
      widget->GetWindowBoundsInScreen(),
      gfx::Rect(
          536, 464,
          omnibox_everywhere::OmniboxEverywhereUIManager::kPopupFixedWidth,
          omnibox_everywhere::OmniboxEverywhereUIManager::
              kDefaultRestingHeight));

  ui_manager->Shutdown();
}

// Verifies that on displays with dimensions smaller than the fixed popup width,
// widget bounds calculation clamps width and coordinates to remain fully within
// the visible work area without overflowing or negative positioning.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CalculateBoundsClampsToSmallDisplays \
  DISABLED_CalculateBoundsClampsToSmallDisplays
#else
#define MAYBE_CalculateBoundsClampsToSmallDisplays \
  CalculateBoundsClampsToSmallDisplays
#endif
TEST_F(OmniboxEverywhereUIManagerTest,
       MAYBE_CalculateBoundsClampsToSmallDisplays) {
  display::test::TestScreen test_screen(/*create_display=*/false,
                                        /*register_screen=*/false);
  ScopedScreenOverride screen_override(&test_screen);

  // Display smaller than default popup width (800 < 848).
  display::Display small_display(1, gfx::Rect(0, 0, 800, 600));
  test_screen.display_list().AddDisplay(small_display,
                                        display::DisplayList::Type::PRIMARY);

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  // Width is clamped to work area width (800) and x starts at 0 (non-negative).
  EXPECT_EQ(widget->GetWindowBoundsInScreen().x(), 0);
  EXPECT_EQ(widget->GetWindowBoundsInScreen().width(), 800);
  EXPECT_EQ(
      widget->GetWindowBoundsInScreen().height(),
      omnibox_everywhere::OmniboxEverywhereUIManager::kDefaultRestingHeight);

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest, FileChooserStateTracking) {
  auto ui_manager = CreateUIManager();
  EXPECT_FALSE(ui_manager->is_file_chooser_open_for_testing());

  ui_manager->OnFileChooserOpened();
  EXPECT_TRUE(ui_manager->is_file_chooser_open_for_testing());

  ui_manager->OnFileChooserClosed();
  EXPECT_FALSE(ui_manager->is_file_chooser_open_for_testing());
}

TEST_F(OmniboxEverywhereUIManagerTest, DismissOnDeactivationInEphemeralMode) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Simulating deactivation (active = false) in ephemeral mode should hide the
  // widget.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return !widget->IsVisible(); }));
  EXPECT_TRUE(ui_manager->widget());
}

TEST_F(OmniboxEverywhereUIManagerTest, PersistentDeactivationDemotesZOrder) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Initial show promotes ZOrder to kFloatingUIElement.
  EXPECT_EQ(widget->GetZOrderLevel(), ui::ZOrderLevel::kFloatingUIElement);

  // Simulating deactivation (active = false) in persistent mode should demote
  // ZOrder to kNormal while keeping widget visible.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(widget->GetZOrderLevel(), ui::ZOrderLevel::kNormal);

  // Re-invoking ShowForProfile should re-elevate ZOrder to kFloatingUIElement.
  ui_manager->ShowForProfile(&profile_, GetContext());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(widget->GetZOrderLevel(), ui::ZOrderLevel::kFloatingUIElement);

  ui_manager->Close();
}

TEST_F(OmniboxEverywhereUIManagerTest, DismissBypassedDuringFileChooser) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
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

  // Clean up: closing file chooser and triggering deactivation should hide the
  // widget in ephemeral mode.
  ui_manager->OnFileChooserClosed();
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return !widget->IsVisible(); }));
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
  ui_manager->Shutdown();
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
  ScopedScreenOverride screen_override(&test_screen);

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
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_PreservePositionAcrossDisplaysOnReinvoke \
  DISABLED_PreservePositionAcrossDisplaysOnReinvoke
#else
#define MAYBE_PreservePositionAcrossDisplaysOnReinvoke \
  PreservePositionAcrossDisplaysOnReinvoke
#endif
TEST_F(OmniboxEverywhereUIManagerTest,
       MAYBE_PreservePositionAcrossDisplaysOnReinvoke) {
  display::test::TestScreen test_screen(/*create_display=*/false,
                                        /*register_screen=*/false);
  ScopedScreenOverride screen_override(&test_screen);

  display::Display display1(1, gfx::Rect(0, 0, 800, 600));
  display::Display display2(2, gfx::Rect(800, 0, 1024, 768));
  test_screen.display_list().AddDisplay(display1,
                                        display::DisplayList::Type::PRIMARY);
  test_screen.display_list().AddDisplay(
      display2, display::DisplayList::Type::NOT_PRIMARY);

  // Set the fake cursor on the second display initially.
  test_screen.set_cursor_screen_point(gfx::Point(1200, 300));

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  gfx::Rect initial_bounds = widget->GetWindowBoundsInScreen();

  // Move the cursor to the first display and re-invoke ShowForProfile.
  test_screen.set_cursor_screen_point(gfx::Point(100, 100));
  ui_manager->ShowForProfile(&profile_, GetContext());

  // The widget bounds should remain unchanged on the secondary display.
  EXPECT_EQ(widget->GetWindowBoundsInScreen(), initial_bounds);

  ui_manager->Close();
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
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
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

  // Clean up: closing drive picker and triggering deactivation should hide the
  // widget in ephemeral mode.
  ui_manager->OnDrivePickerClosed();
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return !widget->IsVisible(); }));
}

TEST_F(OmniboxEverywhereUIManagerTest, CloseDestroysWidgetWhenChooserOpen) {
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  // Open file chooser.
  ui_manager->OnFileChooserOpened();

  // Close() while file chooser is open should destroy widget to prevent
  // orphaned modals.
  ui_manager->Close();
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

TEST_F(OmniboxEverywhereUIManagerTest, DismissBypassedDuringContextMenu) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Mark context menu as open.
  ui_manager->set_is_context_menu_open_for_testing(true);
  EXPECT_TRUE(ui_manager->is_context_menu_open_for_testing());

  // Simulating deactivation while context menu is open should NOT close the
  // widget.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(widget->IsVisible());

  // Clean up: closing context menu and triggering deactivation should close the
  // widget in ephemeral mode.
  ui_manager->OnContextMenuClosedForTesting();
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return !widget->IsVisible(); }));
  EXPECT_TRUE(ui_manager->widget());
}

TEST_F(OmniboxEverywhereUIManagerTest, CloseCancelsOpenContextMenu) {
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  auto* rfh = ui_manager->contents_wrapper_for_testing()
                  ->web_contents()
                  ->GetPrimaryMainFrame();
  content::ContextMenuParams params;
  params.is_editable = true;

  bool menu_runner_created = false;
  ui_manager->SetMenuRunnerFactoryForTesting(base::BindRepeating(
      [](bool* created, ui::MenuModel* model,
         base::RepeatingClosure on_closed) {
        *created = true;
        auto runner = std::make_unique<views::MenuRunner>(
            model,
            views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU,
            on_closed);
        views::test::MenuRunnerTestAPI(runner.get())
            .SetMenuRunnerHandler(std::make_unique<TestMenuRunnerHandler>());
        return runner;
      },
      &menu_runner_created));

  ui_manager->HandleContextMenu(*rfh, params);
  EXPECT_TRUE(menu_runner_created);
  EXPECT_TRUE(ui_manager->is_context_menu_open_for_testing());

  // Calling Close() while context menu is open should cancel the runner and
  // reset state.
  ui_manager->Close();
  EXPECT_FALSE(ui_manager->is_context_menu_open_for_testing());
  EXPECT_FALSE(ui_manager->widget()->IsVisible());
}

TEST_F(OmniboxEverywhereUIManagerTest, ContextMenuModelEditableElement) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  auto* rfh = ui_manager->contents_wrapper_for_testing()
                  ->web_contents()
                  ->GetPrimaryMainFrame();

  content::ContextMenuParams params;
  params.is_editable = true;
  EXPECT_TRUE(ui_manager->HandleContextMenu(*rfh, params));

  const ui::SimpleMenuModel* model =
      ui_manager->context_menu_model_for_testing();
  ASSERT_TRUE(model);
  EXPECT_EQ(model->GetItemCount(), 5u);
  EXPECT_EQ(model->GetCommandIdAt(0),
            omnibox_everywhere::OmniboxEverywhereUIManager::kCut);
  EXPECT_EQ(model->GetCommandIdAt(1),
            omnibox_everywhere::OmniboxEverywhereUIManager::kCopy);
  EXPECT_EQ(model->GetCommandIdAt(2),
            omnibox_everywhere::OmniboxEverywhereUIManager::kPaste);
  EXPECT_EQ(model->GetTypeAt(3), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(4),
            omnibox_everywhere::OmniboxEverywhereUIManager::kSelectAll);
}

TEST_F(OmniboxEverywhereUIManagerTest,
       ContextMenuModelNonEditableElementWithSelection) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  auto* rfh = ui_manager->contents_wrapper_for_testing()
                  ->web_contents()
                  ->GetPrimaryMainFrame();

  content::ContextMenuParams params;
  params.is_editable = false;
  params.selection_text = u"selected text";
  EXPECT_TRUE(ui_manager->HandleContextMenu(*rfh, params));

  const ui::SimpleMenuModel* model =
      ui_manager->context_menu_model_for_testing();
  ASSERT_TRUE(model);
  EXPECT_EQ(model->GetItemCount(), 3u);
  EXPECT_EQ(model->GetCommandIdAt(0),
            omnibox_everywhere::OmniboxEverywhereUIManager::kCopy);
  EXPECT_EQ(model->GetTypeAt(1), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(2),
            omnibox_everywhere::OmniboxEverywhereUIManager::kSelectAll);
}

TEST_F(OmniboxEverywhereUIManagerTest,
       ContextMenuSuppressedOnNonEditableBackground) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  auto* rfh = ui_manager->contents_wrapper_for_testing()
                  ->web_contents()
                  ->GetPrimaryMainFrame();

  // Right-clicking on empty background space (not editable, no selection text).
  content::ContextMenuParams params;
  params.is_editable = false;
  params.selection_text = u"";

  bool menu_runner_created = false;
  ui_manager->SetMenuRunnerFactoryForTesting(base::BindRepeating(
      [](bool* created, ui::MenuModel* model,
         base::RepeatingClosure on_closed) {
        *created = true;
        auto runner = std::make_unique<views::MenuRunner>(
            model,
            views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU,
            on_closed);
        views::test::MenuRunnerTestAPI(runner.get())
            .SetMenuRunnerHandler(std::make_unique<TestMenuRunnerHandler>());
        return runner;
      },
      &menu_runner_created));

  // Should return true (consumed/handled) but NOT create or open a context
  // menu.
  EXPECT_TRUE(ui_manager->HandleContextMenu(*rfh, params));
  EXPECT_FALSE(menu_runner_created);
  EXPECT_FALSE(ui_manager->is_context_menu_open_for_testing());
  EXPECT_FALSE(ui_manager->context_menu_model_for_testing());
}

TEST_F(OmniboxEverywhereUIManagerTest, ContextMenuCommandEnablement) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  auto* rfh = ui_manager->contents_wrapper_for_testing()
                  ->web_contents()
                  ->GetPrimaryMainFrame();

  content::ContextMenuParams params;
  params.is_editable = true;
  params.edit_flags = blink::ContextMenuDataEditFlags::kCanCut |
                      blink::ContextMenuDataEditFlags::kCanCopy;
  ui_manager->HandleContextMenu(*rfh, params);

  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kCut));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kCopy));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kPaste));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kSelectAll));

  // Without edit flags, Cut and Copy should be disabled if selection is empty.
  params.edit_flags = 0;
  params.selection_text = u"";
  ui_manager->HandleContextMenu(*rfh, params);

  EXPECT_FALSE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kCut));
  EXPECT_FALSE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kCopy));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kPaste));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kSelectAll));
}

// TODO(crbug.com/546710681): Re-enable test on linux
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_ResizeDueToAutoResizeUpdatesWidgetBounds \
  DISABLED_ResizeDueToAutoResizeUpdatesWidgetBounds
#else
#define MAYBE_ResizeDueToAutoResizeUpdatesWidgetBounds \
  ResizeDueToAutoResizeUpdatesWidgetBounds
#endif
TEST_F(OmniboxEverywhereUIManagerTest,
       MAYBE_ResizeDueToAutoResizeUpdatesWidgetBounds) {
  display::test::TestScreen test_screen(/*create_display=*/false,
                                        /*register_screen=*/false);
  display::Screen* old_screen = display::Screen::SetScreenInstance(nullptr);
  display::Screen::SetScreenInstance(&test_screen);
  base::ScopedClosureRunner screen_restorer(base::BindOnce(
      [](display::Screen* old_screen) {
        display::Screen::SetScreenInstance(nullptr);
        display::Screen::SetScreenInstance(old_screen);
      },
      old_screen));
  test_screen.display_list().AddDisplay(
      display::Display(1, gfx::Rect(0, 0, 1920, 1080)),
      display::DisplayList::Type::PRIMARY);

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  EXPECT_EQ(widget->GetWindowBoundsInScreen().width(), 848);

  // Resize above minimum height should resize the widget height directly.
  ui_manager->ResizeDueToAutoResize(nullptr, gfx::Size(848, 150));
  EXPECT_EQ(widget->GetWindowBoundsInScreen().height(), 150);
  EXPECT_EQ(widget->GetWindowBoundsInScreen().width(), 848);

  // Resize below minimum height (56) should clamp to 56.
  ui_manager->ResizeDueToAutoResize(nullptr, gfx::Size(848, 30));
  EXPECT_EQ(widget->GetWindowBoundsInScreen().height(), 56);
  EXPECT_EQ(widget->GetWindowBoundsInScreen().width(), 848);

  // Even if widget width was temporarily modified (e.g. edge clamping),
  // ResizeDueToAutoResize enforces the fixed width.
  gfx::Rect clamped_bounds = widget->GetWindowBoundsInScreen();
  clamped_bounds.set_width(400);
  widget->SetBounds(clamped_bounds);
  EXPECT_EQ(widget->GetWindowBoundsInScreen().width(), 400);

  ui_manager->ResizeDueToAutoResize(nullptr, gfx::Size(848, 200));
  EXPECT_EQ(widget->GetWindowBoundsInScreen().height(), 200);
  EXPECT_EQ(widget->GetWindowBoundsInScreen().width(), 848);

  // While dragging, AutoResize should be deferred.
  ui_manager->OnWidgetUserDragStarted(widget);
  ui_manager->ResizeDueToAutoResize(nullptr, gfx::Size(848, 300));
  // Size remains unchanged during drag.
  EXPECT_EQ(widget->GetWindowBoundsInScreen().height(), 200);

  // When drag ends, the pending AutoResize is applied.
  ui_manager->OnWidgetUserDragEnded(widget);
  EXPECT_EQ(widget->GetWindowBoundsInScreen().height(), 300);

  ui_manager->Close();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       RebuildWidgetOnEphemeralModelPrefChangeWhenVisible) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* original_widget = ui_manager->widget();
  ASSERT_TRUE(original_widget);
  EXPECT_TRUE(original_widget->IsVisible());

  // Changing the ephemeral model pref should rebuild the widget.
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  views::Widget* rebuilt_widget = ui_manager->widget();
  ASSERT_TRUE(rebuilt_widget);
  EXPECT_TRUE(rebuilt_widget->IsVisible());
  EXPECT_NE(original_widget, rebuilt_widget);

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       RebuildWidgetOnEphemeralModelPrefChangeWhenHidden) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());

  // Hide the widget.
  ui_manager->Close();
  EXPECT_FALSE(ui_manager->widget()->IsVisible());

  // Changing the ephemeral pref while hidden should clean up the old widget.
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  EXPECT_FALSE(ui_manager->widget());

  // Showing again creates a new widget with the updated ephemeral settings.
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       CleanUpWidgetOnMostVisitedPrefChangeWhenHidden) {
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpCustomLinksVisible, true);
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());

  // Hide the widget.
  ui_manager->Close();
  EXPECT_FALSE(ui_manager->widget()->IsVisible());
  EXPECT_TRUE(ui_manager->widget());

  // Changing a most-visited pref while hidden should clean up the old widget to
  // prevent stale frame buffer and tile flicker upon reopen.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpCustomLinksVisible, false);
  EXPECT_FALSE(ui_manager->widget());

  // Showing again creates a fresh widget with the updated preferences.
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       KeepWidgetOnMostVisitedPrefChangeWhenVisible) {
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpCustomLinksVisible, true);
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());

  // Changing a most-visited pref while visible should keep the widget alive for
  // in-place dynamic update via Mojo.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpCustomLinksVisible, false);
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest, ScreensharePickerStateTracking) {
  auto ui_manager = CreateUIManager();
  EXPECT_FALSE(ui_manager->is_screenshare_picker_open_for_testing());

  ui_manager->OnScreensharePickerOpened();
  EXPECT_TRUE(ui_manager->is_screenshare_picker_open_for_testing());

  ui_manager->OnScreensharePickerClosed();
  EXPECT_FALSE(ui_manager->is_screenshare_picker_open_for_testing());
}

TEST_F(OmniboxEverywhereUIManagerTest, DismissBypassedDuringScreensharePicker) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Mark screenshare picker as open.
  ui_manager->OnScreensharePickerOpened();
  EXPECT_TRUE(ui_manager->is_screenshare_picker_open_for_testing());

  // Simulating deactivation while screenshare picker is open should NOT close
  // the widget.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(widget->IsVisible());

  // Clean up: closing screenshare picker and triggering deactivation should
  // hide the widget in ephemeral mode.
  ui_manager->OnScreensharePickerClosed();
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return !widget->IsVisible(); }));
}
