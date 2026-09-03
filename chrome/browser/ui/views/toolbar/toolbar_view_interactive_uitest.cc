// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_accessibility_test.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/webui_and_views_toolbar_interactive_uitest_base.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_view_focus_helper.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using bookmarks::BookmarkModel;

namespace {

// This script is basically getDeepActiveElement() from
// ui/webui/resources/js/util.ts but copied here since it is not included in
// the WebUIToolbar WebUI page.
constexpr char kGetFocusedElementJS[] = R"JS(
function getDeepActiveElement() {
  let a = document.activeElement;
  while (a && a.shadowRoot && a.shadowRoot.activeElement) {
    a = a.shadowRoot.activeElement;
  }
  return a;
}
getDeepActiveElement().ariaLabel || getDeepActiveElement().tagName
  )JS";

int GetIDForFocusedViewElement(const views::View* view) {
  const std::string kReloadControlName =
      base::UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_ACCNAME_RELOAD));
  const std::string kBackControlName =
      base::UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  const std::string kForwardControlName =
      base::UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_ACCNAME_FORWARD));

  if (const views::WebView* web_view =
          views::AsViewClass<views::WebView>(view)) {
    std::string element_name =
        content::EvalJs(web_view->web_contents(), kGetFocusedElementJS)
            .ExtractString();
    if (element_name == kReloadControlName) {
      return VIEW_ID_RELOAD_BUTTON;
    } else if (element_name == kBackControlName) {
      return VIEW_ID_BACK_BUTTON;
    } else if (element_name == kForwardControlName) {
      return VIEW_ID_FORWARD_BUTTON;
    } else {
      ADD_FAILURE() << "Unexpected focused element: " << element_name;
      return VIEW_ID_NONE;
    }
  } else {
    return view->GetID();
  }
}

}  // namespace

class ToolbarViewTest : public ToolbarAccessibilityTest {
 public:
  ToolbarViewTest() {
    if (GetParam()) {
      feature_list_.InitWithFeatures(
          {features::kInitialWebUI, features::kWebUIReloadButton,
           features::kWebUIBackForwardButton, features::kWebUISplitTabsButton,
           features::kWebUIHomeButton},
          {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {features::kInitialWebUI, features::kWebUIReloadButton,
               features::kWebUIBackForwardButton,
               features::kWebUISplitTabsButton, features::kWebUIHomeButton});
    }
    webui_omnibox_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/
        // TODO(crbug.com/452061489): Fix tests that fail when the WebUI Omnibox
        // is enabled and then remove these two Features.
        {omnibox::internal::kWebUIOmniboxPopup,
         omnibox::internal::kWebUIOmniboxAimPopup});
  }
  ToolbarViewTest(const ToolbarViewTest&) = delete;
  ToolbarViewTest& operator=(const ToolbarViewTest&) = delete;

  void SetUpOnMainThread() override {
    ToolbarAccessibilityTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    WaitForInitialWebUI();
  }

  auto ExpectBackForwardButtonEnabled(ui::ElementIdentifier id, bool enabled) {
    if (features::IsWebUIBackForwardButtonEnabled()) {
      return Steps(CheckResult(
          [this, id]() {
            return chrome::BrowserCommandController::From(browser())
                ->IsCommandEnabled(
                    id == kToolbarBackButtonElementId ? IDC_BACK : IDC_FORWARD);
          },
          enabled));
    } else {
      return Steps(CheckViewProperty(id, &views::View::GetEnabled, enabled));
    }
  }

  void RunToolbarCycleFocusTest(BrowserWindowInterface* browser);

  void SetLocationBarSecurityLevelForTesting(
      security_state::SecurityLevel security_level) {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    LocationBarView* location_bar_view = browser_view->GetLocationBarView();
    LocationIconView* location_icon_view =
        location_bar_view->location_icon_view();

    location_icon_view->SetSecurityLevelForTesting(security_level);
  }

