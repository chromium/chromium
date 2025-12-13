// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/screen.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/event_monitor.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_sequence_views.h"
#include "ui/views/interaction/widget_focus_observer.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

namespace {
constexpr char kDocumentWithNamedElement[] = "/select.html";
constexpr char kDocumentWithTitle[] = "/title3.html";
constexpr char kDocumentWithTextField[] = "/form_interaction.html";
}  // namespace

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
                       PressButtonAndMouseMoveClick) {
  RelativePositionSpecifier pos = CenterPoint();
#if BUILDFLAG(IS_WIN)
  // Handler for http://crbug.com/392854216 and https://crbug.com/432623498
  // (menu may overlap button).
  pos = base::BindOnce([](ui::TrackedElement* el) {
    gfx::Rect bounds = el->GetScreenBounds();
    auto* const menu_item =
        ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
            AppMenuModel::kMoreToolsMenuItem);
    const gfx::Rect widget_bounds = menu_item->AsA<views::TrackedElementViews>()
                                        ->view()
                                        ->GetWidget()
                                        ->GetWindowBoundsInScreen();

    // Create a rectangle where all points are strictly inside the original
    // bounds.
    bounds.Inset(gfx::Insets::TLBR(1, 1, 2, 2));

    // Test points around the rectangle to find one that does not intersect
    // the menu widget.
    for (const auto& point :
         {bounds.CenterPoint(), bounds.bottom_center(), bounds.left_center(),
          bounds.right_center(), bounds.origin(), bounds.top_right(),
          bounds.bottom_right(), bounds.bottom_left()}) {
      if (!widget_bounds.Contains(point)) {
        return point;
      }
    }

    NOTREACHED() << "Menu widget ()" << widget_bounds.ToString()
                 << ") significantly overlaps menu button ("
                 << bounds.ToString() << ") cannot target button.";
  });
#endif

  RunTestSequence(
      // Ensure the mouse isn't over the app menu button.
      MoveMouseTo(kTabStripElementId),
      // Simulate press of the menu button and ensure the button activates and
      // the menu appears.
      PressButton(kToolbarAppMenuButtonElementId),
      WaitForShow(AppMenuModel::kMoreToolsMenuItem),
      // Move the mouse to the button and click it. This will hide the menu.
      MoveMouseTo(kToolbarAppMenuButtonElementId, std::move(pos)), ClickMouse(),
      WaitForHide(AppMenuModel::kMoreToolsMenuItem));
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
      MoveMouseTo(std::ref(p1)),
      // Verify that the mouse has been moved to the correct point.
      Check(base::BindLambdaForTesting([&]() {
        gfx::Rect rect(p1, gfx::Size());
        rect.Inset(-1);
        const gfx::Point point = display::Screen::Get()->GetCursorScreenPoint();
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
        const gfx::Point point = display::Screen::Get()->GetCursorScreenPoint();
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
  const auto context = BrowserElements::From(incognito)->GetContext();

  RunTestSequenceInContext(
      context, WaitForShow(kBrowserViewElementId),
      ActivateSurface(kBrowserViewElementId),
      MoveMouseTo(kToolbarAppMenuButtonElementId), ClickMouse(),
      SelectMenuItem(AppMenuModel::kDownloadsMenuItem),
      WaitForHide(AppMenuModel::kDownloadsMenuItem),
      // These two types of actions use PostTask() internally and bounce off
      // the pivot element. Make sure they still work in a "InSameContext".
      EnsureNotPresent(AppMenuModel::kDownloadsMenuItem),
      // Make sure this picks up the correct button, since it was after a
      // string of non-element-specific actions.
      CheckElement(
          kToolbarAppMenuButtonElementId,
          [](ui::TrackedElement* el) { return el->context(); }, context));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest,
                       MouseToNewWindowAndDoActionsInSpecificContext) {
  auto* const incognito = CreateIncognitoBrowser();
  const auto context = BrowserElements::From(incognito)->GetContext();

  RunTestSequenceInContext(
      context, ActivateSurface(kBrowserViewElementId),
      MoveMouseTo(kToolbarAppMenuButtonElementId), ClickMouse(),
      SelectMenuItem(AppMenuModel::kDownloadsMenuItem),
      WaitForHide(AppMenuModel::kDownloadsMenuItem),
      // These two types of actions use PostTask() internally and
      // bounce off the pivot element. Make sure they still work in a
      // "InSameContext".
      EnsureNotPresent(AppMenuModel::kDownloadsMenuItem),
      // Make sure this picks up the correct button, since it was
      // after a string of non-element-specific actions.
      CheckElement(
          kToolbarAppMenuButtonElementId,
          [](ui::TrackedElement* el) { return el->context(); }, context));
}

// Tests whether ActivateSurface() can correctly bring a browser window to the
// front so that mouse input can be sent to it.
IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest, ActivateMultipleSurfaces) {
  auto* const incognito = CreateIncognitoBrowser();
  const auto context = BrowserElements::From(incognito)->GetContext();

  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kHaltTest,
                              "Some Linux window managers do not allow "
                              "programmatically raising/activating windows. "
                              "This invalidates the rest of the test."),
      InContext(context, ActivateSurface(kBrowserViewElementId),
                MoveMouseTo(kToolbarAppMenuButtonElementId), ClickMouse(),
                SelectMenuItem(AppMenuModel::kDownloadsMenuItem),
                WaitForHide(AppMenuModel::kDownloadsMenuItem)),
      ActivateSurface(kBrowserViewElementId),
      MoveMouseTo(kToolbarAppMenuButtonElementId), ClickMouse(),
      WaitForShow(AppMenuModel::kDownloadsMenuItem));
}

