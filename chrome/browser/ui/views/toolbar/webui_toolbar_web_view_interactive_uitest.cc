// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"

#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabrestore.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/collaboration/public/features.h"
#include "components/contextual_tasks/public/features.h"
#include "components/data_sharing/public/features.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace {


std::string GetPinnedButtonJS(
    toolbar_ui_api::mojom::PinnedToolbarAction action) {
  return base::StringPrintf(R"(
    ((action) => {
      const app = document.querySelector('toolbar-app');
      if (!app) return null;
      const pinnedToolbarActions =
        app.shadowRoot.querySelector('#pinnedToolbarActions');
      if (!pinnedToolbarActions) return null;
      const container = pinnedToolbarActions.shadowRoot;
      if (!container) return null;
      const actionEl =
        Array.from(container.querySelectorAll('pinned-toolbar-action'))
             .find(el => el.state && el.state.action === action);
      if (!actionEl) return null;
      return actionEl.shadowRoot.querySelector('cr-icon-button');
    })(%d)
  )",
                            static_cast<int>(action));
}

}  // namespace

class WebUIToolbarWebViewInteractiveTest : public InteractiveBrowserTest {
 public:
  WebUIToolbarWebViewInteractiveTest() {
    // Disable kExtensionsPinnedByDefault to prevent test extensions from being
    // pinned automatically upon installation. This avoids a DCHECK crash when
    // the test subsequently tries to pin them manually.
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kWebUILocationBar, features::kWebUIBackForwardButton,
         features::kWebUIAppMenuButton, features::kWebUIPinnedToolbarActions,
         features::kWebUIExtensionsContainer,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2},
        /*disabled_features=*/{features::kExtensionsPinnedByDefault});
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
  }

  auto CheckElementIsCentered(ui::ElementIdentifier webview_id,
                              const DeepQuery& query) {
    return CheckJsResultAt(
        webview_id, query,
        "el => Math.abs(el.getBoundingClientRect().top - (window.innerHeight "
        "- el.getBoundingClientRect().bottom)) <= 1",
        true);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewInteractiveTest,
                       LocationIconOpensPageInfo) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarWebViewId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInstrumentedWebViewId);
  WaitForInitialWebUIToolbar(browser());
  RunTestSequence(
      WaitForShow(kWebUIToolbarElementIdentifier),
      WithView(kWebUIToolbarElementIdentifier,
               [](WebUIToolbarWebView* parent) {
                 parent->GetWebViewForTesting()->SetProperty(
                     views::kElementIdentifierKey, kInstrumentedWebViewId);
               }),
      InstrumentNonTabWebView(kWebUIToolbarWebViewId, kInstrumentedWebViewId,
                              /*wait_for_ready=*/true),
      ExecuteJsAt(
          kWebUIToolbarWebViewId,
          DeepQuery{"toolbar-app", "location-bar", "location-icon", "#button"},
          "el => el.click()"),
      WaitForShow(PageInfoBubbleViewBase::kPageInfoBubbleElementIdentifier));
}