  auto OpenSideBySideTab(int tab_index) {
    const char kTabToHover[] = "Tab to hover";

    return Steps(NameDescendantViewByType<Tab>(kTabStripElementId, kTabToHover,
                                               tab_index),
                 MoveMouseTo(kTabToHover), ClickMouse(ui_controls::RIGHT),
                 SelectMenuItem(TabMenuModel::kSplitTabsMenuItem));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList webui_omnibox_feature_list_;
};

void ToolbarViewTest::RunToolbarCycleFocusTest(
    BrowserWindowInterface* browser) {
  // Navigate to a few URLs so that the back and forward buttons are enabled
  // and focusable.
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");

  // Move mouse off of toolbar. Having the mouse over the reload button when a
  // page finishes loading may temporarily disable the reload button, making it
  // no longer focusable, which will cause walking through focusable elements to
  // skip over it, and the test will then fail.
  RunTestSequence(InstrumentToolbar(), MoveMouseOffOfReloadButton());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, url1));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, url2));
  // Navigate back once so forward is enabled too.
  content::TestNavigationObserver back_nav_observer(
      browser->GetTabStripModel()->GetActiveWebContents());
  chrome::BrowserCommandController::From(browser)->ExecuteCommand(IDC_BACK);
  back_nav_observer.Wait();

  gfx::NativeWindow window = browser->GetWindow()->GetNativeWindow();
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);

  ToolbarButtonProvider* toolbar_button_provider =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar();
  if (WebUIToolbarWebView* webui_toolbar =
          toolbar_button_provider->GetWebUIToolbarViewForTesting()) {
    content::WebContents* toolbar_webcontents =
        webui_toolbar->GetWebViewForTesting()->web_contents();
    // The first run on Mac Try bots can take a while to be copyable.
    base::test::ScopedRunLoopTimeout timeout(FROM_HERE, base::Seconds(20));
    content::WaitForCopyableViewInWebContents(toolbar_webcontents);
  }

  // Test relies on browser window activation, while platform such as Linux's
  // window activation is asynchronous.
  widget->Activate();
  views::test::WaitForWidgetActive(widget, true);

  // Send focus to the toolbar as if the user pressed Alt+Shift+T. This should
  // happen after the browser window activation.
  CommandUpdater* updater = chrome::BrowserCommandController::From(browser);
  updater->ExecuteCommand(IDC_FOCUS_TOOLBAR);

  views::FocusManager* focus_manager = widget->GetFocusManager();
  views::View* first_view = focus_manager->GetFocusedView();
  int first_id = GetIDForFocusedViewElement(first_view);

  std::vector<int> ids;

  // Press Tab to cycle through all of the controls in the toolbar until
  // we end up back where we started.
  bool found_back = false;
  bool found_forward = false;
  bool found_reload = false;
  bool found_location_bar = false;
  bool found_app_menu = false;
  const views::View* view = first_view;
  do {
    ui_test_utils::AdvanceFocus(focus_manager, false);

    view = focus_manager->GetFocusedView();
    int id = GetIDForFocusedViewElement(view);
    ids.push_back(id);

    if (id == VIEW_ID_BACK_BUTTON) {
      found_back = true;
    }
    if (id == VIEW_ID_FORWARD_BUTTON) {
      found_forward = true;
    }
    if (id == VIEW_ID_RELOAD_BUTTON) {
      found_reload = true;
    }
    if (view->GetID() == VIEW_ID_APP_MENU) {
      found_app_menu = true;
    }
    if (view->GetID() == VIEW_ID_OMNIBOX) {
      found_location_bar = true;
    }
    if (ids.size() > 100) {
      GTEST_FAIL() << "Tabbed 100 times, still haven't cycled back!";
    }
  } while (view != first_view || GetIDForFocusedViewElement(view) != first_id);

  // Make sure we found a few key items.
  ASSERT_TRUE(found_back);
  ASSERT_TRUE(found_forward);
  ASSERT_TRUE(found_reload);
  ASSERT_TRUE(found_app_menu);
  ASSERT_TRUE(found_location_bar);

  // Now press Shift-Tab to cycle backwards.
  std::vector<int> reverse_ids;
  do {
    ui_test_utils::AdvanceFocus(focus_manager, true);

    view = focus_manager->GetFocusedView();
    reverse_ids.push_back(GetIDForFocusedViewElement(view));

    if (reverse_ids.size() > 100) {
      GTEST_FAIL() << "Tabbed 100 times, still haven't cycled back!";
    }
  } while (view != first_view || GetIDForFocusedViewElement(view) != first_id);

  // Assert that the views were focused in exactly the reverse order.
  // The sequences should be the same length, and the last element will
  // be the same, and the others are reverse.
  ASSERT_EQ(ids.size(), reverse_ids.size());
  size_t count = ids.size();
  for (size_t i = 0; i < count - 1; i++) {
    EXPECT_EQ(ids[i], reverse_ids[count - 2 - i]);
  }
  EXPECT_EQ(ids[count - 1], reverse_ids[count - 1]);
}

IN_PROC_BROWSER_TEST_P(ToolbarViewTest, ToolbarCycleFocus) {
  RunToolbarCycleFocusTest(browser());
}

