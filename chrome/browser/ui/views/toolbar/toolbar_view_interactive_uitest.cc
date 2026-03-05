// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/focus/focus_manager.h"
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

class WebViewFocusManager : views::FocusChangeListener,
                            content::WebContentsObserver {
 public:
  explicit WebViewFocusManager(views::FocusManager* focus_manager,
                               content::WebContents* web_contents)
      : focus_manager_(focus_manager) {
    focus_manager->AddFocusChangeListener(this);
    Observe(web_contents);
  }

  ~WebViewFocusManager() override {
    focus_manager_->RemoveFocusChangeListener(this);
  }

  void AdvanceFocus(bool reverse) {
    base::Time start = base::Time::Now();
    // Try-bots sometimes don't register these key presses (even with if the
    // view is copyable), so we try multiple times to avoid flakes.
    do {
      run_loop_ = std::make_unique<base::RunLoop>();
      ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
          web_contents()->GetTopLevelNativeWindow(), ui::VKEY_TAB, false,
          reverse, false, false));
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, run_loop_->QuitClosure(), base::Seconds(1));
      run_loop_->Run();
      run_loop_.reset();
    } while (!done_ && (base::Time::Now() < start + base::Seconds(5)));
  }

 private:
  // content::WebContentsObserver overrides
  void OnFocusChangedInPage(
      const content::FocusedNodeDetails& details) override {
    if (details.node_bounds_in_screen.IsEmpty()) {
      // Focus is leaving the web contents
      return;
    }
    Done();
  }

  // views::FocusChangeListener
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override {
    Done();
  }

  void Done() {
    done_ = true;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  bool done_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
  raw_ptr<views::FocusManager> focus_manager_;
};

void AdvanceFocus(views::FocusManager* focus_manager, bool reverse) {
  views::View* view = focus_manager->GetFocusedView();
  if (views::WebView* web_view = views::AsViewClass<views::WebView>(view)) {
    WebViewFocusManager web_view_focus_manager(focus_manager,
                                               web_view->web_contents());
    web_view_focus_manager.AdvanceFocus(reverse);
  } else {
    focus_manager->AdvanceFocus(reverse);
  }
}

int GetIDForFocusedViewElement(const views::View* view) {
  const std::string kReloadControlName =
      base::UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_ACCNAME_RELOAD));

  if (const views::WebView* web_view =
          views::AsViewClass<views::WebView>(view)) {
    std::string element_name =
        content::EvalJs(web_view->web_contents(), kGetFocusedElementJS)
            .ExtractString();
    if (element_name == kReloadControlName) {
      return VIEW_ID_RELOAD_BUTTON;
    } else {
      ADD_FAILURE() << "Unexpected focused element: " << element_name;
      return VIEW_ID_NONE;
    }
  } else {
    return view->GetID();
  }
}

}  // namespace

class ToolbarViewTest : public InteractiveBrowserTest,
                        public ::testing::WithParamInterface<bool> {
 public:
  ToolbarViewTest() {
    if (GetParam()) {
      feature_list_.InitWithFeatures(
          {features::kInitialWebUI, features::kWebUIReloadButton}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {features::kInitialWebUI, features::kWebUIReloadButton});
    }
  }
  ToolbarViewTest(const ToolbarViewTest&) = delete;
  ToolbarViewTest& operator=(const ToolbarViewTest&) = delete;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    WaitForInitialWebUICanShow(browser());
  }

  void WaitForInitialWebUICanShow(Browser* browser) {
    // Wait for the toolbar to load. Note that we can't wait for the widget to
    // become visible instead because the Widget will always be visible on Mac
    // OS.
    ASSERT_TRUE(base::test::RunUntil([browser]() {
      InitialWebUIManager* manager = InitialWebUIManager::From(browser);
      return !manager || !manager->RequestDeferShow(base::DoNothing());
    }));
  }

  void RunToolbarCycleFocusTest(Browser* browser);

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
                 MoveMouseTo(kTabToHover),
                 MayInvolveNativeContextMenu(
                     ClickMouse(ui_controls::RIGHT),
                     SelectMenuItem(TabMenuModel::kSplitTabsMenuItem)));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

