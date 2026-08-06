// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/view_type_utils.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace omnibox_everywhere {

class OmniboxEverywhereBrowserTest : public InteractiveBrowserTest {
 public:
  OmniboxEverywhereBrowserTest() {
    feature_list_.InitAndEnableFeature(omnibox::kOmniboxEverywhere);
  }
  ~OmniboxEverywhereBrowserTest() override = default;

  // Simulates triggering the global hotkey to show or dismiss the Omnibox
  // Everywhere widget.
  auto InvokeViaHotkey() {
    return Do([]() {
      OmniboxEverywhereController* controller =
          g_browser_process->GetFeatures()->omnibox_everywhere_controller();
      controller->OnKeyPressed(ui::Accelerator(
          ui::VKEY_SPACE, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR));
    });
  }

  // Moves the mouse cursor to `offset` relative to the top-left origin (0, 0)
  // of `view`, converting view-local coordinates to absolute screen
  // coordinates.
  auto MoveMouseInView(ElementSpecifier view, gfx::Point offset) {
    return MoveMouseTo(view, base::BindOnce(
                                 [](gfx::Point pt, ui::TrackedElement* el) {
                                   views::View* v = AsView<views::View>(el);
                                   return views::View::ConvertPointToScreen(v,
                                                                            pt);
                                 },
                                 offset));
  }

  // Checks whether the Omnibox Everywhere widget exists and matches the
  // `expected_visible` state.
  auto CheckWidgetVisible(bool expected_visible) {
    return CheckResult(
        []() {
          OmniboxEverywhereController* controller =
              g_browser_process->GetFeatures()->omnibox_everywhere_controller();
          views::Widget* widget = controller->ui_manager()->widget();
          return controller->IsVisible() && widget && widget->IsVisible();
        },
        expected_visible);
  }

  // Assigns a Kombucha element name to the widget's contents view for element
  // targeting in test steps.
  auto NameOmniboxContentsView(std::string_view name) {
    return NameView(
        name, base::BindOnce([]() -> views::View* {
          OmniboxEverywhereController* controller =
              g_browser_process->GetFeatures()->omnibox_everywhere_controller();
          return controller->ui_manager()->widget_delegate()->GetContentsView();
        }));
  }

  // Instruments the WebUI WebContents view and waits for the search input
  // element (`cr-searchbox-input`) to render in the DOM.
  auto WaitForOmniboxWebUIReady(ui::ElementIdentifier web_contents_id) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kSearchInputLoadedEvent);
    StateChange search_input_loaded;
    search_input_loaded.where = {
        "omnibox-everywhere-app",
        "omnibox-everywhere-omnibox",
        "cr-searchbox-input",
    };
    search_input_loaded.event = kSearchInputLoadedEvent;
    return Steps(
        InstrumentNonTabWebView(
            web_contents_id, base::BindOnce([]() -> views::View* {
              OmniboxEverywhereController* controller =
                  g_browser_process->GetFeatures()
                      ->omnibox_everywhere_controller();
              return controller->ui_manager()
                  ->widget_delegate()
                  ->GetContentsView();
            })),
        WaitForStateChange(web_contents_id, search_input_loaded));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest, ShowAndCloseWidget) {
  OmniboxEverywhereUIManager ui_manager;

  EXPECT_FALSE(ui_manager.widget());

  // Show the widget.
  ui_manager.ShowForProfile(browser()->GetProfile(),
                            browser()->GetWindow()->GetNativeWindow());

  views::Widget* widget = ui_manager.widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Check the widget delegate and its properties.
  views::WidgetDelegate* delegate = widget->widget_delegate();
  ASSERT_TRUE(delegate);
  EXPECT_TRUE(delegate->CanActivate());
  EXPECT_FALSE(delegate->CanMaximize());
  EXPECT_FALSE(delegate->CanMinimize());
  EXPECT_FALSE(delegate->CanResize());

  // Close the widget and wait for destruction.
  views::test::WidgetDestroyedWaiter waiter(widget);
  ui_manager.Close();
  waiter.Wait();

  EXPECT_FALSE(ui_manager.widget());
}

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest, FocusAndActivationState) {
  OmniboxEverywhereUIManager ui_manager;

  ui_manager.ShowForProfile(browser()->GetProfile(),
                            browser()->GetWindow()->GetNativeWindow());
  views::Widget* widget = ui_manager.widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  views::test::WaitForWidgetActive(widget, true);
  EXPECT_TRUE(widget->IsActive());

  views::test::WidgetDestroyedWaiter waiter(widget);
  ui_manager.Close();
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest, ShowAndDismissViaHotkey) {
  OmniboxEverywhereController* controller =
      g_browser_process->GetFeatures()->omnibox_everywhere_controller();
  ASSERT_TRUE(controller);

  EXPECT_FALSE(controller->IsVisible());

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOmniboxWebContentsId);

  RunTestSequence(InvokeViaHotkey(), CheckWidgetVisible(true),
                  WaitForOmniboxWebUIReady(kOmniboxWebContentsId),
                  InvokeViaHotkey(), CheckWidgetVisible(false));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#define MAYBE_IgnoreDragToMoveInNoDragRegion IgnoreDragToMoveInNoDragRegion