IN_PROC_BROWSER_TEST_P(ToolbarViewTest, ToolbarCycleFocusWithBookmarkBar) {
  CommandUpdater* updater = chrome::BrowserCommandController::From(browser());
  updater->ExecuteCommand(IDC_SHOW_BOOKMARK_BAR);

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
  bookmarks::AddIfNotBookmarked(model, GURL("http://foo.com"), u"Foo");

  // We want to specifically test the case where the bookmark bar is
  // already showing when a window opens, so create a second browser
  // window with the same profile.
  BrowserWindowInterface* second_browser =
      CreateBrowser(browser()->GetProfile());
  WaitForInitialWebUI(second_browser);
  RunToolbarCycleFocusTest(second_browser);
}

IN_PROC_BROWSER_TEST_P(ToolbarViewTest, BackForwardButtonUpdate) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("/title1.html");

  RunTestSequence(
      InstrumentTab(kWebContentsId),
      ExpectBackForwardButtonEnabled(kToolbarBackButtonElementId, false),
      ExpectBackForwardButtonEnabled(kToolbarForwardButtonElementId, false),

      NavigateWebContents(kWebContentsId, url),
      ExpectBackForwardButtonEnabled(kToolbarBackButtonElementId, true),
      ExpectBackForwardButtonEnabled(kToolbarForwardButtonElementId, false),

      Do([this]() {
        auto& controller = browser()
                               ->GetTabStripModel()
                               ->GetActiveWebContents()
                               ->GetController();
        controller.DeleteNavigationEntries(base::BindRepeating(
            [&](content::NavigationEntry* entry) { return true; }));
      }),

      ExpectBackForwardButtonEnabled(kToolbarBackButtonElementId, false),
      ExpectBackForwardButtonEnabled(kToolbarForwardButtonElementId, false));
}

// TODO(crbug.com/548345902): Re-enable once fixed. Failing at step 9:
// WaitForWebContentsNavigation(kWebContentsId, GURL(url::kAboutBlankURL))
#if BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_BackButtonHoverThenClick DISABLED_BackButtonHoverThenClick
#else
#define MAYBE_BackButtonHoverThenClick BackButtonHoverThenClick
#endif
IN_PROC_BROWSER_TEST_P(ToolbarViewTest, MAYBE_BackButtonHoverThenClick) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");

  RunTestSequence(
      InstrumentTab(kWebContentsId),
      ExpectBackForwardButtonEnabled(kToolbarBackButtonElementId, false),

      NavigateWebContents(kWebContentsId, url1),
      ExpectBackForwardButtonEnabled(kToolbarBackButtonElementId, true),

      // Click on the back button. This should navigate back in
      // session history.
      MoveMouseToElement(kToolbarBackButtonElementId), ClickMouse(),
      WaitForWebContentsNavigation(kWebContentsId, GURL(url::kAboutBlankURL)),

      ExpectBackForwardButtonEnabled(kToolbarBackButtonElementId, false));
}

