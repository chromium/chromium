// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/background/omnibox_everywhere/omnibox_everywhere_background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/permissions/permission_request_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/base/window_open_disposition.h"
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
      controller->OnKeyPressed(
          ui::Accelerator(ui::VKEY_SPACE, ui::EF_ALT_DOWN));
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

  // Waits for the Omnibox Everywhere widget to reach `active` activation state.
  auto WaitForWidgetActiveState(bool active) {
    return Do([active]() {
      OmniboxEverywhereController* controller =
          g_browser_process->GetFeatures()->omnibox_everywhere_controller();
      if (controller && controller->ui_manager()->widget()) {
        views::test::WaitForWidgetActive(controller->ui_manager()->widget(),
                                         active);
      }
    });
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
    return Steps(InstrumentNonTabWebView(
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

  // Waits until the WebUI draggable regions have been received by the browser
  // process and applied to the widget delegate.
  auto WaitForDraggableRegions() {
    return CheckResult(
        []() {
          OmniboxEverywhereController* controller =
              g_browser_process->GetFeatures()->omnibox_everywhere_controller();
          return base::test::RunUntil([controller]() {
            return controller && controller->ui_manager() &&
                   controller->ui_manager()
                       ->draggable_region_for_testing()
                       .has_value() &&
                   !controller->ui_manager()
                        ->draggable_region_for_testing()
                        ->isEmpty();
          });
        },
        true);
  }

  // Simulates dragging the mouse from `start_point` to `end_point` within
  // `view`. Crosses the drag threshold to ensure window drag handlers are
  // triggered.
  auto DragMouseInView(ElementSpecifier view,
                       gfx::Point start_point,
                       gfx::Point end_point) {
    const gfx::Point threshold_point = start_point + gfx::Vector2d(15, 15);
    return Steps(MoveMouseInView(view, start_point),
                 ClickMouse(ui_controls::LEFT, /*release=*/false),
                 MoveMouseInView(view, threshold_point),
                 MoveMouseInView(view, end_point),
                 ReleaseMouse(ui_controls::LEFT));
  }

  // Tests that OpenUrl creates a browser window when no other browsers are
  // open, and updates widget visibility according to `ephemeral` mode.
  void TestOpenUrlCreatesBrowserWhenNoBrowsers(bool ephemeral) {
    Profile* profile = browser()->GetProfile();
    set_exit_when_last_browser_closes(false);

    GlobalFeatures* features = g_browser_process->GetFeatures();
    ASSERT_TRUE(features);
    auto* controller = features->omnibox_everywhere_controller();
    ASSERT_TRUE(controller);

    // Show the Omnibox Everywhere widget.
    controller->OnInvoke(InvocationSource::kGlobalHotkey, profile);
    EXPECT_TRUE(controller->IsVisible());

    // Close the existing browser window so 0 browser windows exist.
    CloseBrowserSynchronously(browser());
    EXPECT_EQ(0u, GlobalBrowserCollection::GetInstance()->GetSize());
    EXPECT_TRUE(controller->IsVisible());

    // Trigger OpenUrl from the Omnibox Everywhere service.
    auto* service = OmniboxEverywhereServiceFactory::GetForProfile(profile);
    ASSERT_TRUE(service);
    service->OpenUrl(GURL("chrome://version/"),
                     WindowOpenDisposition::CURRENT_TAB,
                     ui::PAGE_TRANSITION_TYPED);

    // Verify that a new browser window was created.
    EXPECT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
    if (ephemeral) {
      // In ephemeral mode, the popup widget is closed.
      EXPECT_FALSE(controller->IsVisible());
    } else {
      // In persistent mode, the popup widget remains visible and is demoted.
      EXPECT_TRUE(controller->IsVisible());
      EXPECT_FALSE(controller->ui_manager()->IsActive());
    }
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

  // Close (hide) the widget.
  ui_manager.Close();
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_TRUE(ui_manager.widget());

  // Shutdown destroys the widget.
  ui_manager.Shutdown();
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

  ui_manager.Close();
  EXPECT_FALSE(widget->IsVisible());
  ui_manager.Shutdown();
}

class OmniboxEverywhereEphemeralBrowserTest
    : public OmniboxEverywhereBrowserTest {
 public:
  void SetUpOnMainThread() override {
    OmniboxEverywhereBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(
        prefs::kOmniboxEverywhereEphemeralModel, true);
  }
};

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ShowAndDismissViaHotkey DISABLED_ShowAndDismissViaHotkey
#else
#define MAYBE_ShowAndDismissViaHotkey ShowAndDismissViaHotkey
#endif
IN_PROC_BROWSER_TEST_F(OmniboxEverywhereEphemeralBrowserTest,
                       MAYBE_ShowAndDismissViaHotkey) {
  OmniboxEverywhereController* controller =
      g_browser_process->GetFeatures()->omnibox_everywhere_controller();
  ASSERT_TRUE(controller);

  EXPECT_FALSE(controller->IsVisible());

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOmniboxWebContentsId);

  RunTestSequence(InvokeViaHotkey(), CheckWidgetVisible(true),
                  WaitForOmniboxWebUIReady(kOmniboxWebContentsId),
                  InvokeViaHotkey(), CheckWidgetVisible(false));
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CopyPasteSupportInQueryBox DISABLED_CopyPasteSupportInQueryBox
#else
#define MAYBE_CopyPasteSupportInQueryBox CopyPasteSupportInQueryBox
#endif
IN_PROC_BROWSER_TEST_F(OmniboxEverywhereEphemeralBrowserTest,
                       MAYBE_CopyPasteSupportInQueryBox) {
  OmniboxEverywhereController* controller =
      g_browser_process->GetFeatures()->omnibox_everywhere_controller();
  ASSERT_TRUE(controller);

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOmniboxWebContentsId);
  const DeepQuery kSearchInputElement = {
      "omnibox-everywhere-app",
      "omnibox-everywhere-omnibox",
      "cr-searchbox-input",
      "#input",
  };

  RunTestSequence(
      InvokeViaHotkey(), CheckWidgetVisible(true),
      WaitForOmniboxWebUIReady(kOmniboxWebContentsId),
      // Enter initial text into search input and verify.
      ExecuteJsAt(kOmniboxWebContentsId, kSearchInputElement,
                  "el => { el.focus(); el.value = 'initial query'; "
                  "el.dispatchEvent(new "
                  "Event('input', {bubbles: true})); }"),
      CheckJsResultAt(kOmniboxWebContentsId, kSearchInputElement,
                      "el => el.value", "initial query"),
      // Simulate paste into the query input element.
      ExecuteJsAt(
          kOmniboxWebContentsId, kSearchInputElement,
          "el => { "
          "  el.focus(); "
          "  el.select(); "
          "  const dataTransfer = new DataTransfer(); "
          "  dataTransfer.setData('text/plain', 'pasted search query'); "
          "  const pasteEvent = new ClipboardEvent('paste', { "
          "    clipboardData: dataTransfer, "
          "    bubbles: true, "
          "    cancelable: true "
          "  }); "
          "  el.dispatchEvent(pasteEvent); "
          "  if (!pasteEvent.defaultPrevented) { "
          "    el.value = 'pasted search query'; "
          "    el.dispatchEvent(new Event('input', {bubbles: true})); "
          "  } "
          "}"),
      // Verify query box value has updated to the pasted text.
      CheckJsResultAt(kOmniboxWebContentsId, kSearchInputElement,
                      "el => el.value", "pasted search query"),
      InvokeViaHotkey(), CheckWidgetVisible(false));
}

class OmniboxEverywherePersistentBrowserTest
    : public OmniboxEverywhereBrowserTest {
 public:
  void SetUpOnMainThread() override {
    OmniboxEverywhereBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(
        prefs::kOmniboxEverywhereEphemeralModel, false);
  }
};

#if BUILDFLAG(IS_WIN)
#define MAYBE_IgnoreDragToMoveInNoDragRegion IgnoreDragToMoveInNoDragRegion
#else
#define MAYBE_IgnoreDragToMoveInNoDragRegion \
  DISABLED_IgnoreDragToMoveInNoDragRegion
#endif
IN_PROC_BROWSER_TEST_F(OmniboxEverywherePersistentBrowserTest,
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
  gfx::Point drag_target;
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOmniboxWebContentsId);

  const DeepQuery kSearchInputElement = {
      "omnibox-everywhere-app",
      "omnibox-everywhere-omnibox",
      "cr-searchbox-input",
      "#input",
  };

  RunTestSequence(
      InvokeViaHotkey(), CheckWidgetVisible(true),
      WaitForWidgetActiveState(true),
      WaitForOmniboxWebUIReady(kOmniboxWebContentsId),
      WaitForDraggableRegions(),
      MoveMouseTo(kOmniboxWebContentsId, kSearchInputElement), Do([&]() {
        initial_origin = controller->ui_manager()
                             ->widget()
                             ->GetWindowBoundsInScreen()
                             .origin();
        drag_target = display::Screen::Get()->GetCursorScreenPoint() +
                      gfx::Vector2d(50, 0);
      }),
      // Drag horizontally within the non-draggable search input element.
      ClickMouse(ui_controls::LEFT, /*release=*/false),
      MoveMouseTo(std::ref(drag_target)), ReleaseMouse(ui_controls::LEFT),
      Check([&]() {
        views::Widget* widget = controller->ui_manager()->widget();
        return widget->GetWindowBoundsInScreen().origin() == initial_origin;
      }),
      InvokeViaHotkey(), CheckWidgetVisible(true),
      WaitForWidgetActiveState(false));
}

// TODO(crbug.com/40249472): Support modal dragging on Windows with Kombucha.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_DragToMoveWindowInDraggableRegion \
  DragToMoveWindowInDraggableRegion
#else
#define MAYBE_DragToMoveWindowInDraggableRegion \
  DISABLED_DragToMoveWindowInDraggableRegion
#endif
IN_PROC_BROWSER_TEST_F(OmniboxEverywherePersistentBrowserTest,
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

  RunTestSequence(
      InvokeViaHotkey(), CheckWidgetVisible(true),
      WaitForWidgetActiveState(true),
      WaitForOmniboxWebUIReady(kOmniboxWebContentsId),
      WaitForDraggableRegions(), Do([&]() {
        initial_origin = controller->ui_manager()
                             ->widget()
                             ->GetWindowBoundsInScreen()
                             .origin();
      }),
      NameOmniboxContentsView(kContentsView),
      DragMouseInView(kContentsView, gfx::Point(10, 5), gfx::Point(110, 5)),
      Check([&]() {
        views::Widget* widget = controller->ui_manager()->widget();
        return widget->GetWindowBoundsInScreen().origin() != initial_origin;
      }),
      InvokeViaHotkey(), CheckWidgetVisible(true),
      WaitForWidgetActiveState(false));
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

  ui_manager.Close();
  EXPECT_FALSE(ui_manager.widget()->IsVisible());
  ui_manager.Shutdown();
  EXPECT_FALSE(ui_manager.widget());
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_RetainPositionOnReinvoke DISABLED_RetainPositionOnReinvoke
#else
#define MAYBE_RetainPositionOnReinvoke RetainPositionOnReinvoke
#endif
IN_PROC_BROWSER_TEST_F(OmniboxEverywhereEphemeralBrowserTest,
                       MAYBE_RetainPositionOnReinvoke) {
#if BUILDFLAG(IS_OZONE)
  if (ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP()
        << "Window drag is not supported under Wayland test compositor.";
  }
#endif

  OmniboxEverywhereController* controller =
      g_browser_process->GetFeatures()->omnibox_everywhere_controller();
  ASSERT_TRUE(controller);

  gfx::Point dragged_origin;
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOmniboxWebContentsId);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // TODO(crbug.com/40249472): Modal drag loops in tests aren't supported on
  // MacOS and Windows. Manually set widget bounds to test position retention
  // across re-invocations.
  auto reposition_step = Steps(Do([&]() {
    views::Widget* widget = controller->ui_manager()->widget();
    gfx::Rect target_bounds = widget->GetWindowBoundsInScreen();
    target_bounds.Offset(100, 100);
    widget->SetBounds(target_bounds);
    dragged_origin = target_bounds.origin();
  }));
#else
  const char kContentsView[] = "OmniboxContentsView";
  auto reposition_step = Steps(
      NameOmniboxContentsView(kContentsView),
      DragMouseInView(kContentsView, gfx::Point(10, 5), gfx::Point(110, 5)),
      Do([&]() {
        dragged_origin = controller->ui_manager()
                             ->widget()
                             ->GetWindowBoundsInScreen()
                             .origin();
      }));
#endif

  RunTestSequence(InvokeViaHotkey(), CheckWidgetVisible(true),
                  WaitForOmniboxWebUIReady(kOmniboxWebContentsId),
                  std::move(reposition_step),
                  // Dismiss via hotkey.
                  InvokeViaHotkey(), CheckWidgetVisible(false),
                  // Re-invoke via hotkey.
                  InvokeViaHotkey(), CheckWidgetVisible(true), Check([&]() {
                    return controller->ui_manager()
                               ->widget()
                               ->GetWindowBoundsInScreen()
                               .origin() == dragged_origin;
                  }),
                  InvokeViaHotkey(), CheckWidgetVisible(false));
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_RetainInputTextOnReinvoke DISABLED_RetainInputTextOnReinvoke
#else
#define MAYBE_RetainInputTextOnReinvoke RetainInputTextOnReinvoke
#endif
IN_PROC_BROWSER_TEST_F(OmniboxEverywhereEphemeralBrowserTest,
                       MAYBE_RetainInputTextOnReinvoke) {
  OmniboxEverywhereController* controller =
      g_browser_process->GetFeatures()->omnibox_everywhere_controller();
  ASSERT_TRUE(controller);

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOmniboxWebContentsId);

  const DeepQuery kSearchInputQuery{
      "omnibox-everywhere-app",
      "omnibox-everywhere-omnibox",
      "cr-searchbox-input",
  };
  const DeepQuery kInputElementQuery{
      "omnibox-everywhere-app",
      "omnibox-everywhere-omnibox",
      "cr-searchbox-input",
      "input",
  };

  RunTestSequence(
      InvokeViaHotkey(), CheckWidgetVisible(true),
      WaitForOmniboxWebUIReady(kOmniboxWebContentsId),
      ExecuteJsAt(
          kOmniboxWebContentsId, kSearchInputQuery,
          "el => { "
          "el.setInputText('hello wor'); "
          "el.dispatchEvent(new CustomEvent('searchbox-input-text-updated', "
          "{ detail: { value: 'hello wor', isComposing: false }, bubbles: "
          "true, composed: true })); }"),
      // Dismiss via hotkey.
      InvokeViaHotkey(), CheckWidgetVisible(false),
      // Re-invoke via hotkey.
      InvokeViaHotkey(), CheckWidgetVisible(true),
      CheckJsResultAt(kOmniboxWebContentsId, kInputElementQuery,
                      "el => el.value", "hello wor"),
      InvokeViaHotkey(), CheckWidgetVisible(false));
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_ShowAndDemoteViaHotkey ShowAndDemoteViaHotkey
#else
#define MAYBE_ShowAndDemoteViaHotkey DISABLED_ShowAndDemoteViaHotkey
#endif
IN_PROC_BROWSER_TEST_F(OmniboxEverywherePersistentBrowserTest,
                       MAYBE_ShowAndDemoteViaHotkey) {
  OmniboxEverywhereController* controller =
      g_browser_process->GetFeatures()->omnibox_everywhere_controller();
  ASSERT_TRUE(controller);

  EXPECT_FALSE(controller->IsVisible());

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOmniboxWebContentsId);

  RunTestSequence(
      // First hotkey press: Shows widget and activates.
      InvokeViaHotkey(), CheckWidgetVisible(true),
      WaitForWidgetActiveState(true),
      WaitForOmniboxWebUIReady(kOmniboxWebContentsId),
      // Second hotkey press: Demotes widget (remains visible, but inactive).
      InvokeViaHotkey(), CheckWidgetVisible(true),
      WaitForWidgetActiveState(false),
      // Third hotkey press: Re-activates widget.
      InvokeViaHotkey(), CheckWidgetVisible(true),
      WaitForWidgetActiveState(true));
}

IN_PROC_BROWSER_TEST_F(OmniboxEverywherePersistentBrowserTest,
                       DemoteOnQuerySubmitInPersistentMode) {
  OmniboxEverywhereController* controller =
      g_browser_process->GetFeatures()->omnibox_everywhere_controller();
  ASSERT_TRUE(controller);

  EXPECT_FALSE(controller->IsVisible());

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOmniboxWebContentsId);

  RunTestSequence(
      // Show widget and activate.
      InvokeViaHotkey(), CheckWidgetVisible(true),
      WaitForWidgetActiveState(true),
      WaitForOmniboxWebUIReady(kOmniboxWebContentsId),
      // Submit query via OmniboxEverywhereService.
      Do([this]() {
        auto* service = OmniboxEverywhereServiceFactory::GetForProfile(
            browser()->GetProfile());
        ASSERT_TRUE(service);
        service->OpenUrl(GURL("https://www.google.com/search?q=test"),
                         WindowOpenDisposition::NEW_FOREGROUND_TAB,
                         ui::PAGE_TRANSITION_GENERATED);
      }),
      // In persistent mode, submitting a query should demote the widget
      // (remains visible, but deactivated).
      CheckWidgetVisible(true), WaitForWidgetActiveState(false));
}

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereEphemeralBrowserTest,
                       CloseOnQuerySubmitInEphemeralMode) {
  OmniboxEverywhereController* controller =
      g_browser_process->GetFeatures()->omnibox_everywhere_controller();
  ASSERT_TRUE(controller);

  EXPECT_FALSE(controller->IsVisible());

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOmniboxWebContentsId);

  RunTestSequence(
      // Show widget and activate.
      InvokeViaHotkey(), CheckWidgetVisible(true),
      WaitForWidgetActiveState(true),
      WaitForOmniboxWebUIReady(kOmniboxWebContentsId),
      // Submit query via OmniboxEverywhereService.
      Do([this]() {
        auto* service = OmniboxEverywhereServiceFactory::GetForProfile(
            browser()->GetProfile());
        ASSERT_TRUE(service);
        service->OpenUrl(GURL("https://www.google.com/search?q=test"),
                         WindowOpenDisposition::NEW_FOREGROUND_TAB,
                         ui::PAGE_TRANSITION_GENERATED);
      }),
      // In ephemeral mode, submitting a query should close the widget.
      CheckWidgetVisible(false));
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_StatusIconLifecycle DISABLED_StatusIconLifecycle_
#else
#define MAYBE_StatusIconLifecycle StatusIconLifecycle
#endif
IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest,
                       MAYBE_StatusIconLifecycle) {
  StatusTray* status_tray = g_browser_process->status_tray();
  PrefService* local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state);

  if (!status_tray) {
    GTEST_SKIP() << "StatusTray is not supported on this platform.";
  }

  // Initially background mode pref is false, status icon should not exist.
  EXPECT_FALSE(status_tray->HasStatusIconOfTypeForTesting(
      StatusTray::OMNIBOX_EVERYWHERE_ICON));

  // Enable background mode pref.
  local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, true);
  EXPECT_TRUE(status_tray->HasStatusIconOfTypeForTesting(
      StatusTray::OMNIBOX_EVERYWHERE_ICON));

  // Disable background mode pref.
  local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, false);
  EXPECT_FALSE(status_tray->HasStatusIconOfTypeForTesting(
      StatusTray::OMNIBOX_EVERYWHERE_ICON));
}

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest, BackgroundModeKeepAlive) {
  Profile* profile = browser()->GetProfile();
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  KeepAliveRegistry* keep_alive_registry = KeepAliveRegistry::GetInstance();
  PrefService* local_state = g_browser_process->local_state();
  ASSERT_TRUE(profile_manager);
  ASSERT_TRUE(keep_alive_registry);
  ASSERT_TRUE(local_state);

  // Background mode should initially be disabled.
  EXPECT_FALSE(profile_manager->HasKeepAliveForTesting(
      profile, ProfileKeepAliveOrigin::kOmniboxEverywhere));

  // Enable background mode pref.
  local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, true);

  // Verify both ScopedProfileKeepAlive and ScopedKeepAlive are held.
  EXPECT_TRUE(profile_manager->HasKeepAliveForTesting(
      profile, ProfileKeepAliveOrigin::kOmniboxEverywhere));
  EXPECT_TRUE(keep_alive_registry->IsKeepingAlive());
  EXPECT_TRUE(keep_alive_registry->IsOriginRegistered(
      KeepAliveOrigin::OMNIBOX_EVERYWHERE));

  // Disable background mode pref.
  local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, false);

  // Wait until ScopedProfileKeepAlive is released on UI thread.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !profile_manager->HasKeepAliveForTesting(
        profile, ProfileKeepAliveOrigin::kOmniboxEverywhere);
  }));
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_StatusIconContextMenuOpensSearchSettings \
  DISABLED_StatusIconContextMenuOpensSearchSettings