// Tests whether ActivateSurface() results in kCurrentWidgetFocus updating
// correctly.
IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest,
                       WatchForBrowserActivation) {
  auto* const incognito = CreateIncognitoBrowser();
  const auto context = BrowserElements::From(incognito)->GetContext();

  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kHaltTest,
                              "Some Linux window managers do not allow "
                              "programmatically raising/activating windows. "
                              "This invalidates the rest of the test."),
      ObserveState(views::test::kCurrentWidgetFocus),
      InContext(context, ActivateSurface(kBrowserViewElementId),
                MoveMouseTo(kToolbarAppMenuButtonElementId), ClickMouse(),
                SelectMenuItem(AppMenuModel::kDownloadsMenuItem),
                WaitForHide(AppMenuModel::kDownloadsMenuItem)),
      ActivateSurface(kBrowserViewElementId),
      WaitForState(views::test::kCurrentWidgetFocus, [this]() {
        return BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
      }));
}

// Tests whether ActivateSurface() results in kCurrentWidgetFocus updating
// correctly when targeting a tab's web contents.
IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest,
                       WatchForTabWebContentsActivation) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  auto* const incognito = CreateIncognitoBrowser();
  const auto context = BrowserElements::From(incognito)->GetContext();

  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kHaltTest,
                              "Some Linux window managers do not allow "
                              "programmatically raising/activating windows. "
                              "This invalidates the rest of the test."),
      ObserveState(views::test::kCurrentWidgetFocus),
      InContext(context, ActivateSurface(kBrowserViewElementId),
                MoveMouseTo(kToolbarAppMenuButtonElementId), ClickMouse(),
                SelectMenuItem(AppMenuModel::kDownloadsMenuItem),
                WaitForHide(AppMenuModel::kDownloadsMenuItem)),
      InstrumentTab(kWebContentsElementId),
      ActivateSurface(kWebContentsElementId),
      WaitForState(views::test::kCurrentWidgetFocus, [this]() {
        return BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
      }));
}

// TODO(crbug.com/330095872): Flaky on linux-chromeos-rel and Linux ChromiumOS
// MSan.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_WatchForNonTabWebContentsActivation \
  DISABLED_WatchForNonTabWebContentsActivation
#else
#define MAYBE_WatchForNonTabWebContentsActivation \
  WatchForNonTabWebContentsActivation