// Verifies the vertical layout alignment of the WebUI toolbar when maximized.
// In maximized/fullscreen mode:
// 1. The WebUIToolbarWebView should remain centered and limited to the content
// height.
// 2. All WebUI elements (Back, Reload, Location Bar, Pinned actions, App Menu)
//    should be vertically centered relative to the WebUI viewport.
// 3. The container itself is centered in the toolbar (y = (parent_height -
// height) / 2 in C++).
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewInteractiveTest,
                       MaximizedLayoutAlignment) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarWebViewId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInstrumentedWebViewId);

  // Make a test extension
  extensions::TestExtensionDir extension_dir;
  extension_dir.WriteManifest(R"({
    "name": "Test Extension",
    "version": "1.0",
    "manifest_version": 3,
    "action": {}
  })");
  extensions::ChromeTestExtensionLoader loader(browser()->GetProfile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Pin the extension to make it visible in the toolbar.
  ToolbarActionsModel::Get(browser()->GetProfile())
      ->SetActionVisibility(extension->id(), true);

  PinnedToolbarActionsModel::Get(browser()->GetProfile())
      ->UpdatePinnedState(kActionPrint, true);
  WaitForInitialWebUIToolbar(browser());

  RunTestSequence(
      WaitForShow(kWebUIToolbarElementIdentifier),
      WithView(kWebUIToolbarElementIdentifier,
               [](WebUIToolbarWebView* parent) {
                 parent->GetWebViewForTesting()->SetProperty(
                     views::kElementIdentifierKey, kInstrumentedWebViewId);
                 parent->GetWidget()->Maximize();
               }),
      // Wait for the window to actually maximize.
      PollUntil(
          [this]() {
            return browser()->GetWindow() &&
                   browser()->GetWindow()->IsMaximized();
          },
          "Wait for window to maximize"),
      // Verify C++ side centering.
      CheckView(kWebUIToolbarElementIdentifier,
                [](WebUIToolbarWebView* view) {
                  return view->bounds().y() ==
                         (view->parent()->height() - view->bounds().height()) /
                             2;
                }),
      InstrumentNonTabWebView(kWebUIToolbarWebViewId, kInstrumentedWebViewId,
                              /*wait_for_ready=*/true),
      // Verify WebUI elements are vertically centered in the viewport.
      CheckElementIsCentered(kWebUIToolbarWebViewId,
                             DeepQuery{"toolbar-app", "#reload"}),
      CheckElementIsCentered(kWebUIToolbarWebViewId,
                             DeepQuery{"toolbar-app", "#back"}),
      CheckElementIsCentered(kWebUIToolbarWebViewId,
                             DeepQuery{"toolbar-app", "#location-bar"}),
      CheckElementIsCentered(kWebUIToolbarWebViewId,
                             DeepQuery{"toolbar-app", "#app-menu"}),
      CheckElementIsCentered(
          kWebUIToolbarWebViewId,
          DeepQuery{"toolbar-app", "#extensions", "webui-toolbar-extension"}),
      CheckElementIsCentered(kWebUIToolbarWebViewId,
                             DeepQuery{"toolbar-app", "#pinnedToolbarActions",
                                       "pinned-toolbar-action"}));
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewInteractiveTest, FocusReloadButton) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarWebViewId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInstrumentedWebViewId);
  WaitForInitialWebUIToolbar(browser());
  RunTestSequence(
      WaitForShow(kWebUIToolbarElementIdentifier),
      WithView(kWebUIToolbarElementIdentifier,
               [](WebUIToolbarWebView* parent) {
                 parent->GetWebViewForTesting()->SetProperty(
                     views::kElementIdentifierKey, kInstrumentedWebViewId);
               }),
      InstrumentNonTabWebView(kWebUIToolbarWebViewId, kInstrumentedWebViewId,
                              /*wait_for_ready=*/true),
      Do([this]() {
        chrome::BrowserCommandController::From(browser())->ExecuteCommand(
            IDC_FOCUS_TOOLBAR);
      }),
      CheckJsResultAt(kWebUIToolbarWebViewId, DeepQuery{},
                      "() => {"
                      "  let active = document.activeElement;"
                      "  while (active && active.shadowRoot && "
                      "active.shadowRoot.activeElement) {"
                      "    active = active.shadowRoot.activeElement;"
                      "  }"
                      "  return active ? active.ariaLabel : null;"
                      "}",
                      l10n_util::GetStringUTF8(IDS_ACCNAME_RELOAD)));
}