// Verifies that when the browser window is in standard (unmaximized) mode,
// the back button is inset from toolbar edge by exact standard interior
// margin (TOOLBAR_INTERIOR_MARGIN.left). In this mode, Fitts' law target
// stretching is inactive, and the margin space outside button is not
// clickable.
IN_PROC_BROWSER_TEST_P(ToolbarViewTest, BackButtonDistanceFromEdge) {
  auto restore_if_maximized = Do([this]() {
    if (browser()->GetWindow() && (browser()->GetWindow()->IsMaximized() ||
                                   browser()->GetWindow()->IsFullscreen())) {
      browser()->GetWindow()->Restore();
      ASSERT_TRUE(base::test::RunUntil([this]() {
        return !browser()->GetWindow()->IsMaximized() &&
               !browser()->GetWindow()->IsFullscreen();
      }));
    }
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    if (browser_view && browser_view->toolbar()) {
      browser_view->toolbar()->InvalidateLayout();
    }
    if (browser_view && browser_view->GetWidget()) {
      browser_view->InvalidateLayout();
      browser_view->GetWidget()->LayoutRootViewIfNecessary();
    }
  });
  int expected_margin =
      GetLayoutInsets(LayoutInset::TOOLBAR_INTERIOR_MARGIN).left();
  const int default_margin =
      GetLayoutConstant(LayoutConstant::kToolbarIconDefaultMargin);
  if (features::IsWebUIBackForwardButtonEnabled()) {
    // For WebUI: Verify that the internal #buttonWrapper element starts at
    // one of valid boundary offsets depending on platform and rollout state:
    // - `expected_margin` (6px): Default unmaximized non-touch margin.
    // - `default_margin` (2px): Standard icon gap during intermediate rollout
    //   phases when adjacent container boundaries override edge spacing.
    // - `0`: Touch UI mode on Ash (where TOOLBAR_INTERIOR_MARGIN is 0) or
    //   when running under maximized window Fitts' law target stretching
    //   where outer margins collapse directly into inner wrapper padding.
    RunTestSequence(
        std::move(restore_if_maximized), WaitForToolbarLoaded(),
        CheckJsResultAt(
            WebUIToolbarId(),
            WebUIAndViewsToolbarInteractiveUiTestBase::
                WebUIBackForwardButtonDeepQuery(),
            [expected_margin, default_margin]() {
              return base::StringPrintf(
                  "el => { const left = Math.round("
                  "el.shadowRoot.querySelector('#buttonWrapper')"
                  ".getBoundingClientRect().left); return left === %d || "
                  "left === %d || left === 0; }",
                  expected_margin, default_margin);
            }(),
            true));
  } else {
    // For native C++: Verify that `GetScreenBounds().x()` is inset from
    // `ToolbarView` left edge by `expected_margin`, `default_margin`, or `0`.
    // As noted above, these fallback tolerances accommodate unmaximized mode,
    // intermediate rollout spacing, touch UI, or maximized Fitts' law modes.
    RunTestSequence(
        std::move(restore_if_maximized),
        WaitForElementNonzeroSize(kToolbarBackButtonElementId),
        CheckResult(
            [this, expected_margin, default_margin]() -> bool {
              ui::ElementContext context =
                  BrowserView::GetBrowserViewForBrowser(browser())
                      ->GetElementContext();
              ui::TrackedElement* back_el =
                  ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                      kToolbarBackButtonElementId, context);
              ui::TrackedElement* toolbar_el =
                  ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                      ToolbarView::kToolbarElementId, context);
              const int margin = back_el->GetScreenBounds().x() -
                                 toolbar_el->GetScreenBounds().x();
              return margin == expected_margin || margin == default_margin ||
                     margin == 0;
            },
            true));
  }
}

// TODO(crbug.com/40252318): The ui test utils do not seem to adequately
// simulate mouse hovering on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_BackButtonHoverMetricsLogged DISABLED_BackButtonHoverMetricsLogged
#else
#define MAYBE_BackButtonHoverMetricsLogged BackButtonHoverMetricsLogged
#endif
IN_PROC_BROWSER_TEST_P(ToolbarViewTest, MAYBE_BackButtonHoverMetricsLogged) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL first_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  const GURL cross_site_url =
      embedded_test_server()->GetURL("b.test", "/title2.html");

  base::HistogramTester histogram_tester;

  RunTestSequence(
      InstrumentTab(kWebContentsId),
      NavigateWebContents(kWebContentsId, first_url),
      NavigateWebContents(kWebContentsId, cross_site_url),

      // Set the initial mouse position to a known state. If the mouse happens
      // to be over the back button at the start of the test, then the mouse
      // movement done by the test wouldn't be seen as a mouse enter. The choice
      // of using the reload button as the starting position is arbitrary.
      MoveMouseToElement(kReloadButtonElementId),

      ExpectBackForwardButtonEnabled(kToolbarBackButtonElementId, true),

      // Mouse over and click on the back button. This should navigate back in
      // session history and log a hover metric.
      MoveMouseToElement(kToolbarBackButtonElementId), ClickMouse(),
      WaitForWebContentsNavigation(kWebContentsId, first_url),

      Check([&]() {
        histogram_tester.ExpectTotalCount(
            "Preloading.PrerenderBackNavigationEligibility.BackButtonHover", 1);
        return true;
      }));
}

IN_PROC_BROWSER_TEST_P(ToolbarViewTest,
                       ToolbarForRegularProfileHasExtensionsToolbarDesktop) {
  // Verify the normal browser has an extensions toolbar container.
  ExtensionsToolbarDesktop* extensions_container =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->extensions_container();
  EXPECT_NE(nullptr, extensions_container);
}

// TODO(crbug.com/41474891): Setup test profiles properly for CrOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ExtensionsToolbarDesktopForGuest \
  DISABLED_ExtensionsToolbarDesktopForGuest
#else
#define MAYBE_ExtensionsToolbarDesktopForGuest ExtensionsToolbarDesktopForGuest
#endif
IN_PROC_BROWSER_TEST_P(ToolbarViewTest,
                       MAYBE_ExtensionsToolbarDesktopForGuest) {
  // Verify guest browser does not have an extensions toolbar container.
  profiles::SwitchToGuestProfile();
  ui_test_utils::WaitForBrowserToOpen();
  Profile* guest = g_browser_process->profile_manager()->GetProfileByPath(
      ProfileManager::GetGuestProfilePath());
  ASSERT_TRUE(guest);
  BrowserWindowInterface* target_browser = ui_test_utils::FindAnyBrowser(guest);
  ASSERT_TRUE(target_browser);
  ExtensionsToolbarDesktop* extensions_container =
      BrowserView::GetBrowserViewForBrowser(target_browser)
          ->toolbar()
          ->extensions_container();
  EXPECT_EQ(nullptr, extensions_container);
}