#else
#define MAYBE_IgnoreDragToMoveInNoDragRegion \
  DISABLED_IgnoreDragToMoveInNoDragRegion
#endif
IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest,
                       MAYBE_IgnoreDragToMoveInNoDragRegion) {
#if BUILDFLAG(IS_OZONE)
  if (ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP()
        << "Window drag is not supported under Wayland test compositor.";
  }
#endif

  OmniboxEverywhereController* controller =
      g_browser_process->GetFeatures()->omnibox_everywhere_controller();
  ASSERT_TRUE(controller);

  gfx::Point initial_origin;
  const char kContentsView[] = "OmniboxContentsView";
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOmniboxWebContentsId);

  RunTestSequence(InvokeViaHotkey(), CheckWidgetVisible(true),
                  WaitForOmniboxWebUIReady(kOmniboxWebContentsId), Do([&]() {
                    initial_origin = controller->ui_manager()
                                         ->widget()
                                         ->GetWindowBoundsInScreen()
                                         .origin();
                  }),
                  NameOmniboxContentsView(kContentsView),
                  // Search input region is non-draggable (centered at y=40).
                  MoveMouseInView(kContentsView, gfx::Point(400, 40)),
                  ClickMouse(ui_controls::LEFT, /*release=*/false),
                  MoveMouseInView(kContentsView, gfx::Point(500, 40)),
                  ReleaseMouse(ui_controls::LEFT), Check([&]() {
                    views::Widget* widget = controller->ui_manager()->widget();
                    return widget->GetWindowBoundsInScreen().origin() ==
                           initial_origin;
                  }),
                  InvokeViaHotkey(), CheckWidgetVisible(false));
}

// TODO(crbug.com/40249472): Support modal dragging on Windows with Kombucha.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_DragToMoveWindowInDraggableRegion \
  DragToMoveWindowInDraggableRegion
#else
#define MAYBE_DragToMoveWindowInDraggableRegion \
  DISABLED_DragToMoveWindowInDraggableRegion
#endif
IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest,
                       MAYBE_DragToMoveWindowInDraggableRegion) {
#if BUILDFLAG(IS_OZONE)
  if (ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP()
        << "Window drag is not supported under Wayland test compositor.";
  }
#endif

  OmniboxEverywhereController* controller =
      g_browser_process->GetFeatures()->omnibox_everywhere_controller();
  ASSERT_TRUE(controller);

  gfx::Point initial_origin;
  const char kContentsView[] = "OmniboxContentsView";
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOmniboxWebContentsId);

  const gfx::Point initial_point(10, 10);
  const gfx::Point threshold_point(25, 25);
  ASSERT_TRUE(
      views::View::ExceededDragThreshold(threshold_point - initial_point));

  RunTestSequence(InvokeViaHotkey(), CheckWidgetVisible(true),
                  WaitForOmniboxWebUIReady(kOmniboxWebContentsId), Do([&]() {
                    initial_origin = controller->ui_manager()
                                         ->widget()
                                         ->GetWindowBoundsInScreen()
                                         .origin();
                  }),
                  NameOmniboxContentsView(kContentsView),
                  // Drag background area (draggable region at 10, 10).
                  MoveMouseInView(kContentsView, initial_point),
                  ClickMouse(ui_controls::LEFT, /*release=*/false),
                  MoveMouseInView(kContentsView, threshold_point),
                  MoveMouseInView(kContentsView, gfx::Point(110, 110)),
                  ReleaseMouse(ui_controls::LEFT), Check([&]() {
                    views::Widget* widget = controller->ui_manager()->widget();
                    return widget->GetWindowBoundsInScreen().origin() !=
                           initial_origin;
                  }),
                  InvokeViaHotkey(), CheckWidgetVisible(false));
}

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest, VoicePermissionState) {
  OmniboxEverywhereUIManager ui_manager;
  ui_manager.ShowForProfile(browser()->GetProfile(),
                            browser()->GetWindow()->GetNativeWindow());

  ASSERT_TRUE(ui_manager.contents_wrapper_for_testing());
  content::WebContents* contents =
      ui_manager.contents_wrapper_for_testing()->web_contents();
  ASSERT_TRUE(contents);

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  EXPECT_EQ(extensions::GetViewType(contents),
            extensions::mojom::ViewType::kComponent);
#endif
  EXPECT_TRUE(permissions::PermissionRequestManager::FromWebContents(contents));

  views::test::WidgetDestroyedWaiter waiter(ui_manager.widget());
  ui_manager.Close();
  waiter.Wait();
}

}  // namespace omnibox_everywhere
