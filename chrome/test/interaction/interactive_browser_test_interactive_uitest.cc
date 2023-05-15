// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interactive_browser_test.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/event_monitor.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_sequence_views.h"
#include "ui/views/layout/fill_layout.h"
#include "url/gurl.h"

namespace {
constexpr char kDocumentWithNamedElement[] = "/select.html";
constexpr char kDocumentWithTitle[] = "/title3.html";
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

// Tests whether ActivateSurface() can correctly bring a browser window to the
// front so that mouse input can be sent to it.
IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest, ActivateMultipleSurfaces) {
  auto* const incognito = CreateIncognitoBrowser();

  RunTestSequence(
      InContext(incognito->window()->GetElementContext(),
                Steps(ActivateSurface(kBrowserViewElementId),
                      MoveMouseTo(kAppMenuButtonElementId), ClickMouse(),
                      SelectMenuItem(AppMenuModel::kDownloadsMenuItem),
                      WaitForHide(AppMenuModel::kDownloadsMenuItem))),
      FlushEvents(),
      SetOnIncompatibleAction(OnIncompatibleAction::kHaltTest,
                              "Some Linux window managers do not allow "
                              "programmatically raising/activating windows. "
                              "This invalidates the rest of the test."),
      ActivateSurface(kBrowserViewElementId),
      MoveMouseTo(kAppMenuButtonElementId), ClickMouse(),
      WaitForShow(AppMenuModel::kDownloadsMenuItem));
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
      WaitForStateChange(kWebPageId, state_change),
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

namespace {

// Simple bubble containing a WebView. Allows us to simulate swapping out one
// WebContents for another.
class WebBubbleView : public views::BubbleDialogDelegateView {
 public:
  ~WebBubbleView() override = default;

  // Creates a bubble with a WebView and loads `url` in the view.
  static WebBubbleView* CreateBubble(Browser* browser, GURL url) {
    BrowserView* const browser_view =
        BrowserView::GetBrowserViewForBrowser(browser);
    auto bubble_ptr = base::WrapUnique(
        new WebBubbleView(browser_view->toolbar(), browser->profile(), url));
    auto* const bubble = bubble_ptr.get();
    views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_ptr))
        ->Show();
    return bubble;
  }

  // Swaps out the current WebContents for a new one and loads `url` into that
  // new WebContents.
  void SwapWebContents(GURL url) {
    owned_web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_view_->SetWebContents(owned_web_contents_.get());
    web_view_->LoadInitialURL(url);
  }

  // Gets the WebView displayed by this bubble.
  views::WebView* web_view() { return web_view_; }

 private:
  WebBubbleView(views::View* anchor_view, Profile* profile, GURL url)
      : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT),
        profile_(profile) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    web_view_ = AddChildView(std::make_unique<views::WebView>(profile));
    web_view_->LoadInitialURL(url);
  }

  const base::raw_ptr<Profile> profile_;
  base::raw_ptr<views::WebView> web_view_;
  std::unique_ptr<content::WebContents> owned_web_contents_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest,
                       SwappingWebViewWebContentsTreatedAsNavigation) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const GURL url2 = embedded_test_server()->GetURL(kDocumentWithTitle);

  auto* const bubble = WebBubbleView::CreateBubble(browser(), url);

  RunTestSequence(InstrumentNonTabWebView(kWebContentsId, bubble->web_view()),
                  // Need to flush here because we're still responding to the
                  // original WebContents being shown, so we can't destroy the
                  // WebContents until the call resolves.
                  FlushEvents(), Do([&]() { bubble->SwapWebContents(url2); }),
                  WaitForWebContentsNavigation(kWebContentsId, url2));

  bubble->GetWidget()->CloseNow();
}