// A focus request that could not be delivered (here: the WebView was hidden
// and so not focusable) must be dropped once it is clear that focus is not
// going to come back to the WebView on its own, rather than left pending and
// fired on some unrelated later focus of the WebView.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewInteractiveTest,
                       StaleFocusRequestDropped) {
  WaitForInitialWebUIToolbar(browser());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  WebUIToolbarWebView* const toolbar = GetWebUIToolbarWebView(browser());
  ASSERT_TRUE(toolbar);
  views::WebView* const web_view = toolbar->GetWebViewForTesting();
  ASSERT_TRUE(web_view);
  views::FocusManager* const focus_manager = web_view->GetFocusManager();
  ASSERT_TRUE(focus_manager);

  // Put views focus on some other view, so that the stored focus view is not
  // the WebView either.
  views::View* const contents_view =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->contents_web_view_for_testing();
  ASSERT_TRUE(contents_view);
  contents_view->RequestFocus();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return contents_view->HasFocus(); }));
  ASSERT_FALSE(web_view->HasFocus());

  web_view->SetVisible(false);
  ASSERT_FALSE(web_view->IsFocusable());

  static_cast<WebUIToolbarControlDelegate*>(toolbar)->OnFocusRequested(
      toolbar_ui_api::mojom::FocusRequestTarget::kLocationBar);

  // `RequestFocus()` was a silent no-op, focus is still elsewhere and not set
  // to return to the WebView: the request is stale and gone.
  EXPECT_FALSE(web_view->HasFocus());
  EXPECT_TRUE(contents_view->HasFocus());
  EXPECT_FALSE(toolbar->HasPendingFocusRequestForTesting());

  // Focusing the WebView later must not resurrect the dropped request.
  web_view->SetVisible(true);
  web_view->RequestFocus();
  ASSERT_TRUE(base::test::RunUntil([&]() { return web_view->HasFocus(); }));
  EXPECT_FALSE(toolbar->HasPendingFocusRequestForTesting());
}

// A focus request must survive views focus being merely cleared:
// `FocusManager::ClearFocus()` keeps the WebView as the stored focus view, so
// focus comes back to it (e.g. on the next activation) and the request is
// delivered then.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewInteractiveTest,
                       FocusRequestKeptWhileFocusCleared) {
  WaitForInitialWebUIToolbar(browser());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  WebUIToolbarWebView* const toolbar = GetWebUIToolbarWebView(browser());
  ASSERT_TRUE(toolbar);
  views::WebView* const web_view = toolbar->GetWebViewForTesting();
  ASSERT_TRUE(web_view);
  views::FocusManager* const focus_manager = web_view->GetFocusManager();
  ASSERT_TRUE(focus_manager);

  web_view->RequestFocus();
  ASSERT_TRUE(base::test::RunUntil([&]() { return web_view->HasFocus(); }));
  focus_manager->ClearFocus();
  ASSERT_FALSE(web_view->HasFocus());
  ASSERT_EQ(focus_manager->GetStoredFocusView(), web_view);

  // Make the WebView temporarily unfocusable so that `RequestFocus()` inside
  // `OnFocusRequested()` cannot just grant focus synchronously.
  web_view->SetVisible(false);
  static_cast<WebUIToolbarControlDelegate*>(toolbar)->OnFocusRequested(
      toolbar_ui_api::mojom::FocusRequestTarget::kLocationBar);
  EXPECT_FALSE(web_view->HasFocus());
  EXPECT_TRUE(toolbar->HasPendingFocusRequestForTesting());

  // Focus coming back to the WebView delivers the request.
  web_view->SetVisible(true);
  ASSERT_TRUE(focus_manager->RestoreFocusedView());
  ASSERT_TRUE(web_view->HasFocus());
  EXPECT_FALSE(toolbar->HasPendingFocusRequestForTesting());
}

