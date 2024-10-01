// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/find_bar_host.h"
#include "chrome/browser/ui/views/find_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/test/base/find_result_waiter.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/find_in_page/find_notification_details.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/focus_changed_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_modifiers.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/view_focus_observer.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

using base::ASCIIToUTF16;
using content::WebContents;
using ui_test_utils::IsViewFocused;

namespace {
const char kSimplePage[] = "/find_in_page/simple.html";

std::unique_ptr<net::test_server::HttpResponse> HandleHttpRequest(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content(
      "This is some text with a <a href=\"linked_page.html\">link</a>.");
  response->set_content_type("text/html");
  return response;
}

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabBId);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                    kTextCopiedState);
const ui::Accelerator ctrl_c_accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN);
const ui::Accelerator ctrl_v_accelerator(ui::VKEY_V, ui::EF_CONTROL_DOWN);
}  // namespace

struct FindResultState {
  static const int kInitialActiveMatchOrdinalCount = -1;
  static const int kInitialNumberOfMatches = -1;
  int active_match_ordinal = kInitialActiveMatchOrdinalCount;
  int number_of_matches = kInitialNumberOfMatches;
  FindResultState()
      : FindResultState(kInitialActiveMatchOrdinalCount,
                        kInitialNumberOfMatches) {}
  FindResultState(int active_match_ordinal, int number_of_matches)
      : active_match_ordinal(active_match_ordinal),
        number_of_matches(number_of_matches) {}

  bool operator==(const FindResultState& b) const = default;
};

class FindResulStateObserver : public ui::test::ObservationStateObserver<
                                   FindResultState,
                                   find_in_page::FindTabHelper,
                                   find_in_page::FindResultObserver> {
 public:
  explicit FindResulStateObserver(find_in_page::FindTabHelper* find_tab_helper)
      : ObservationStateObserver(find_tab_helper) {}
  ~FindResulStateObserver() override = default;

  // ObservationStateObserver:
  FindResultState GetStateObserverInitialState() const override {
    return FindResultState();
  }

  // FindResultObserver:
  void OnFindResultAvailable(content::WebContents* web_contents) override {
    const find_in_page::FindNotificationDetails& find_details =
        find_in_page::FindTabHelper::FromWebContents(web_contents)
            ->find_result();

    if (!find_details.final_update()) {
      return;
    }

    FindResultState result = {find_details.active_match_ordinal(),
                              find_details.number_of_matches()};

    OnStateObserverStateChanged(result);
  }
};
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(FindResulStateObserver, kFindResultState);

// TODO(crbug.com/41482547): Remaining tests should be migrated to
// FindBarViewsUiTest.
class LegacyFindInPageTest : public InProcessBrowserTest {
 public:
  LegacyFindInPageTest() {
    // TODO(https://crbug.com/40183900): Undo this in the destructor!
    FindBarHost::SetEnableAnimationsForTesting(false);
  }

  LegacyFindInPageTest(const LegacyFindInPageTest&) = delete;
  LegacyFindInPageTest& operator=(const LegacyFindInPageTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Some bots are flaky due to slower loading interacting with
    // deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  FindBarHost* GetFindBarHost() {
    FindBar* find_bar = browser()->GetFindBarController()->find_bar();
    return static_cast<FindBarHost*>(find_bar);
  }

  FindBarView* GetFindBarView() {
    return GetFindBarHost()->GetFindBarViewForTesting();
  }

  std::u16string GetFindBarText() { return GetFindBarHost()->GetFindText(); }

  std::u16string GetFindBarSelectedText() {
    return GetFindBarHost()->GetFindBarTesting()->GetFindSelectedText();
  }

  bool IsFindBarVisible() { return GetFindBarHost()->IsFindBarVisible(); }