#else
#define MAYBE_StatusIconContextMenuOpensSearchSettings \
  StatusIconContextMenuOpensSearchSettings
#endif
IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest,
                       MAYBE_StatusIconContextMenuOpensSearchSettings) {
  StatusTray* status_tray = g_browser_process->status_tray();
  if (!status_tray) {
    GTEST_SKIP() << "StatusTray is not supported on this platform.";
  }

  PrefService* local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state);

  GlobalFeatures* features = g_browser_process->GetFeatures();
  ASSERT_TRUE(features);
  auto* controller = features->omnibox_everywhere_controller();
  ASSERT_TRUE(controller);

  // Enable background mode so background_mode_manager is active.
  local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, true);
  ASSERT_TRUE(controller->background_mode_manager());

  auto* delegate = static_cast<StatusIconMenuModel::Delegate*>(
      controller->background_mode_manager());

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInitialTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSettingsTab);

  RunTestSequence(
      InstrumentTab(kInitialTab),
      NavigateWebContents(kInitialTab, GURL(chrome::kChromeUIVersionURL)),
      InstrumentNextTab(kSettingsTab, AnyBrowser()), Do([delegate]() {
        delegate->ExecuteCommand(
            IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_SETTINGS, 0);
      }),
      WaitForWebContentsReady(kSettingsTab,
                              chrome::GetSettingsUrl(chrome::kSearchSubPage)),
      CheckResult([this] { return browser()->tab_strip_model()->count(); }, 2,
                  "CheckTabCount"));

  local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, false);
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_StatusIconContextMenuOpensCustomizeKeyboardShortcut \
  DISABLED_StatusIconContextMenuOpensCustomizeKeyboardShortcut