#endif
// Tests whether ActivateSurface() results in kCurrentWidgetFocus updating
// correctly when targeting a non-tab web contents.
//
// TODO(crbug.com/40069026): These tests can be kind of hairy and we're working
// on making sure these primitives play nice together and do not flake. If you
// see a flake, first, note that these are edge case tests for new test
// infrastructure and do not directly affect Chrome stability. Next, please:
//  - Reopen or add to the attached bug.
//  - Make sure it is assigned to dfried@chromium.org or another
//    chrome/test/interaction owner.
//  - [Selectively] disable the test on the offending platforms.
//
// Thank you for working with us to make Chrome test infrastructure better!
IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest,
                       MAYBE_WatchForNonTabWebContentsActivation) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  constexpr char kWebViewName[] = "Web View";
  auto* const incognito = CreateIncognitoBrowser();
  const auto context = BrowserElements::From(incognito)->GetContext();

  views::Widget* expected_widget = nullptr;

  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kHaltTest,
                              "Some Linux window managers do not allow "
                              "programmatically raising/activating windows. "
                              "This invalidates the rest of the test."),
      ObserveState(views::test::kCurrentWidgetFocus),
      InContext(context, ActivateSurface(kBrowserViewElementId),
                MoveMouseTo(kToolbarAppMenuButtonElementId), ClickMouse(),
                SelectMenuItem(AppMenuModel::kDownloadsMenuItem),
                WaitForHide(AppMenuModel::kDownloadsMenuItem)),
      PressButton(kTabSearchButtonElementId),
      WaitForShow(kTabSearchBubbleElementId),
      NameDescendantViewByType<views::WebView>(kTabSearchBubbleElementId,
                                               kWebViewName),
      InstrumentNonTabWebView(kWebContentsElementId, kWebViewName),
      ActivateSurface(kWebContentsElementId),
      WithView(kTabSearchBubbleElementId,
               [&expected_widget](views::View* view) {
                 expected_widget = view->GetWidget();
               }),
      WaitForState(views::test::kCurrentWidgetFocus,
                   std::ref(expected_widget)));
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
        const gfx::Point point = display::Screen::Get()->GetCursorScreenPoint();
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

  Browser* const incognito = this->CreateIncognitoBrowser();
  const auto context = BrowserElements::From(incognito)->GetContext();

  // Run the test in the context of the incognito browser.
  RunTestSequenceInContext(
      context,
      // Instrument the tabs but do not force them to load.
      InstrumentTab(kIncognitoPageId, std::nullopt, CurrentBrowser(),
                    /* wait_for_ready =*/false),
      InstrumentTab(kBrowserPageId, std::nullopt, browser(),
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
      InAnyContext(EnsurePresent(kIncognitoPageId)));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest,
                       InstrumentNonTabAsTestStep) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
  const char kTabSearchWebViewName[] = "Tab Search WebView";

  RunTestSequence(
      PressButton(kTabSearchButtonElementId),
      WaitForShow(kTabSearchBubbleElementId),
      NameChildViewByType<views::WebView>(kTabSearchBubbleElementId,
                                          kTabSearchWebViewName),
      InstrumentNonTabWebView(kWebContentsId, kTabSearchWebViewName));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest,
                       SendAcceleratorToWebContents) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kClearAllClickEvent);
  const DeepQuery kClearAllDownloadsButton = {"downloads-manager",
                                              "downloads-toolbar", "#clearAll"};
  const ui::Accelerator kClickWebButtonAccelerator(ui::KeyboardCode::VKEY_SPACE,
                                                   ui::EF_NONE);
  StateChange clear_all_downloads_click;
  clear_all_downloads_click.type = StateChange::Type::kExists;
  clear_all_downloads_click.where = kClearAllDownloadsButton;
  clear_all_downloads_click.event = kClearAllClickEvent;
  RunTestSequence(
      InstrumentTab(kWebContentsId),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kDownloadsMenuItem),
      WaitForWebContentsNavigation(kWebContentsId,
                                   GURL(chrome::kChromeUIDownloadsURL)),
      FocusElement(kWebContentsId),
      ExecuteJsAt(kWebContentsId, kClearAllDownloadsButton, "el => el.focus()"),
      SendAccelerator(kWebContentsId, kClickWebButtonAccelerator),
      WaitForStateChange(kWebContentsId, clear_all_downloads_click));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest, SendKeyToWebContents) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithTextField);
  const DeepQuery kTextField = {"#value"};

  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
      FocusWebContents(kWebContentsId),
      ExecuteJsAt(kWebContentsId, kTextField,
                  "el => { el.focus(); el.value = ''; }"),
      SendKeyPress(kWebContentsId, ui::VKEY_A),
      SendKeyPress(kWebContentsId, ui::VKEY_B, ui::EF_SHIFT_DOWN),
      CheckJsResultAt(kWebContentsId, kTextField, "el => el.value", u"aB"));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest, FocusElement) {
  RunTestSequence(
      FocusElement(kToolbarAppMenuButtonElementId),
      CheckViewProperty(kToolbarAppMenuButtonElementId, &views::View::HasFocus,
                        true),
      FocusElement(kOmniboxElementId),
      CheckViewProperty(kOmniboxElementId, &views::View::HasFocus, true));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest, SendKeyPress) {
  RunTestSequence(
      FocusElement(kOmniboxElementId),
      SendKeyPress(kOmniboxElementId, ui::VKEY_A),
      SendKeyPress(kOmniboxElementId, ui::VKEY_B, ui::EF_SHIFT_DOWN),
      CheckViewProperty(kOmniboxElementId, &OmniboxViewViews::GetText, u"aB"));
}