// If a pending focus request was kept because `web_view` was the stored focus
// view, but before `web_view` regains focus something else replaces or clears
// the stored focus view, the request is stale and must be dropped rather than
// firing on a later unrelated focus of `web_view`.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewInteractiveTest,
                       PendingFocusRequestDroppedWhenStoredFocusLost) {
  WaitForInitialWebUIToolbar(browser());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  WebUIToolbarWebView* const toolbar = GetWebUIToolbarWebView(browser());
  ASSERT_TRUE(toolbar);
  views::WebView* const web_view = toolbar->GetWebViewForTesting();
  ASSERT_TRUE(web_view);
  views::FocusManager* const focus_manager = web_view->GetFocusManager();
  ASSERT_TRUE(focus_manager);
  views::View* const contents_view =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->contents_web_view_for_testing();
  ASSERT_TRUE(contents_view);

  // Queue a pending focus request waiting on stored focus in `web_view`.
  web_view->RequestFocus();
  ASSERT_TRUE(base::test::RunUntil([&]() { return web_view->HasFocus(); }));
  focus_manager->ClearFocus();
  web_view->SetVisible(false);
  static_cast<WebUIToolbarControlDelegate*>(toolbar)->OnFocusRequested(
      toolbar_ui_api::mojom::FocusRequestTarget::kLocationBar);
  web_view->SetVisible(true);
  ASSERT_TRUE(toolbar->HasPendingFocusRequestForTesting());
  ASSERT_EQ(focus_manager->GetStoredFocusView(), web_view);

  // Case 1: Stored focus moves to another view before restoration. Restoring
  // focus to that other view must drop the pending request.
  focus_manager->SetStoredFocusView(contents_view);
  ASSERT_TRUE(focus_manager->RestoreFocusedView());
  EXPECT_TRUE(contents_view->HasFocus());
  EXPECT_FALSE(toolbar->HasPendingFocusRequestForTesting());

  // Case 2: Stored focus is cleared without focusing another view, and later
  // `web_view` is focused directly (e.g. a user click). The stale request must
  // be dropped before `OnInternalWebViewFocused()` runs rather than delivered
  // to the WebUI.
  web_view->RequestFocus();
  ASSERT_TRUE(base::test::RunUntil([&]() { return web_view->HasFocus(); }));
  // Any focus request legitimately delivered to the WebUI so far (e.g. the
  // browser focusing the location bar for the initial about:blank tab once
  // `web_view` first gained focus above) travels over mojo. Make sure the page
  // has processed it before blurring, otherwise it would land after the blur
  // and leave the input focused, making the final check below fail.
  ASSERT_TRUE(FlushWebUIToolbarPipes(browser()));
  // Blur whatever is focused in the page so the final check below starts from
  // an unfocused input. `blur()` drops focus synchronously and nothing in the
  // toolbar WebUI refocuses on blur/focusout, so there is nothing to verify
  // here; if the input were still focused, the final check would catch it.
  ASSERT_TRUE(content::ExecJs(web_view->web_contents(), R"(
    (() => {
      let active = document.activeElement;
      while (active && active.shadowRoot && active.shadowRoot.activeElement) {
        active = active.shadowRoot.activeElement;
      }
      if (active) {
        active.blur();
      }
    })()
  )"));
  focus_manager->ClearFocus();
  web_view->SetVisible(false);
  static_cast<WebUIToolbarControlDelegate*>(toolbar)->OnFocusRequested(
      toolbar_ui_api::mojom::FocusRequestTarget::kLocationBar);
  web_view->SetVisible(true);
  ASSERT_TRUE(toolbar->HasPendingFocusRequestForTesting());

  focus_manager->SetStoredFocusView(nullptr);
  web_view->RequestFocus();
  ASSERT_TRUE(base::test::RunUntil([&]() { return web_view->HasFocus(); }));
  EXPECT_FALSE(toolbar->HasPendingFocusRequestForTesting());
  ASSERT_TRUE(FlushWebUIToolbarPipes(browser()));
  EXPECT_EQ("", content::EvalJs(web_view->web_contents(), R"(
              (() => {
                let active = document.activeElement;
                while (active && active.shadowRoot &&
                       active.shadowRoot.activeElement) {
                  active = active.shadowRoot.activeElement;
                }
                return active && active.id === 'input' ? active.id : '';
              })()
            )"));
}

// A focus request made while the widget is inactive must not be dropped:
// `RequestFocus()` only stores the WebView as the view to focus on activation,
// so the request has to stay pending until the WebView actually gains views
// focus, at which point it is handed to the WebUI.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewInteractiveTest,
                       FocusRequestKeptWhileWidgetInactive) {
#if BUILDFLAG(IS_OZONE)
  // The test re-activates `browser()` after another window took activation.
  // Wayland does not support programmatic window activation, so
  // `BringBrowserWindowToFront()` would never return there.
  if (::ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP() << "Wayland doesn't support programmatic window activation";
  }
#endif

  WaitForInitialWebUIToolbar(browser());

  WebUIToolbarWebView* const toolbar = GetWebUIToolbarWebView(browser());
  ASSERT_TRUE(toolbar);
  views::WebView* const web_view = toolbar->GetWebViewForTesting();
  ASSERT_TRUE(web_view);
  views::FocusManager* const focus_manager = web_view->GetFocusManager();
  ASSERT_TRUE(focus_manager);
  views::Widget* const widget = web_view->GetWidget();
  ASSERT_TRUE(widget);

  // Deactivate the browser window by activating another one. Losing activation
  // stores the focused view and clears views focus
  // (`FocusManager::StoreFocusedView()`), so the WebView loses focus without
  // any explicit `ClearFocus()`. Don't call `FocusManager::ClearFocus()` here:
  // it also clears native focus, which on Windows is `::SetFocus(hwnd)` and
  // re-activates the browser window, restoring focus to the WebView.
  BrowserWindowInterface* const other =
      ui_test_utils::OpenNewEmptyWindowAndWaitUntilActivated(
          browser()->GetProfile());
  ASSERT_TRUE(other);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !widget->IsActive() && !web_view->HasFocus(); }));

  static_cast<WebUIToolbarControlDelegate*>(toolbar)->OnFocusRequested(
      toolbar_ui_api::mojom::FocusRequestTarget::kLocationBar);

  // `RequestFocus()` on an inactive widget stores the view and asks the widget
  // to activate. Activation may complete synchronously on some platforms, in
  // which case the WebView is focused and the request already flushed. Either
  // way the request must not have been dropped: it is pending exactly as long
  // as the WebView has not gained focus yet.
  EXPECT_EQ(toolbar->HasPendingFocusRequestForTesting(), !web_view->HasFocus());
  if (!web_view->HasFocus()) {
    EXPECT_EQ(focus_manager->GetStoredFocusView(), web_view);
  }

  // Once the window is active and the WebView gets focus, the request is
  // delivered and no longer pending.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(base::test::RunUntil([&]() { return web_view->HasFocus(); }));
  EXPECT_FALSE(toolbar->HasPendingFocusRequestForTesting());
}