#else
#define MAYBE_StatusIconContextMenuOpensCustomizeKeyboardShortcut \
  StatusIconContextMenuOpensCustomizeKeyboardShortcut
#endif
IN_PROC_BROWSER_TEST_F(
    OmniboxEverywhereBrowserTest,
    MAYBE_StatusIconContextMenuOpensCustomizeKeyboardShortcut) {
  StatusTray* status_tray = g_browser_process->status_tray();
  if (!status_tray) {
    GTEST_SKIP() << "StatusTray is not supported on this platform.";
  }

  PrefService* local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state);

  GlobalFeatures* features = g_browser_process->GetFeatures();
  ASSERT_TRUE(features);
  auto* controller = features->omnibox_everywhere_controller();
  ASSERT_TRUE(controller);

  // Enable background mode so background_mode_manager is active.
  local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, true);
  ASSERT_TRUE(controller->background_mode_manager());

  auto* delegate = static_cast<StatusIconMenuModel::Delegate*>(
      controller->background_mode_manager());

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInitialTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSettingsTab);

  RunTestSequence(
      InstrumentTab(kInitialTab),
      NavigateWebContents(kInitialTab, GURL(chrome::kChromeUIVersionURL)),
      InstrumentNextTab(kSettingsTab, AnyBrowser()), Do([delegate]() {
        delegate->ExecuteCommand(
            IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_CUSTOMIZE_KEYBOARD_SHORTCUT,
            0);
      }),
      WaitForWebContentsReady(kSettingsTab,
                              chrome::GetSettingsUrl(chrome::kSearchSubPage)),
      CheckResult([this] { return browser()->tab_strip_model()->count(); }, 2,
                  "CheckTabCount"));

  local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, false);
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TargetProfileUpdatesOnBrowserActivation \
  DISABLED_TargetProfileUpdatesOnBrowserActivation