  void ClickOnView(views::View* view) {
    // EventGenerator and ui_test_utils can't target the find bar (on Windows).
    view->OnMousePressed(ui::MouseEvent(
        ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
        base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
    view->OnMouseReleased(ui::MouseEvent(
        ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
        base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  }

  void TapOnView(views::View* view) {
    // EventGenerator and ui_test_utils can't target the find bar (on Windows).
    ui::GestureEvent event(0, 0, 0, base::TimeTicks(),
                           ui::GestureEventDetails(ui::EventType::kGestureTap));
    view->OnGestureEvent(&event);
  }

  find_in_page::FindNotificationDetails WaitForFindResult() {
    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ui_test_utils::FindResultWaiter(web_contents).Wait();
    return find_in_page::FindTabHelper::FromWebContents(web_contents)
        ->find_result();
  }

  find_in_page::FindNotificationDetails WaitForFinalFindResult() {
    while (true) {
      auto details = WaitForFindResult();
      if (details.final_update())
        return details;
    }
  }
};

class FindBarViewsUiTest : public InteractiveBrowserTest {
 public:
  FindBarViewsUiTest() {
    // TODO(https://crbug.com/40183900): Undo this in the destructor!
    FindBarHost::SetEnableAnimationsForTesting(false);

    feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlay, {
                                          {"find-in-page-entry-point", "true"},
                                      });
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&HandleHttpRequest));
    embedded_test_server()->StartAcceptingConnections();

    InteractiveBrowserTest::SetUpOnMainThread();
  }

  ui::InteractionSequence::StepBuilder BringBrowserWindowToFront() {
    return std::move(Check([this]() {
                       return ui_test_utils::BringBrowserWindowToFront(
                           browser());
                     }).SetDescription("BringBrowserWindowToFront()"));
  }

  auto ShowFindBar() {
    auto result =
        Steps(Do([this]() { browser()->GetFindBarController()->Show(); }),
              WaitForShow(FindBarView::kElementId));
    AddDescription(result, "ShowFindBar( %s )");
    return result;
  }

  auto HideFindBar() {
    auto result = Steps(Do([this]() {
                          browser()->GetFindBarController()->EndFindSession(
                              find_in_page::SelectionAction::kKeep,
                              find_in_page::ResultAction::kKeep);
                        }),
                        WaitForHide(FindBarView::kElementId));
    AddDescription(result, "HideFindBar( %s )");
    return result;
  }

  // NB: Prefer using SendAccelerator() when possible.
  auto SendKeyPress(ui::KeyboardCode key, bool control, bool shift) {
    return Check([this, key, control, shift]() {
      return ui_test_utils::SendKeyPressSync(browser(), key, control, shift,
                                             false, false);
    });
  }

  auto Init(GURL url) {
    return Steps(
        ObserveState(
            views::test::kCurrentFocusedViewId,
            BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()),
        InstrumentTab(kTabId), NavigateWebContents(kTabId, url));
  }

  template <typename M>
  auto CheckHasFocusImpl(M&& matcher, std::string description) {
    auto result = Steps(
        Log("Waiting for focus ", description, " current focus is ",
            [this]() -> std::string {
              if (const auto* const focused =
                      BrowserView::GetBrowserViewForBrowser(browser())
                          ->GetFocusManager()
                          ->GetFocusedView()) {
                if (const auto id =
                        focused->GetProperty(views::kElementIdentifierKey)) {
                  return id.GetName();
                }
                return focused->GetClassName();
              }
              return "(none)";
            }),
        WaitForState(views::test::kCurrentFocusedViewId,
                     std::forward<M>(matcher)));
    AddDescription(result, "CheckHasFocus( %s )");
    return result;
  }

#define CheckHasFocus(matcher) CheckHasFocusImpl(matcher, #matcher)

  static auto Focus(ui::ElementIdentifier view) {
    auto result =
        Steps(WithView(view, [](views::View* view) { view->RequestFocus(); }),
              WaitForState(views::test::kCurrentFocusedViewId, view));
    AddDescription(result, "Focus( %s )");
    return result;
  }

 private:
  FindBarHost* GetFindBarHost() {
    FindBar* find_bar = browser()->GetFindBarController()->find_bar();
    return static_cast<FindBarHost*>(find_bar);
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(FindBarViewsUiTest, CrashEscHandlers) {
  const GURL page_a = embedded_test_server()->GetURL("/a.html");
  const GURL page_b = embedded_test_server()->GetURL("/b.html");

  RunTestSequence(
      // Open tab A and show the Find bar.
      Init(page_a), ShowFindBar(),
      // Open tab B and show the Find bar.
      AddInstrumentedTab(kTabBId, page_b), ShowFindBar(),
      // Select tab A.
      SelectTab(kTabStripElementId, 0),
      // Close tab B.
      Do([this]() {
        browser()->tab_strip_model()->CloseWebContentsAt(
            1, TabCloseTypes::CLOSE_NONE);
      }),
      // Set focus to the omnibox.
      Focus(kOmniboxElementId),
      // This used to crash until bug 1303709 was fixed.
      SendKeyPress(ui::VKEY_ESCAPE, false, false));
}

IN_PROC_BROWSER_TEST_F(FindBarViewsUiTest, NavigationByKeyEvent) {
  constexpr char16_t kSearchThis[] = u"a";
  const GURL page_a = embedded_test_server()->GetURL("/a.html");

  RunTestSequence(
      // Open tab A and show Find bar.
      Init(page_a), ShowFindBar(),
      ObserveState(kFindResultState,
                   [this]() {
                     return find_in_page::FindTabHelper::FromWebContents(
                         browser()->tab_strip_model()->GetActiveWebContents());
                   }),
      // Search for 'a'.
      EnterText(FindBarView::kTextField, kSearchThis),
      WaitForState(
          kFindResultState,
          testing::Field(
              &FindResultState::active_match_ordinal,
              testing::Ne(FindResultState::kInitialActiveMatchOrdinalCount))),
      // Press the [Tab] key and [Enter], the previous button should still be
      // focused.
      SendKeyPress(ui::VKEY_TAB, false, false),
      SendKeyPress(ui::VKEY_RETURN, false, false),
      CheckHasFocus(FindBarView::kPreviousButtonElementId),
      // Press the [Tab] key and [Enter], the next button should still be
      // focused.
      SendKeyPress(ui::VKEY_TAB, false, false),
      SendKeyPress(ui::VKEY_RETURN, false, false),
      CheckHasFocus(FindBarView::kNextButtonElementId),
      StopObservingState(kFindResultState));
}

IN_PROC_BROWSER_TEST_F(LegacyFindInPageTest, AccessibleName) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  // First we navigate to any page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSimplePage)));
  // Show the Find bar.
  browser()->GetFindBarController()->Show();

  FindBarView* find_bar_view = GetFindBarView();
  gfx::Rect clip_rect;
  int number_of_matches = 0;
  int active_match_ordinal = 5;
  std::unique_ptr<find_in_page::FindNotificationDetails> details =
      std::make_unique<find_in_page::FindNotificationDetails>(
          /*request_id= */ 1, number_of_matches, clip_rect,
          active_match_ordinal, /*final_update= */ true);

  find_bar_view->UpdateForResult(*details, u"test_string");
  ui::AXNodeData data;
  find_bar_view->GetFindBarMatchCountLabelViewAccessibilityForTesting()
      .GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(IDS_ACCESSIBLE_FIND_IN_PAGE_NO_RESULTS));

  number_of_matches = 3;
  details = std::make_unique<find_in_page::FindNotificationDetails>(
      /*request_id= */ 1, number_of_matches, clip_rect, active_match_ordinal,
      /*final_update= */ true);
  find_bar_view->UpdateForResult(*details, u"test_string");
  data = ui::AXNodeData();
  find_bar_view->GetFindBarMatchCountLabelViewAccessibilityForTesting()
      .GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(IDS_ACCESSIBLE_FIND_IN_PAGE_COUNT,
                                       base::FormatNumber(active_match_ordinal),
                                       base::FormatNumber(number_of_matches)));
}

