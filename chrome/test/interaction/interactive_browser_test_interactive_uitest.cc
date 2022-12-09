// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/event_monitor.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_sequence_views.h"
#include "url/gurl.h"

namespace {
constexpr char kDocumentWithNamedElement[] = "/select.html";
}

class InteractiveBrowserTestUiTest : public InteractiveBrowserTest {
 public:
  InteractiveBrowserTestUiTest() = default;
  ~InteractiveBrowserTestUiTest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest,
                       TestEventTypesAndMouseMoveClick) {
  RunTestSequence(
      // Ensure the mouse isn't over the app menu button.
      MoveMouseTo(kTabStripElementId),
      // Simulate press of the menu button and ensure the button activates and
      // the menu appears.
      Do(base::BindOnce([]() { LOG(INFO) << "In second action."; })),
      PressButton(kAppMenuButtonElementId),
      AfterActivate(
          kAppMenuButtonElementId,
          base::BindLambdaForTesting(
              [&](ui::InteractionSequence* seq, ui::TrackedElement* el) {
                // Check AsView() to make sure it correctly returns the view.
                auto* const button = AsView<BrowserAppMenuButton>(el);
                auto* const browser_view =
                    BrowserView::GetBrowserViewForBrowser(browser());
                if (button != browser_view->toolbar()->app_menu_button()) {
                  LOG(WARNING)
                      << "AsView() should have returned the app menu button.";
                  seq->FailForTesting();
                }
              })),
      AfterShow(AppMenuModel::kMoreToolsMenuItem, base::DoNothing()),
      // Move the mouse to the button and click it. This will hide the menu.
      MoveMouseTo(kAppMenuButtonElementId), ClickMouse(),
      AfterHide(AppMenuModel::kMoreToolsMenuItem, base::DoNothing()));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest, TestNameAndDrag) {
  const char kWebContentsName[] = "WebContents";
  gfx::Point p1;
  gfx::Point p2;
  auto p2gen = base::BindLambdaForTesting([&](ui::TrackedElement* el) {
    p2 = el->AsA<views::TrackedElementViews>()
             ->view()
             ->GetBoundsInScreen()
             .bottom_right() -
         gfx::Vector2d(5, 5);
    return p2;
  });

  RunTestSequence(
      // Name the browser's primary webview and calculate a point in its upper
      // left.
      NameViewRelative(
          kBrowserViewElementId, kWebContentsName,
          base::BindOnce([](BrowserView* browser_view) -> views::View* {
            return browser_view->contents_web_view();
          })),
      WithView(kWebContentsName,
               base::BindLambdaForTesting([&p1](views::View* view) {
                 p1 = view->GetBoundsInScreen().origin() + gfx::Vector2d(5, 5);
               })),
      // Move the mouse to the point. Use the gfx::Point* version so we can
      // dynamically receive the value calculated in the previous step.
      MoveMouseTo(&p1),
      // Verify that the mouse has been moved to the correct point.
      Check(base::BindLambdaForTesting([&]() {
        gfx::Rect rect(p1, gfx::Size());
        rect.Inset(-1);
        const gfx::Point point =
            display::Screen::GetScreen()->GetCursorScreenPoint();
        if (!rect.Contains(point)) {
          LOG(ERROR) << "Expected cursor pos " << point.ToString()
                     << " to be roughly " << p1.ToString();
          return false;
        }
        return true;
      })),
      // Drag the mouse to a point returned from a generator function. The
      // function also stores the result in |p2|.
      DragMouseTo(kWebContentsName, std::move(p2gen), false),
      // Verify that the mouse moved to the correct point.
      Check(base::BindLambdaForTesting([&]() {
        gfx::Rect rect(p2, gfx::Size());
        rect.Inset(-1);
        const gfx::Point point =
            display::Screen::GetScreen()->GetCursorScreenPoint();
        if (!rect.Contains(point)) {
          LOG(ERROR) << "Expected cursor pos " << point.ToString()
                     << " to be roughly " << p2.ToString();
          return false;
        }
        return true;
      })),
      // Release the mouse button.
      ReleaseMouse());
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest,
                       MouseToNewWindowAndDoActionsInSameContext) {
  Browser* const incognito = CreateIncognitoBrowser();

  RunTestSequence(
      InContext(incognito->window()->GetElementContext(),
                WaitForShow(kBrowserViewElementId)),
      InSameContext(Steps(
          ActivateSurface(kBrowserViewElementId), FlushEvents(),
          MoveMouseTo(kAppMenuButtonElementId), ClickMouse(),
          SelectMenuItem(AppMenuModel::kDownloadsMenuItem),
          WaitForHide(AppMenuModel::kDownloadsMenuItem),
          // These two types of actions use PostTask() internally and bounce off
          // the pivot element. Make sure they still work in a "InSameContext".
          FlushEvents(), EnsureNotPresent(AppMenuModel::kDownloadsMenuItem),
          // Make sure this picks up the correct button, since it was after a
          // string of non-element-specific actions.
          WithElement(kAppMenuButtonElementId,
                      base::BindOnce(base::BindLambdaForTesting(
                          [incognito](ui::TrackedElement* el) {
                            EXPECT_EQ(incognito->window()->GetElementContext(),
                                      el->context());
                          }))))));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest,
                       MouseToNewWindowAndDoActionsInSpecificContext) {
  auto* const incognito = CreateIncognitoBrowser();

  RunTestSequence(InContext(
      incognito->window()->GetElementContext(),
      Steps(ActivateSurface(kBrowserViewElementId), FlushEvents(),
            MoveMouseTo(kAppMenuButtonElementId), ClickMouse(),
            SelectMenuItem(AppMenuModel::kDownloadsMenuItem),
            WaitForHide(AppMenuModel::kDownloadsMenuItem),
            // These two types of actions use PostTask() internally and
            // bounce off the pivot element. Make sure they still work in a
            // "InSameContext".
            FlushEvents(), EnsureNotPresent(AppMenuModel::kDownloadsMenuItem),
            // Make sure this picks up the correct button, since it was
            // after a string of non-element-specific actions.
            WithElement(kAppMenuButtonElementId,
                        base::BindOnce(base::BindLambdaForTesting(
                            [incognito](ui::TrackedElement* el) {
                              EXPECT_EQ(
                                  incognito->window()->GetElementContext(),
                                  el->context());
                            }))))));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest,
                       WebPageNavigateStateAndLocation) {
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebPageId);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementReadyEvent);

  const DeepQuery kDeepQuery{"#select"};
  StateChange state_change;
  state_change.event = kElementReadyEvent;
  state_change.type = StateChange::Type::kExists;
  state_change.where = kDeepQuery;

  RunTestSequence(
      InstrumentTab(kWebPageId),

      // Load a different page. We could use NavigateWebContents() but that's
      // tested elsewhere and this test will test WaitForWebContentsNavigation()
      // instead.
      WithElement(kWebPageId, base::BindOnce(
                                  [](GURL url, ui::TrackedElement* el) {
                                    // This also provides an opportunity to test
                                    // AsInstrumentedWebContents().
                                    auto* const tab =
                                        AsInstrumentedWebContents(el);
                                    tab->LoadPage(url);
                                  },
                                  url)),
      WaitForWebContentsNavigation(kWebPageId, url),

      // Wait for an expected element to be present and move the mouse to that
      // element.
      WaitForStateChange(kWebPageId, std::move(state_change)),
      MoveMouseTo(kWebPageId, kDeepQuery),

      // Verify that the mouse cursor is now in the web contents.
      Check(base::BindLambdaForTesting([&]() {
        BrowserView* const browser_view =
            BrowserView::GetBrowserViewForBrowser(browser());
        const gfx::Rect web_contents_bounds =
            browser_view->contents_web_view()->GetBoundsInScreen();
        const gfx::Point point =
            display::Screen::GetScreen()->GetCursorScreenPoint();
        if (!web_contents_bounds.Contains(point)) {
          LOG(ERROR) << "Expected cursor pos " << point.ToString() << " to in "
                     << web_contents_bounds.ToString();
          return false;
        }
        return true;
      })));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest,
                       InAnyContextAndEnsureNotPresent) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBrowserPageId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kIncognitoPageId);

  Browser* const incognito_browser = this->CreateIncognitoBrowser();

  // Run the test in the context of the incognito browser.
  RunTestSequenceInContext(
      incognito_browser->window()->GetElementContext(),
      // Instrument the tabs but do not force them to load.
      InstrumentTab(kIncognitoPageId, absl::nullopt, CurrentBrowser(),
                    /* wait_for_ready =*/false),
      InstrumentTab(kBrowserPageId, absl::nullopt, browser(),
                    /* wait_for_ready =*/false),
      // Wait for the pages to load. Manually specify that the incognito page
      // must be in the default context (otherwise, this verb defaults to being
      // context-agnostic).
      WaitForWebContentsReady(kIncognitoPageId)
          .SetContext(ui::InteractionSequence::ContextMode::kInitial),
      WaitForWebContentsReady(kBrowserPageId),
      // The regular browser page is not present if we do not specify
      // InAnyContext().
      EnsureNotPresent(kBrowserPageId),
      // But we can find a page in the correct context even if we specify
      // InAnyContext().
      InAnyContext(WithElement(kIncognitoPageId, base::DoNothing())));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest,
                       InstrumentNonTabAsTestStep) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
  const char kTabSearchWebViewName[] = "Tab Search WebView";

  RunTestSequence(
      PressButton(kTabSearchButtonElementId),
      WaitForShow(kTabSearchBubbleElementId),
      NameViewRelative(
          kTabSearchBubbleElementId, kTabSearchWebViewName,
          base::BindOnce([](WebUIBubbleDialogView* view) -> views::View* {
            return view->web_view();
          })),
      InstrumentNonTabWebView(kWebContentsId, kTabSearchWebViewName),
      WithElement(kTabSearchWebViewName, base::DoNothing()));
}