#else
#define MAYBE_TargetProfileUpdatesOnBrowserActivation \
  TargetProfileUpdatesOnBrowserActivation
#endif

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest,
                       MAYBE_TargetProfileUpdatesOnBrowserActivation) {
  Profile* profile1 = browser()->GetProfile();
  ASSERT_TRUE(profile1);

  GlobalFeatures* features = g_browser_process->GetFeatures();
  ASSERT_TRUE(features);
  auto* controller = features->omnibox_everywhere_controller();
  ASSERT_TRUE(controller);

  // Activating the first browser window sets controller's target profile to
  // profile1.
  controller->OnBrowserActivated(browser());
  EXPECT_EQ(profile1, controller->target_profile());

  // Create a secondary profile and browser window.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);
  base::FilePath profile2_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  profiles::testing::CreateProfileSync(profile_manager, profile2_path);
  Profile* profile2 = profile_manager->GetProfile(profile2_path);
  ASSERT_TRUE(profile2);

  BrowserWindowInterface* browser2 = CreateBrowser(profile2);
  ASSERT_TRUE(browser2);

  // Activating browser2 updates controller's target profile to profile2.
  controller->OnBrowserActivated(browser2);
  EXPECT_EQ(profile2, controller->target_profile());

  // Re-activating browser1 updates target profile back to profile1.
  controller->OnBrowserActivated(browser());
  EXPECT_EQ(profile1, controller->target_profile());
}

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest,
                       PRE_RestoresTargetProfileAcrossRestart) {
  Profile* profile = browser()->GetProfile();
  PrefService* local_state = g_browser_process->local_state();
  ASSERT_TRUE(profile);
  ASSERT_TRUE(local_state);

  GlobalFeatures* features = g_browser_process->GetFeatures();
  ASSERT_TRUE(features);
  auto* controller = features->omnibox_everywhere_controller();
  ASSERT_TRUE(controller);

  // Set target profile to profile1 in initial session.
  controller->SetTargetProfile(profile);

  // Verify profile1 path was persisted in local state.
  EXPECT_EQ(local_state->GetFilePath(prefs::kLastTargetProfileDir),
            profile->GetPath());
}

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest,
                       RestoresTargetProfileAcrossRestart) {
  Profile* profile = browser()->GetProfile();
  PrefService* local_state = g_browser_process->local_state();
  ASSERT_TRUE(profile);
  ASSERT_TRUE(local_state);

  GlobalFeatures* features = g_browser_process->GetFeatures();
  ASSERT_TRUE(features);
  auto* controller = features->omnibox_everywhere_controller();
  ASSERT_TRUE(controller);

  // Verify local state persisted profile1 path across restart.
  EXPECT_EQ(local_state->GetFilePath(prefs::kLastTargetProfileDir),
            profile->GetPath());

  // Verify controller restored profile1 as target profile on startup.
  EXPECT_EQ(profile, controller->target_profile());
}

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest,
                       ShutdownWithBackgroundModeEnabled) {
  PrefService* local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state);

  set_exit_when_last_browser_closes(false);

  // Enable background mode pref.
  local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, true);

  // Close the browser window synchronously first. Since background mode is
  // enabled, this should not shut down the browser process.
  CloseBrowserSynchronously(browser());

  // Post a task to quit the browser once the main message loop starts.
  // This ensures that the quit flow executes while the main RunLoop is running,
  // preventing CHECK failures in BrowserProcessImpl::StartTearDown.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&chrome::CloseAllBrowsersAndQuit));
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_OpenUrlCreatesBrowserBeforeClosingPopupWhenNoBrowsers \
  DISABLED_OpenUrlCreatesBrowserBeforeClosingPopupWhenNoBrowsers