IN_PROC_BROWSER_TEST_F(LegacyFindInPageTest, ButtonsDoNotAlterFocus) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  // First we navigate to any page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSimplePage)));
  // Show the Find bar.
  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  const int match_count = ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(), u"e", true, false,
      nullptr, nullptr);
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // This test requires at least 3 possible matches.
  ASSERT_GE(match_count, 3);
  // Avoid GetViewByID on BrowserView; the find bar is outside its hierarchy.
  FindBarView* find_bar_view = GetFindBarView();
  views::View* next_button =
      find_bar_view->GetViewByID(VIEW_ID_FIND_IN_PAGE_NEXT_BUTTON);
  views::View* previous_button =
      find_bar_view->GetViewByID(VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON);

  // Clicking the next and previous buttons should not alter the focused view.
  ClickOnView(next_button);
  EXPECT_EQ(2, WaitForFinalFindResult().active_match_ordinal());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  ClickOnView(previous_button);
  EXPECT_EQ(1, WaitForFinalFindResult().active_match_ordinal());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Tapping the next and previous buttons should not alter the focused view.
  TapOnView(next_button);
  EXPECT_EQ(2, WaitForFinalFindResult().active_match_ordinal());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  TapOnView(previous_button);
  EXPECT_EQ(1, WaitForFinalFindResult().active_match_ordinal());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // The same should be true even when the previous button is focused.
  previous_button->RequestFocus();
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON));
  ClickOnView(next_button);
  EXPECT_EQ(2, WaitForFinalFindResult().active_match_ordinal());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON));
  TapOnView(next_button);
  EXPECT_EQ(3, WaitForFinalFindResult().active_match_ordinal());
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON));
}