void ToolbarViewTest::RunToolbarCycleFocusTest(Browser* browser) {
  gfx::NativeWindow window = browser->window()->GetNativeWindow();
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
  CommandUpdater* updater = browser->command_controller();
  updater->ExecuteCommand(IDC_FOCUS_TOOLBAR);

  views::FocusManager* focus_manager = widget->GetFocusManager();
  views::View* first_view = focus_manager->GetFocusedView();
  int first_id = GetIDForFocusedViewElement(first_view);

  std::vector<int> ids;

  // Press Tab to cycle through all of the controls in the toolbar until
  // we end up back where we started.
  bool found_reload = false;
  bool found_location_bar = false;
  bool found_app_menu = false;
  const views::View* view = first_view;
  do {
    AdvanceFocus(focus_manager, false);

    view = focus_manager->GetFocusedView();
    int id = GetIDForFocusedViewElement(view);
    ids.push_back(id);

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
  ASSERT_TRUE(found_reload);
  ASSERT_TRUE(found_app_menu);
  ASSERT_TRUE(found_location_bar);

  // Now press Shift-Tab to cycle backwards.
  std::vector<int> reverse_ids;
  do {
    AdvanceFocus(focus_manager, true);

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
  CommandUpdater* updater = browser()->command_controller();
  updater->ExecuteCommand(IDC_SHOW_BOOKMARK_BAR);

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::AddIfNotBookmarked(model, GURL("http://foo.com"), u"Foo");

  // We want to specifically test the case where the bookmark bar is
  // already showing when a window opens, so create a second browser
  // window with the same profile.
  Browser* second_browser = CreateBrowser(browser()->profile());
  WaitForInitialWebUICanShow(second_browser);
  RunToolbarCycleFocusTest(second_browser);
}

IN_PROC_BROWSER_TEST_P(ToolbarViewTest, BackButtonUpdate) {
  ToolbarButtonProvider* toolbar_button_provider =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  ToolbarButton* back_button = toolbar_button_provider->GetBackButton();
  EXPECT_FALSE(back_button->GetEnabled());

  // Navigate to title1.html. Back button should be enabled.
  GURL url = chrome_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("title1.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(back_button->GetEnabled());

  // Delete old navigations. Back button will be disabled.
  auto& controller =
      browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  controller.DeleteNavigationEntries(base::BindRepeating(
      [&](content::NavigationEntry* entry) { return true; }));
  EXPECT_FALSE(back_button->GetEnabled());
}

IN_PROC_BROWSER_TEST_P(ToolbarViewTest, BackButtonHoverThenClick) {
  ToolbarButtonProvider* toolbar_button_provider =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  ToolbarButton* back_button = toolbar_button_provider->GetBackButton();
  EXPECT_FALSE(back_button->GetEnabled());

  // Navigate to title1.html. Back button should be enabled.
  GURL url = chrome_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("title1.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(back_button->GetEnabled());

  // Mouse over and click on the back button. This should navigate back in
  // session history.
  content::TestNavigationObserver back_nav_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ui_test_utils::ClickOnView(back_button);
  back_nav_observer.Wait();

  EXPECT_FALSE(back_button->GetEnabled());
}

// TODO(crbug.com/40252318): The ui test utils do not seem to adequately
// simulate mouse hovering on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_BackButtonHoverMetricsLogged DISABLED_BackButtonHoverMetricsLogged
#else
#define MAYBE_BackButtonHoverMetricsLogged BackButtonHoverMetricsLogged
#endif
IN_PROC_BROWSER_TEST_P(ToolbarViewTest, MAYBE_BackButtonHoverMetricsLogged) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ToolbarButtonProvider* toolbar_button_provider =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();

  // Set the initial mouse position to a known state. If the mouse happens to
  // be over the back button at the start of the test, then the mouse movement
  // done by the test wouldn't be seen as a mouse enter.
  // The choice of using the reload button as the starting position is
  // arbitrary.
  ASSERT_TRUE(RunTestSequence(MoveMouseTo(
      kReloadButtonElementId, base::BindOnce([](ui::TrackedElement* el) {
        return el->GetScreenBounds().CenterPoint();
      }))));

  const GURL first_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  const GURL cross_site_url =
      embedded_test_server()->GetURL("b.test", "/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), cross_site_url));

  ToolbarButton* back_button = toolbar_button_provider->GetBackButton();
  EXPECT_TRUE(back_button->GetEnabled());

  base::HistogramTester histogram_tester;

  content::TestNavigationObserver back_nav_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ui_test_utils::ClickOnView(back_button);
  back_nav_observer.Wait();

  // content/ internal tests cover the details of various navigation scenarios
  // in relation to this histogram. It's enough for this test confirm that a
  // sample was added, rather than its specific value.
  histogram_tester.ExpectTotalCount(
      "Preloading.PrerenderBackNavigationEligibility.BackButtonHover", 1);
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
  Browser* target_browser = chrome::FindAnyBrowser(guest, true);
  ASSERT_TRUE(target_browser);
  ExtensionsToolbarDesktop* extensions_container =
      BrowserView::GetBrowserViewForBrowser(target_browser)
          ->toolbar()
          ->extensions_container();
  EXPECT_EQ(nullptr, extensions_container);
}

// Verifies that the identifiers for the pop-up menus are properly assigned so
// that the menu can be located by tests when it is shown.
//
// The back button is just one example for which the menu identifier is defined.
IN_PROC_BROWSER_TEST_P(ToolbarViewTest, BackButtonMenu) {
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
      Log("Logging to probe crbug.com/1489499. Waiting for back button menu."),
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

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(tabs::kVerticalTabs);
    ToolbarViewTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/485291602): Re-enable this test.
IN_PROC_BROWSER_TEST_P(ToolbarViewVerticalTabsRTLTest,
                       DISABLED_ReloadButtonWorks) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("/title1.html");

  RunTestSequence(EnterVerticalTabsMode(),
                  WaitForShow(kVerticalTabStripTopContainerElementId),
                  InstrumentTab(kTabId), NavigateWebContents(kTabId, url),
                  WaitForShow(kReloadButtonElementId),
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