// Simple bubble containing a WebView. Allows us to simulate swapping out one
// WebContents for another.
class WebBubbleView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(WebBubbleView, views::BubbleDialogDelegateView)

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

  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    // Need a large enough bubble that the WebView has size to render.
    return gfx::Size(300, 400);
  }

  const raw_ptr<Profile> profile_;
  raw_ptr<views::WebView> web_view_;
  std::unique_ptr<content::WebContents> owned_web_contents_;
};

BEGIN_METADATA(WebBubbleView)
END_METADATA

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
                  Do([&]() { bubble->SwapWebContents(url2); }),
                  WaitForWebContentsNavigation(kWebContentsId, url2));

  bubble->GetWidget()->CloseNow();
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest,
                       WaitForWebContentsPainted) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);

  auto* const bubble = WebBubbleView::CreateBubble(browser(), url);

  RunTestSequence(
      InstrumentNonTabWebView(kWebContentsId, bubble->web_view(), false),
      // This should wait for the element to appear and then paint.
      WaitForWebContentsPainted(kWebContentsId),
      // This should be more or less a no-op.
      WaitForWebContentsPainted(kWebContentsId));

  bubble->GetWidget()->CloseNow();
}

// Ensure that the initial active window is detected by the focus observer.
IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestUiTest, InitialWindowActive) {
  auto* const widget =
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  views::test::WaitForWidgetActive(widget, true);

  RunTestSequence(ObserveState(views::test::kCurrentWidgetFocus),
                  WaitForState(views::test::kCurrentWidgetFocus, widget));
}

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHoverView1Id);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHoverView2Id);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHoverView3Id);

class HoverDetectionView : public views::View {
  METADATA_HEADER(HoverDetectionView, views::View)

 public:
  ~HoverDetectionView() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize(const views::SizeBounds&) const override {
    return gfx::Size(200, 50);
  }
  void OnMouseMoved(const ui::MouseEvent& event) override {
    on_mouse_move_callbacks_.Notify(event);
  }
  void OnMouseEntered(const ui::MouseEvent& event) override {
    on_mouse_move_callbacks_.Notify(event);
  }
  void OnMouseExited(const ui::MouseEvent& event) override {
    on_mouse_move_callbacks_.Notify(event);
  }

  using MouseMoveCallback =
      base::RepeatingCallback<void(const ui::MouseEvent&)>;
  auto AddOnMouseMoveCallback(MouseMoveCallback callback) {
    return on_mouse_move_callbacks_.Add(callback);
  }

 private:
  base::RepeatingCallbackList<void(const ui::MouseEvent&)>
      on_mouse_move_callbacks_;
};

class LastHoverEventObserver
    : public ui::test::StateObserver<std::set<ui::EventType>> {
 public:
  explicit LastHoverEventObserver(HoverDetectionView* view)
      : subscription_(view->AddOnMouseMoveCallback(
            base::BindRepeating(&LastHoverEventObserver::OnHoverEvent,
                                base::Unretained(this)))) {}
  ~LastHoverEventObserver() override = default;

  void OnHoverEvent(const ui::MouseEvent& event) {
    observed_events_.insert(event.type());
    OnStateObserverStateChanged(observed_events_);
  }

 private:
  base::CallbackListSubscription subscription_;
  std::set<ui::EventType> observed_events_;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(LastHoverEventObserver, kHoverView1State);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(LastHoverEventObserver, kHoverView2State);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(LastHoverEventObserver, kHoverView3State);

BEGIN_METADATA(HoverDetectionView)
END_METADATA

}  // namespace