IN_PROC_BROWSER_TEST_F(FindBarViewsUiTest, ButtonsDisabledWithoutText) {
  if (browser()
          ->GetFindBarController()
          ->find_bar()
          ->HasGlobalFindPasteboard()) {
    // The presence of a global find pasteboard does not guarantee the find bar
    // will be empty on launch.
    return;
  }

  const GURL page_a = embedded_test_server()->GetURL("/a.html");

  RunTestSequence(Init(page_a), ShowFindBar(),
                  CheckViewProperty(FindBarView::kPreviousButtonElementId,
                                    &views::View::GetEnabled, false),
                  CheckViewProperty(FindBarView::kNextButtonElementId,
                                    &views::View::GetEnabled, false),
                  SendKeyPress(ui::VKEY_TAB, false, false),
                  CheckHasFocus(FindBarView::kCloseButtonElementId));
}

// TODO(crbug.com/361216144): Re-enable on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_FocusRestore DISABLED_FocusRestore
#else
#define MAYBE_FocusRestore FocusRestore
#endif
IN_PROC_BROWSER_TEST_F(FindBarViewsUiTest, MAYBE_FocusRestore) {
  const GURL page_a = embedded_test_server()->GetURL("/a.html");
  constexpr char16_t kSearchA[] = u"a";

  RunTestSequence(
      Init(page_a),

      // Set focus to the omnibox.
      Focus(kOmniboxElementId),
      // Show the Find bar.
      ShowFindBar(), CheckHasFocus(FindBarView::kTextField),
      // Dismiss the Find bar, the omnibox view should get focus.
      HideFindBar(), CheckHasFocus(kOmniboxElementId),

      // Show the Find bar and search for "a".
      ShowFindBar(),
      ObserveState(kFindResultState,
                   [this]() {
                     return find_in_page::FindTabHelper::FromWebContents(
                         browser()->tab_strip_model()->GetActiveWebContents());
                   }),
      CheckHasFocus(FindBarView::kTextField),
      EnterText(FindBarView::kTextField, kSearchA),
      WaitForState(
          kFindResultState,
          testing::Field(
              &FindResultState::active_match_ordinal,
              testing::Ne(FindResultState::kInitialActiveMatchOrdinalCount))),
      // Dismiss the Find bar, the content view should get focus.
      HideFindBar(), CheckHasFocus(ContentsWebView::kContentsWebViewElementId),

      // Focus the location bar, open and close the find box, focus should
      // return to the location bar (same as before, just checking that
      // http://crbug.com/23599 is fixed).
      Focus(kOmniboxElementId),
      // Show the Find bar.
      ShowFindBar(), CheckHasFocus(FindBarView::kTextField),
      // Dismiss the Find bar, the omnibox or web contents should get focus.
      // Since there is still text in the box, it's possible that the contents
      // pane will receive focus instead.
      HideFindBar(),
      CheckHasFocus(testing::Matcher<ui::ElementIdentifier>(testing::AnyOf(
          kOmniboxElementId, ContentsWebView::kContentsWebViewElementId))));
}