// Opening a new browser window via the new-window shortcut (Cmd-N on Mac,
// Ctrl-N elsewhere / `IDC_NEW_WINDOW`) requests omnibox focus before the new
// window's toolbar WebUI is initialized and before the window finishes
// activating (and on Mac, across transient native first-responder blurs as the
// WebUI frame swaps in). The pending focus request must survive until the new
// window is active and the toolbar page is ready, leaving the omnibox focused.
// Note: crbug.com/571806192 tracks a separate Mac-specific failure in this
// scenario under additional WebUIToolbar startup/prewarm variations, where
// AppKit's `_selectFirstKeyView` moves focus to `ContentsWebView` while the
// toolbar NSView is still hidden before initial layout.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewInteractiveTest,
                       NewWindowAcceleratorFocusesOmnibox) {
  WaitForInitialWebUIToolbar(browser());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_N, /*control=*/!BUILDFLAG(IS_MAC),
      /*shift=*/false, /*alt=*/false, /*command=*/BUILDFLAG(IS_MAC)));
  BrowserWindowInterface* const new_browser = browser_created_observer.Wait();
  ASSERT_TRUE(new_browser);
  ASSERT_NE(new_browser, browser());

  WaitForInitialWebUIToolbar(new_browser);
  ui_test_utils::WaitUntilBrowserBecomeActive(new_browser);

  WebUIToolbarWebView* const new_toolbar = GetWebUIToolbarWebView(new_browser);
  ASSERT_TRUE(new_toolbar);
  views::WebView* const new_web_view = new_toolbar->GetWebViewForTesting();
  ASSERT_TRUE(new_web_view);

  ui_test_utils::WaitForViewFocus(new_browser, VIEW_ID_OMNIBOX, true);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(new_browser, VIEW_ID_OMNIBOX));
  EXPECT_TRUE(new_web_view->HasFocus());
  // Views focus is granted before the new window's toolbar page is
  // initialized, and `WaitForInitialWebUIToolbar()` only waits while the
  // window's show is deferred, so the page may still be initializing here.
  // The request is handed to the WebUI once it is.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !new_toolbar->HasPendingFocusRequestForTesting(); }));
  EXPECT_TRUE(new_web_view->HasFocus());
  ASSERT_TRUE(FlushWebUIToolbarPipes(new_browser));
  EXPECT_EQ("input", content::EvalJs(new_web_view->web_contents(), R"(
              (() => {
                let active = document.activeElement;
                while (active && active.shadowRoot &&
                       active.shadowRoot.activeElement) {
                  active = active.shadowRoot.activeElement;
                }
                return active ? active.id : null;
              })()
            )"));
}