class HoverDetectionBubbleView : public views::FlexLayoutView,
                                 public views::BubbleDialogDelegate {
  METADATA_HEADER(HoverDetectionBubbleView, views::FlexLayoutView)

 public:
  explicit HoverDetectionBubbleView(views::View* anchor_view)
      : BubbleDialogDelegate(anchor_view, views::BubbleBorder::TOP_RIGHT) {
    SetOwnedByWidget(OwnedByWidgetPassKey());

    auto* view = AddChildView(std::make_unique<HoverDetectionView>());
    view->SetProperty(views::kElementIdentifierKey, kHoverView1Id);
    views_.push_back(view);

    view = AddChildView(std::make_unique<HoverDetectionView>());
    view->SetProperty(views::kElementIdentifierKey, kHoverView2Id);
    views_.push_back(view);

    view = AddChildView(std::make_unique<HoverDetectionView>());
    view->SetProperty(views::kElementIdentifierKey, kHoverView3Id);
    views_.push_back(view);

    SetOrientation(views::LayoutOrientation::kVertical);
  }

  ~HoverDetectionBubbleView() override { views_.clear(); }

  views::View* GetContentsView() override { return this; }

  HoverDetectionView* view(size_t idx) { return views_[idx].get(); }

 private:
  std::vector<raw_ptr<HoverDetectionView>> views_;
};

BEGIN_METADATA(HoverDetectionBubbleView)
END_METADATA

class InteractiveBrowserTestHoverUiTest : public InteractiveBrowserTestUiTest {
 public:
  InteractiveBrowserTestHoverUiTest() = default;
  ~InteractiveBrowserTestHoverUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTestUiTest::SetUpOnMainThread();

    // Move the mouse somewhere completely outside where the dialog will show.
    auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    mouse_util().PerformGestures(
        {browser_view->GetNativeWindow(), /*force_async=*/false},
        views::test::InteractionTestUtilMouse::MoveTo(
            browser_view->GetBoundsInScreen().origin() +
            gfx::Vector2d(10, 10)));

    // Create and show the bubble.
    auto* const anchor_view = BrowserElementsViews::From(browser())->GetView(
        kToolbarAppMenuButtonElementId);
    CHECK(anchor_view);
    auto bubble_view = std::make_unique<HoverDetectionBubbleView>(anchor_view);
    bubble_view_ = bubble_view.get();
    bubble_widget_ = base::WrapUnique(views::BubbleDialogDelegate::CreateBubble(
        std::move(bubble_view), views::Widget::InitParams::CLIENT_OWNS_WIDGET));
    bubble_widget_->Show();
    bubble_view_->SizeToContents();
  }

  void TearDownOnMainThread() override {
    bubble_view_ = nullptr;
    bubble_widget_.reset();
    InteractiveBrowserTestUiTest::TearDownOnMainThread();
  }

 protected:
  raw_ptr<HoverDetectionBubbleView> bubble_view_ = nullptr;
  std::unique_ptr<views::Widget> bubble_widget_;
};

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestHoverUiTest, MoveMouseHoversView) {
  RunTestSequence(
      WaitForShow(kHoverView1Id),
      ObserveState(kHoverView1State, bubble_view_->view(0)),
      MoveMouseTo(kHoverView1Id),
      WaitForState(kHoverView1State,
                   testing::UnorderedElementsAre(ui::EventType::kMouseEntered,
                                                 ui::EventType::kMouseMoved)));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestHoverUiTest,
                       MoveMouseHoverMultipleViews) {
  RunTestSequence(
      WaitForShow(kHoverView1Id),
      ObserveState(kHoverView1State, bubble_view_->view(0)),
      ObserveState(kHoverView2State, bubble_view_->view(1)),
      ObserveState(kHoverView3State, bubble_view_->view(2)),
      MoveMouseTo(kHoverView1Id),
      WaitForState(kHoverView1State,
                   testing::UnorderedElementsAre(ui::EventType::kMouseEntered,
                                                 ui::EventType::kMouseMoved)),
      MoveMouseTo(kHoverView2Id),
      WaitForState(kHoverView1State,
                   testing::Contains(ui::EventType::kMouseExited)),
      WaitForState(kHoverView2State,
                   testing::UnorderedElementsAre(ui::EventType::kMouseEntered,
                                                 ui::EventType::kMouseMoved)),
      MoveMouseTo(kHoverView3Id),
      WaitForState(kHoverView2State,
                   testing::Contains(ui::EventType::kMouseExited)),
      WaitForState(kHoverView3State,
                   testing::UnorderedElementsAre(ui::EventType::kMouseEntered,
                                                 ui::EventType::kMouseMoved)),
      MoveMouseTo(
          kBrowserViewElementId, base::BindOnce([](ui::TrackedElement* el) {
            return el->GetScreenBounds().origin() + gfx::Vector2d(10, 10);
          })),
      WaitForState(kHoverView3State,
                   testing::Contains(ui::EventType::kMouseExited)));
}