IN_PROC_BROWSER_TEST_F(FindBarViewsUiTest, SelectionRestoreOnTabSwitch) {
#if BUILDFLAG(IS_MAC)
  // Mac intentionally changes selection on focus.
  if (::testing::internal::AlwaysTrue()) {
    GTEST_SKIP() << "Mac intentionally has different behavior";
  }
#endif

  constexpr char16_t kAbc[] = u"abc";
  constexpr char16_t kBc[] = u"bc";
  constexpr char16_t kDef[] = u"def";
  constexpr char16_t kDe[] = u"de";
  const GURL page_a = embedded_test_server()->GetURL("/a.html");
  const GURL page_b = embedded_test_server()->GetURL("/b.html");

  RunTestSequence(
      // Make sure Chrome is in the foreground, otherwise sending input
      // won't do anything and the test will hang.
      // BringBrowserWindowToFront(),
      Init(page_a), WaitForWebContentsReady(kTabId),
      // Show the find bar
      ShowFindBar(),
      // Search for "abc".
      EnterText(FindBarView::kTextField, kAbc),
      CheckViewProperty(FindBarView::kElementId, &FindBarView::GetFindText,
                        kAbc),
      // Select "bc".
      // NB: SendAccelerator() didn't work here.
      SendKeyPress(ui::VKEY_LEFT, false, true),
      SendKeyPress(ui::VKEY_LEFT, false, true),
      CheckViewProperty(FindBarView::kElementId,
                        &FindBarView::GetFindSelectedText, kBc),
      // Open another tab (tab B).
      AddInstrumentedTab(kTabBId, page_b), ShowFindBar(),
      // Search for "def".
      EnterText(FindBarView::kTextField, kDef),
      CheckViewProperty(FindBarView::kElementId, &FindBarView::GetFindText,
                        kDef),
      // Select "de".
      // NB: SendAccelerator() didn't work here.
      SendKeyPress(ui::VKEY_HOME, false, false),
      SendKeyPress(ui::VKEY_RIGHT, false, true),
      SendKeyPress(ui::VKEY_RIGHT, false, true),
      CheckViewProperty(FindBarView::kElementId,
                        &FindBarView::GetFindSelectedText, kDe),
      // Select tab A. Find bar should select "bc".
      SelectTab(kTabStripElementId, 0),
      CheckViewProperty(FindBarView::kElementId,
                        &FindBarView::GetFindSelectedText, kBc),
      // Select tab B. Find bar should select "de".
      SelectTab(kTabStripElementId, 1),
      CheckViewProperty(FindBarView::kElementId,
                        &FindBarView::GetFindSelectedText, kDe));
}

IN_PROC_BROWSER_TEST_F(FindBarViewsUiTest, FocusRestoreOnTabSwitch) {
  constexpr char16_t kSearchA[] = u"a";
  constexpr char16_t kSearchB[] = u"b";
  const GURL page_a = embedded_test_server()->GetURL("/a.html");
  const GURL page_b = embedded_test_server()->GetURL("/b.html");

  RunTestSequence(
      // Open tab A and search for 'a'.
      Init(page_a), WaitForWebContentsReady(kTabId), ShowFindBar(),
      EnterText(FindBarView::kTextField, kSearchA),
      CheckViewProperty(FindBarView::kElementId, &FindBarView::GetFindText,
                        kSearchA),
      // Open another tab (tab B) and search for 'b'.
      AddInstrumentedTab(kTabBId, page_b), ShowFindBar(),
      EnterText(FindBarView::kTextField, kSearchB),
      CheckViewProperty(FindBarView::kElementId, &FindBarView::GetFindText,
                        kSearchB),
      // Set focus away from the Find bar (to the omnibox).
      Focus(kOmniboxElementId),
      // Select tab A, Find bar should get focus.
      SelectTab(kTabStripElementId, 0), CheckHasFocus(FindBarView::kTextField),
      // Select tab B, Omnibox should get focus.
      SelectTab(kTabStripElementId, 1), CheckHasFocus(kOmniboxElementId));
}

// TODO(crbug.com/361216144): Re-enable on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_FocusRestoreOnTabSwitchDismiss \
  DISABLED_FocusRestoreOnTabSwitchDismiss