class WebUIPinnedToolbarActionsInteractiveTest : public InteractiveBrowserTest {
 public:
  WebUIPinnedToolbarActionsInteractiveTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIPinnedToolbarActions,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2,
         features::kWebUIReloadButton},
        {});
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    // Set tested actions as pinnable.
    auto* action_item = actions::ActionManager::Get().FindAction(
        kActionPrint, BrowserActions::From(browser())->root_action_item());
    if (action_item) {
      action_item->SetProperty(
          actions::kActionItemPinnableKey,
          static_cast<int>(actions::ActionPinnableState::kPinnable));
    }
    action_item = actions::ActionManager::Get().FindAction(
        kActionSidePanelShowBookmarks,
        BrowserActions::From(browser())->root_action_item());
    if (action_item) {
      action_item->SetProperty(
          actions::kActionItemPinnableKey,
          static_cast<int>(actions::ActionPinnableState::kPinnable));
    }

    model_ = PinnedToolbarActionsModel::Get(browser()->GetProfile());
  }

  void TearDownOnMainThread() override {
    model_ = nullptr;
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 protected:
  raw_ptr<PinnedToolbarActionsModel> model_ = nullptr;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsInteractiveTest,
                       RightClickPinnedAction) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarWebViewId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInstrumentedWebViewId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kMenuRunningState);

  actions::ActionId action_id1 = kActionPrint;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action1 =
      toolbar_ui_api::mojom::PinnedToolbarAction::kPrint;

  actions::ActionId action_id2 = kActionSidePanelShowBookmarks;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action2 =
      toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowBookmarks;

  RunTestSequence(
      WaitForShow(kWebUIToolbarElementIdentifier),
      WithView(kWebUIToolbarElementIdentifier,
               [](WebUIToolbarWebView* parent) {
                 parent->GetWebViewForTesting()->SetProperty(
                     views::kElementIdentifierKey, kInstrumentedWebViewId);
               }),
      InstrumentNonTabWebView(kWebUIToolbarWebViewId, kInstrumentedWebViewId,
                              /*wait_for_ready=*/true),
      Do([this, action_id1, action_id2]() {
        model_->UpdatePinnedState(action_id1, true);
        model_->UpdatePinnedState(action_id2, true);
      }),
      WaitForJsResult(
          kWebUIToolbarWebViewId,
          base::StringPrintf(R"(
            () => {
              const btn1 = %s;
              const btn2 = %s;
              return !!btn1 && btn1.checkVisibility() &&
                     !!btn2 && btn2.checkVisibility();
            }
          )",
                             GetPinnedButtonJS(mojom_action1).c_str(),
                             GetPinnedButtonJS(mojom_action2).c_str()),
          true),

      PollState(kMenuRunningState,
                [this]() {
                  WebUIToolbarWebView* webui_toolbar_view =
                      GetWebUIToolbarWebView(browser());
                  WebUIPinnedToolbarActions* actions =
                      webui_toolbar_view->GetPinnedToolbarActions();
                  return actions->menu_runner_ &&
                         actions->menu_runner_->IsRunning();
                }),

      // Open context menu for first action.
      ExecuteJsAt(kWebUIToolbarWebViewId, DeepQuery{},
                  base::StringPrintf(R"(
        () => {
          const btn = %s;
          btn.dispatchEvent(
            new MouseEvent(
              'contextmenu', {button: 2, bubbles: true, composed: true}
            )
          );
        }
      )",
                                     GetPinnedButtonJS(mojom_action1).c_str())),

      // Wait for the menu to be running.
      WaitForState(kMenuRunningState, true),

      // Verify first action is highlighted.
      WaitForJsResult(
          kWebUIToolbarWebViewId,
          base::StringPrintf(R"(
            () => {
              const btn = %s;
              return !!btn && btn.hasAttribute('is-menu-open');
            }
          )",
                             GetPinnedButtonJS(mojom_action1).c_str()),
          true),

      // Open context menu for second action.
      ExecuteJsAt(kWebUIToolbarWebViewId, DeepQuery{},
                  base::StringPrintf(R"(
        () => {
          const btn = %s;
          btn.dispatchEvent(
            new MouseEvent(
              'contextmenu', {button: 2, bubbles: true, composed: true}
            )
          );
        }
      )",
                                     GetPinnedButtonJS(mojom_action2).c_str())),

      // Wait for first action to NOT be highlighted.
      WaitForJsResult(
          kWebUIToolbarWebViewId,
          base::StringPrintf(R"(
            () => {
              const btn = %s;
              return !!btn && !btn.hasAttribute('is-menu-open');
            }
          )",
                             GetPinnedButtonJS(mojom_action1).c_str()),
          true),

      // Verify second action is highlighted.
      WaitForJsResult(
          kWebUIToolbarWebViewId,
          base::StringPrintf(R"(
            () => {
              const btn = %s;
              return !!btn && btn.hasAttribute('is-menu-open');
            }
          )",
                             GetPinnedButtonJS(mojom_action2).c_str()),
          true),

      // Clean menu
      Do([this]() {
        WebUIToolbarWebView* webui_toolbar_view =
            GetWebUIToolbarWebView(browser());
        webui_toolbar_view->GetPinnedToolbarActions()->menu_runner_->Cancel();
      }),

      // Verify second action is not highlighted.
      WaitForJsResult(
          kWebUIToolbarWebViewId,
          base::StringPrintf(R"(
            () => {
              const btn = %s;
              return !!btn && !btn.hasAttribute('is-menu-open');
            }
          )",
                             GetPinnedButtonJS(mojom_action2).c_str()),
          true),

      // Clean up pinned state
      Do([this, action_id1, action_id2]() {
        model_->UpdatePinnedState(action_id1, false);
        model_->UpdatePinnedState(action_id2, false);
      }));
}