#else
#define MAYBE_OpenUrlCreatesBrowserBeforeClosingPopupWhenNoBrowsers \
  OpenUrlCreatesBrowserBeforeClosingPopupWhenNoBrowsers
#endif
IN_PROC_BROWSER_TEST_F(
    OmniboxEverywhereEphemeralBrowserTest,
    MAYBE_OpenUrlCreatesBrowserBeforeClosingPopupWhenNoBrowsers) {
  TestOpenUrlCreatesBrowserWhenNoBrowsers(/*ephemeral=*/true);
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_OpenUrlCreatesBrowserBeforeDemotingPopupWhenNoBrowsers \
  DISABLED_OpenUrlCreatesBrowserBeforeDemotingPopupWhenNoBrowsers
#else
#define MAYBE_OpenUrlCreatesBrowserBeforeDemotingPopupWhenNoBrowsers \
  OpenUrlCreatesBrowserBeforeDemotingPopupWhenNoBrowsers
#endif
IN_PROC_BROWSER_TEST_F(
    OmniboxEverywherePersistentBrowserTest,
    MAYBE_OpenUrlCreatesBrowserBeforeDemotingPopupWhenNoBrowsers) {
  TestOpenUrlCreatesBrowserWhenNoBrowsers(/*ephemeral=*/false);
}

class OmniboxEverywhereCommandLineBrowserTest
    : public OmniboxEverywhereBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OmniboxEverywhereBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kOmniboxEverywhere);
  }
};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_LaunchesWidgetOnStartup LaunchesWidgetOnStartup
#else
#define MAYBE_LaunchesWidgetOnStartup DISABLED_LaunchesWidgetOnStartup
#endif
IN_PROC_BROWSER_TEST_F(OmniboxEverywhereCommandLineBrowserTest,
                       MAYBE_LaunchesWidgetOnStartup) {
  GlobalFeatures* features = g_browser_process->GetFeatures();
  ASSERT_TRUE(features);
  auto* controller = features->omnibox_everywhere_controller();
  ASSERT_TRUE(controller);

  // Verify that the Omnibox Everywhere widget is visible on startup when
  // launched with --omnibox-everywhere.
  EXPECT_TRUE(controller->IsVisible());
  ASSERT_TRUE(controller->target_profile());
  EXPECT_FALSE(controller->target_profile()->IsOffTheRecord());
}

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest,
                       FreModalVisibilityAndDismissal) {
  Profile* profile = browser()->GetProfile();
  ASSERT_TRUE(profile);

  PrefService* profile_prefs = profile->GetPrefs();
  ASSERT_TRUE(profile_prefs);

  // By default, FRE should not be dismissed initially.
  EXPECT_FALSE(profile_prefs->GetBoolean(prefs::kFreDismissed));

  // Dismissing the FRE persists the preference.
  profile_prefs->SetBoolean(prefs::kFreDismissed, true);
  EXPECT_TRUE(profile_prefs->GetBoolean(prefs::kFreDismissed));
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ProfileAvatarUpdatesWhenGAIAPictureLoads \
  DISABLED_ProfileAvatarUpdatesWhenGAIAPictureLoads
#else
#define MAYBE_ProfileAvatarUpdatesWhenGAIAPictureLoads \
  ProfileAvatarUpdatesWhenGAIAPictureLoads
#endif
IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest,
                       MAYBE_ProfileAvatarUpdatesWhenGAIAPictureLoads) {
  Profile* profile = browser()->GetProfile();
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_TRUE(entry);

  // 1. Configure the profile to use a GAIA picture, but simulate cold startup
  // where the GAIA picture has not yet finished loading from cache/disk.
  entry->SetIsUsingGAIAPicture(true);
  entry->SetGAIAPicture(std::string(), gfx::Image());

  GlobalFeatures* features = g_browser_process->GetFeatures();
  ASSERT_TRUE(features);
  auto* controller = features->omnibox_everywhere_controller();
  ASSERT_TRUE(controller);

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOmniboxWebContentsId);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAvatarUpdatedEvent);

  // 2. Compute the initial placeholder avatar data URL.
  gfx::Image initial_icon =
      profiles::GetSizedAvatarIcon(entry->GetAvatarIcon(), 48, 48);
  const std::string initial_avatar_data_url =
      webui::GetBitmapDataUrl(initial_icon.AsBitmap());

  // 3. Create a unique 48x48 GAIA avatar image (solid red) that will be
  // delivered when disk loading finishes.
  SkBitmap gaia_bitmap;
  gaia_bitmap.allocN32Pixels(48, 48);
  gaia_bitmap.eraseColor(SK_ColorRED);
  gfx::Image gaia_image = gfx::Image::CreateFrom1xBitmap(gaia_bitmap);
  const std::string expected_gaia_avatar_data_url =
      webui::GetBitmapDataUrl(gaia_bitmap);

  StateChange avatar_updated_to_gaia;
  avatar_updated_to_gaia.where = {
      "omnibox-everywhere-app",
      "omnibox-everywhere-omnibox",
      "omnibox-everywhere-profile-icon",
      "img#profileIcon",
  };
  avatar_updated_to_gaia.test_function =
      base::StringPrintf(R"((el) => el && el.src === '%s')",
                         expected_gaia_avatar_data_url.c_str());
  avatar_updated_to_gaia.event = kAvatarUpdatedEvent;

  RunTestSequence(
      InvokeViaHotkey(), WaitForOmniboxWebUIReady(kOmniboxWebContentsId),
      CheckJsResult(
          kOmniboxWebContentsId,
          "() => {"
          "  const iconEl = document.querySelector('omnibox-everywhere-app')"
          "      .shadowRoot.querySelector('omnibox-everywhere-omnibox')"
          "      .shadowRoot.querySelector('omnibox-everywhere-profile-icon')"
          "      .shadowRoot.querySelector('img#profileIcon');"
          "  return iconEl ? iconEl.src : '';"
          "}",
          initial_avatar_data_url),
      Do([&]() { entry->SetGAIAPicture("gaia_picture_key", gaia_image); }),
      WaitForStateChange(kOmniboxWebContentsId, avatar_updated_to_gaia));
}

}  // namespace omnibox_everywhere