#else
#define MAYBE_FocusRestoreOnTabSwitchDismiss FocusRestoreOnTabSwitchDismiss
#endif
IN_PROC_BROWSER_TEST_F(FindBarViewsUiTest,
                       MAYBE_FocusRestoreOnTabSwitchDismiss) {
  const GURL page_a = embedded_test_server()->GetURL("/a.html");
  const GURL page_b = embedded_test_server()->GetURL("/b.html");

  RunTestSequence(
      // Open tab A and show the Find bar.
      Init(page_a), ShowFindBar(), EnsurePresent(FindBarView::kElementId),
      CheckHasFocus(FindBarView::kTextField),
      // Open tab B.
      AddInstrumentedTab(kTabBId, page_b), WaitForHide(FindBarView::kTextField),
      // Switch to tab A, the Find bar should get focus.
      SelectTab(kTabStripElementId, 0), WaitForShow(FindBarView::kTextField),
      CheckHasFocus(FindBarView::kTextField),
      // Dismiss the Find bar, the content view should get focus.
      SendAccelerator(FindBarView::kTextField,
                      ui::Accelerator(ui::VKEY_ESCAPE, ui::MODIFIER_NONE)),
      WaitForHide(FindBarView::kTextField),
      CheckHasFocus(ContentsWebView::kContentsWebViewElementId));
}

// FindInPage on Mac doesn't use prepopulated values. Search there is global.
#if !BUILDFLAG(IS_MAC) && !defined(USE_AURA)
// Flaky because the test server fails to start? See: http://crbug.com/96594.
// This tests that whenever you clear values from the Find box and close it that
// it respects that and doesn't show you the last search, as reported in bug:
// http://crbug.com/40121. For Aura see bug http://crbug.com/292299.
IN_PROC_BROWSER_TEST_F(LegacyFindInPageTest, PrepopulateRespectBlank) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // First we navigate to any page.
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Show the Find bar.
  browser()->GetFindBarController()->Show();

  // Search for "a".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_A, false, false, false, false));

  // We should find "a" here.
  EXPECT_EQ(u"a", GetFindBarText());

  // Delete "a".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_BACK, false, false, false, false));

  // Validate we have cleared the text.
  EXPECT_EQ(std::u16string(), GetFindBarText());

  // Close the Find box.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));

  // Show the Find bar.
  browser()->GetFindBarController()->Show();

  // After the Find box has been reopened, it should not have been prepopulated
  // with "a" again.
  EXPECT_EQ(std::u16string(), GetFindBarText());

  // Close the Find box.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));

  // Press F3 to trigger FindNext.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F3, false, false, false, false));

  // After the Find box has been reopened, it should still have no prepopulate
  // value.
  EXPECT_EQ(std::u16string(), GetFindBarText());
}
#endif

IN_PROC_BROWSER_TEST_F(FindBarViewsUiTest, PasteWithoutTextChange) {
  constexpr char16_t kSearchA[] = u"a";
  const GURL page_a = embedded_test_server()->GetURL("/a.html");
  RunTestSequence(
      ObserveState(kFindResultState,
                   [this]() {
                     return find_in_page::FindTabHelper::FromWebContents(
                         browser()->tab_strip_model()->GetActiveWebContents());
                   }),
      // Load page + open find bar.
      Init(page_a), ShowFindBar(),
      WaitForState(views::test::kCurrentFocusedViewId, FindBarView::kTextField),
      // Search for "a".
      EnterText(FindBarView::kTextField, kSearchA),
      // We should find "a" here.
      CheckViewProperty(FindBarView::kElementId, &FindBarView::GetFindText,
                        kSearchA),
      // Reload the page to clear the matching result.
      PressButton(kReloadButtonElementId), WaitForWebContentsNavigation(kTabId),
      WaitForState(views::test::kCurrentFocusedViewId,
                   ContentsWebView::kContentsWebViewElementId),
      // Focus the Find bar again to make sure the text is selected.
      ShowFindBar(),
      WaitForState(views::test::kCurrentFocusedViewId, FindBarView::kTextField),
      // "a" should be selected.
      CheckViewProperty(FindBarView::kElementId, &FindBarView::GetFindText,
                        kSearchA),
      // Press Ctrl-C to copy the content.
      SendAccelerator(kTabId, ctrl_c_accelerator),
      // Make sure the text is copied successfully.
      PollState(
          kTextCopiedState,
          [&]() {
            ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
            std::u16string clipboard_text;
            clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste,
                                /* data_dst = */ nullptr, &clipboard_text);
            return base::EqualsASCII(clipboard_text, "a");
          }),
      WaitForState(kTextCopiedState, true),
      // Press Ctrl-V to paste the content back, it should start finding even if
      // the content is not changed.
      SendAccelerator(kTabId, ctrl_v_accelerator),
      WaitForState(
          kFindResultState,
          testing::Field(
              &FindResultState::active_match_ordinal,
              testing::Ne(FindResultState::kInitialActiveMatchOrdinalCount))));
}