// Verifies that restoring a background WebUI tab and switching to it does not
// cause the WebUI toolbar to be occluded or freeze its Omnibox location
// updates. See b/547718312.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewInteractiveTest,
                       TabSwitchStaleBoundsOccludesWebUIToolbar) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarWebViewId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInstrumentedWebViewId);

  WaitForInitialWebUIToolbar(browser());

  // Restore a background WebUI tab (e.g. chrome://version).
  const GURL webui_url("chrome://version/");
  std::vector<sessions::SerializedNavigationEntry> navigations;
  sessions::SerializedNavigationEntry entry;
  entry.set_virtual_url(webui_url);
  entry.set_index(0);
  navigations.push_back(entry);

  content::WebContents* restored_contents = chrome::AddRestoredTab(
      browser(), navigations, /*tab_index=*/1, /*selected_navigation=*/0,
      /*extension_app_id=*/"", /*group=*/std::nullopt, /*select=*/false,
      /*pin=*/false, base::TimeTicks::Now(), base::Time::Now(),
      /*storage_namespace=*/nullptr,
      sessions::SerializedUserAgentOverride(),
      /*extra_data=*/{}, /*from_session_restore=*/true,
      /*is_active_browser=*/std::nullopt);
  ASSERT_TRUE(restored_contents);
  EXPECT_EQ(2, browser()->GetTabStripModel()->count());
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());

  RunTestSequence(
      WaitForShow(kWebUIToolbarElementIdentifier),
      WithView(kWebUIToolbarElementIdentifier,
               [](WebUIToolbarWebView* parent) {
                 parent->GetWebViewForTesting()->SetProperty(
                     views::kElementIdentifierKey, kInstrumentedWebViewId);
               }),
      InstrumentNonTabWebView(kWebUIToolbarWebViewId, kInstrumentedWebViewId,
                              /*wait_for_ready=*/true),
      // Switch to the restored WebUI background tab.
      Do([this]() { browser()->GetTabStripModel()->ActivateTabAt(1); }),
      // Verify that the WebUI Location Bar receives the update and displays the
      // new URL.
      WaitForJsResultAt(kWebUIToolbarWebViewId,
                        DeepQuery{"toolbar-app", "#location-bar", "#omnibox",
                                  "#textInput", "#input"},
                        "el => el.value.includes('chrome://version')", true));
}