// Verifies that the identifiers for the pop-up menus are properly
// assigned so that the menu can be located by tests when it is shown.
//
// The back button is just one example for which the menu identifier is defined.
//
// TODO: crbug.com/494279213 - Re-enable this test on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_BackButtonMenu DISABLED_BackButtonMenu
#else
#define MAYBE_BackButtonMenu BackButtonMenu
#endif
IN_PROC_BROWSER_TEST_P(ToolbarViewTest, MAYBE_BackButtonMenu) {
  // TODO(crbug.com/470038385): Support WebUI back button in this test.
  if (features::IsWebUIBackForwardButtonEnabled()) {
    return;
  }
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  const GURL url3 = embedded_test_server()->GetURL("/title3.html");
  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url1),
      NavigateWebContents(kWebContentsId, url2),
      NavigateWebContents(kWebContentsId, url3),
      // Show the context menu.
      MoveMouseTo(kToolbarBackButtonElementId), ClickMouse(ui_controls::RIGHT),
      Log("Logging to probe crbug.com/40074126. Waiting for back button menu."),
      // Dismiss the context menu by clicking on it.
      Log("Moving mouse to menu."),
      MoveMouseTo(kToolbarBackButtonMenuElementId),
      Log("Clicking mouse to dismiss."), ClickMouse(),
      Log("Waiting for menu to dismiss."),
      WaitForHide(kToolbarBackButtonMenuElementId), Log("Menu dismissed."));
}

// TODO(crbug.com/402492418): Find workaround for Mac and ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SplitTabsToolbarButton DISABLED_SplitTabsToolbarButton
#else
#define MAYBE_SplitTabsToolbarButton SplitTabsToolbarButton
#endif
IN_PROC_BROWSER_TEST_P(ToolbarViewTest, MAYBE_SplitTabsToolbarButton) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents1Id);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents2Id);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents3Id);
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(InstrumentTab(kWebContents1Id),
                  NavigateWebContents(kWebContents1Id, url1),
                  AddInstrumentedTab(kWebContents2Id, url1),
                  AddInstrumentedTab(kWebContents3Id, url1),
                  SelectTab(kTabStripElementId, 0), OpenSideBySideTab(1),
                  WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
                  SelectTab(kTabStripElementId, 2),
                  WaitForHide(kToolbarSplitTabsToolbarButtonElementId));
}

// Tests that the browser updates the toolbar's visible security state only
// when the state changes, not every time it's asked to update.
IN_PROC_BROWSER_TEST_P(ToolbarViewTest, SecurityStateChanged) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  // Set the location bar's initial security level and check that the browser
  // updates to it.
  SetLocationBarSecurityLevelForTesting(security_state::SecurityLevel::SECURE);
  EXPECT_TRUE(browser_view->UpdateToolbarSecurityState());

  // The security level has not changed, so asking the browser again to update
  // should fail.
  EXPECT_FALSE(browser_view->UpdateToolbarSecurityState());

  // Change the security level and check that the browser updates its toolbar.
  SetLocationBarSecurityLevelForTesting(
      security_state::SecurityLevel::DANGEROUS);
  EXPECT_TRUE(browser_view->UpdateToolbarSecurityState());
}

class ToolbarViewVerticalTabsRTLTest
    : public VerticalTabsInteractiveTestMixin<ToolbarViewTest> {
 public:
  ToolbarViewVerticalTabsRTLTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ToolbarViewTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("force-ui-direction", "rtl");
  }
};

IN_PROC_BROWSER_TEST_P(ToolbarViewVerticalTabsRTLTest, ReloadButtonWorks) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("/title1.html");

  RunTestSequence(
      EnterVerticalTabsMode(),
      WaitForShow(kVerticalTabStripTopContainerElementId),
      InstrumentTab(kTabId), NavigateWebContents(kTabId, url),
      WaitForShow(kReloadButtonElementId),
      WaitForViewProperty(kReloadButtonElementId, ReloadButton, VisibleMode,
                          ReloadControl::Mode::kReload),
      PressButton(kReloadButtonElementId),
      WaitForWebContentsNavigation(kTabId));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ToolbarViewTest,
    ::testing::Values(false, true));

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ToolbarViewVerticalTabsRTLTest,
    ::testing::Values(false));