// Slow flakiness on Linux. crbug.com/803743
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CtrlEnter DISABLED_CtrlEnter
#else
#define MAYBE_CtrlEnter CtrlEnter
#endif
IN_PROC_BROWSER_TEST_F(LegacyFindInPageTest, MAYBE_CtrlEnter) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,This is some text with a "
                      "<a href=\"about:blank\">link</a>.")));

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* host = web_contents->GetRenderWidgetHostView()->GetRenderWidgetHost();

  browser()->GetFindBarController()->Show();

  // Search for "link".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_L, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_I, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_N, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_K, false, false, false, false));
  content::RunUntilInputProcessed(host);

  EXPECT_EQ(u"link", GetFindBarText());

  ui_test_utils::UrlLoadObserver observer(GURL("about:blank"));

  // Send Ctrl-Enter, should cause navigation to about:blank.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RETURN, true, false, false, false));

  observer.Wait();
}

// This tests the following bug that used to exist:
// 1) Do a find that has 0 results. The search text must contain a space.
// 2) Navigate to a new page (on the same domain) that contains the search text.
// 3) Open the find bar. It should display 0/N (where N is the number of
// matches) and have no active-match highlighting. The bug caused it to display
// 1/N, with the first having active-match highlighting (and the page wouldn't
// scroll to the match if it was off-screen).
IN_PROC_BROWSER_TEST_F(LegacyFindInPageTest, ActiveMatchAfterNoResults) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/find_in_page/simple.html")));

  // This bug does not reproduce when using ui_test_utils::FindInPage here;
  // sending keystrokes like this is required. Also note that the text must
  // contain a space.
  browser()->GetFindBarController()->Show();
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_A, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_SPACE, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_L, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_I, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_N, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_K, false, false, false, false));
  EXPECT_EQ(u"a link", GetFindBarText());

  browser()->GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/find_in_page/link.html")));

  browser()->GetFindBarController()->Show();
  auto details = WaitForFindResult();
  EXPECT_EQ(1, details.number_of_matches());
  EXPECT_EQ(0, details.active_match_ordinal());
}

// TODO (crbug.com/337186755): Investigate flakiness.
IN_PROC_BROWSER_TEST_F(LegacyFindInPageTest, DISABLED_SelectionDuringFind) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/find_in_page/find_from_selection.html")));

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* host_view = web_contents->GetRenderWidgetHostView();
  auto* host = host_view->GetRenderWidgetHost();

  content::FocusChangedObserver observer(web_contents);

  // Tab to the input (which selects the text inside)
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                              false, false, false));

  observer.Wait();

  auto* find_bar_controller = browser()->GetFindBarController();
  find_bar_controller->Show();
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Verify the text matches the selection
  EXPECT_EQ(u"text", GetFindBarText());
  find_in_page::FindNotificationDetails details = WaitForFindResult();
  // We don't ever want the page to (potentially) scroll just from opening the
  // find bar, so the active match should always be 0 at this point.
  // See http://crbug.com/1043550
  EXPECT_EQ(0, details.active_match_ordinal());
  EXPECT_EQ(5, details.number_of_matches());

  // Make sure pressing an arrow key doesn't result in a find request.
  // See https://crbug.com/1127666
  auto* helper = find_in_page::FindTabHelper::FromWebContents(web_contents);
  int find_request_id = helper->current_find_request_id();
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_LEFT, false,
                                              false, false, false));
  content::RunUntilInputProcessed(host);
  EXPECT_EQ(find_request_id, helper->current_find_request_id());

  // Make sure calling Show while the findbar is already showing doesn't result
  // in a find request. It's wasted work, could cause some flicker in the
  // results, and was previously triggering another bug that caused an endless
  // loop of searching and flickering results. See http://crbug.com/1129756
  find_bar_controller->Show(false /*find_next*/);
  EXPECT_EQ(find_request_id, helper->current_find_request_id());

  // Find the next match and verify the correct match is highlighted (the
  // one after text that was selected).
  find_bar_controller->Show(true /*find_next*/);
  details = WaitForFindResult();
  EXPECT_EQ(3, details.active_match_ordinal());
  EXPECT_EQ(5, details.number_of_matches());

  // Start a new find without a selection and verify we still get find results.
  // See https://crbug.com/1124605
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));
  // Wait until the focus settles.
  content::RunUntilInputProcessed(host);

  // Shift-tab back to the input box, then clear the text (and selection).
  // Doing it this way in part because there's a bug with non-input-based
  // selection changes not affecting GetSelectedText().
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                              true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DELETE, false,
                                              false, false, false));
  content::RunUntilInputProcessed(host);
  EXPECT_EQ(std::u16string(), host_view->GetSelectedText());

  find_bar_controller->Show();
  details = WaitForFindResult();
  EXPECT_EQ(0, details.active_match_ordinal());
  // One less than before because we deleted the text in the input box.
  EXPECT_EQ(4, details.number_of_matches());
}