#if BUILDFLAG(IS_MAC)
// Verifies the toolbar state tells the WebUI to mute icons while the window is
// inactive. Only Mac does this (see
// views::PlatformStyle::kInactiveWidgetControlsAppearDisabled). This also
// can't run on e.g. Wayland, which doesn't allow activating windows
// programmatically.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewInteractiveTest,
                       ControlsAppearInactiveFollowsWindowActivation) {
  static_assert(views::PlatformStyle::kInactiveWidgetControlsAppearDisabled);
  WaitForInitialWebUIToolbar(browser());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  views::Widget* const widget =
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  BrowserWindowInterface* const other_browser =
      CreateBrowser(browser()->GetProfile());

  auto check_inactive = [this](bool expected) {
    return CheckView(kWebUIToolbarElementIdentifier,
                     [expected](WebUIToolbarWebView* view) {
                       view->FlushPendingToolbarStateForTesting();
                       return static_cast<WebUIToolbarControlDelegate*>(view)
                                  ->GetState()
                                  .controls_appear_inactive == expected;
                     });
  };

  RunTestSequence(
      WaitForShow(kWebUIToolbarElementIdentifier), Do([this]() {
        ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
      }),
      check_inactive(false), Do([other_browser]() {
        ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(other_browser));
      }),
      PollUntil([widget]() { return !widget->ShouldPaintAsActive(); },
                "Wait for the browser window to paint as inactive"),
      check_inactive(true), Do([this]() {
        ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
      }),
      PollUntil([widget]() { return widget->ShouldPaintAsActive(); },
                "Wait for the browser window to paint as active"),
      check_inactive(false));
}
#else
// Native toolbar buttons don't change when the window is inactive on platforms
// other than Mac, so the WebUI must never be told to mute icons.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewInteractiveTest,
                       ControlsNeverAppearInactive) {
  static_assert(!views::PlatformStyle::kInactiveWidgetControlsAppearDisabled);
  WaitForInitialWebUIToolbar(browser());
  RunTestSequence(
      WaitForShow(kWebUIToolbarElementIdentifier),
      CheckView(kWebUIToolbarElementIdentifier, [](WebUIToolbarWebView* view) {
        view->FlushPendingToolbarStateForTesting();
        return !static_cast<WebUIToolbarControlDelegate*>(view)
                    ->GetState()
                    .controls_appear_inactive;
      }));
}
#endif  // BUILDFLAG(IS_MAC)

class WebUIToolbarWebViewDeferStartupLayoutInteractiveTest
    : public WebUIToolbarWebViewInteractiveTest {
 public:
  WebUIToolbarWebViewDeferStartupLayoutInteractiveTest() {
    feature_list_.InitAndEnableFeature(
        features::kDeferLayoutDuringBrowserStartup);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Regression test for https://crbug.com/571806192: opening a new window should
// focus the WebUI toolbar even when layout is deferred while the window is
// invisible. On Mac, the window is activated (and initial focus is restored)
// before it is reported as visible, so the toolbar must already be laid out by
// then.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewDeferStartupLayoutInteractiveTest,
                       NewWindowFocusesToolbar) {
  ui_test_utils::BrowserCreatedObserver browser_created;
  chrome::ExecuteCommand(browser(), IDC_NEW_WINDOW);
  BrowserWindowInterface* const new_browser = browser_created.Wait();
  ui_test_utils::WaitUntilBrowserBecomeActive(new_browser);

  views::WebView* const toolbar_web_view =
      BrowserView::GetBrowserViewForBrowser(new_browser)
          ->toolbar()
          ->GetWebUIToolbarViewForTesting()
          ->GetWebViewForTesting();
  EXPECT_TRUE(toolbar_web_view->HasFocus());
}