IN_PROC_BROWSER_TEST_F(LegacyFindInPageTest, GlobalEscapeClosesFind) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSimplePage)));

  // Open find.
  browser()->GetFindBarController()->Show(false, true);
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Put focus to the bookmarks' toolbar, which won't consume the escape key.
  chrome::FocusBookmarksToolbar(browser());

  // Close find with escape.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));

  // Find should be closed.
  EXPECT_FALSE(IsFindBarVisible());
}

IN_PROC_BROWSER_TEST_F(LegacyFindInPageTest,
                       ConsumedGlobalEscapeDoesNotCloseFind) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSimplePage)));

  // Open find.
  browser()->GetFindBarController()->Show(false, true);
  EXPECT_TRUE(IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Put focus into location bar, which will consume escape presses.
  chrome::FocusLocationBar(browser());

  // Press escape.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));

  // Find should not be closed; the escape should have been consumed by the
  // location bar.
  EXPECT_TRUE(IsFindBarVisible());
}

// See http://crbug.com/1142027
IN_PROC_BROWSER_TEST_F(FindBarViewsUiTest, MatchOrdinalStableWhileTyping) {
  const GURL page_foo =
      embedded_test_server()->GetURL("/find_in_page/foo.html");
  RunTestSequence(
      Init(page_foo), ShowFindBar(),
      ObserveState(kFindResultState,
                   [this]() {
                     return find_in_page::FindTabHelper::FromWebContents(
                         browser()->tab_strip_model()->GetActiveWebContents());
                   }),
      EnterText(FindBarView::kTextField, u"f"),
      WaitForState(kFindResultState, []() { return FindResultState(1, 3); }),
      EnterText(FindBarView::kTextField, u"o", TextEntryMode::kAppend),
      WaitForState(kFindResultState, []() { return FindResultState(1, 3); }));
}

IN_PROC_BROWSER_TEST_F(FindBarViewsUiTest, LensButton) {
  if (browser()
          ->GetFindBarController()
          ->find_bar()
          ->HasGlobalFindPasteboard()) {
    // The presence of a global find pasteboard does not guarantee the find bar
    // will be empty on launch.
    return;
  }

  constexpr char16_t kASearch[] = u"a";
  const GURL page_a = embedded_test_server()->GetURL("/a.html");

  RunTestSequence(
      // Setup test and open Find Bar.
      Init(page_a), ShowFindBar(),
      EnsurePresent(FindBarView::kLensButtonElementId),
      // Search for 'a'.
      EnterText(FindBarView::kTextField, kASearch),
      // Ensure Lens Button hides after a search is made.
      WaitForHide(FindBarView::kLensButtonElementId),
      // Delete the search text.
      SendKeyPress(ui::VKEY_BACK, false, false),
      // Ensure Lens Button comes back after no search is being made.
      WaitForShow(FindBarView::kLensButtonElementId),
      // Ensure clicking on the button triggers the Lens Overlay.
      MoveMouseTo(FindBarView::kLensButtonElementId), ClickMouse(),
      WaitForShow(kLensPermissionDialogOkButtonElementId));
}
