// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/number_formatting.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/find_bar_host.h"
#include "chrome/browser/ui/views/find_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/test/base/find_result_waiter.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/enterprise/data_controls/content/browser/last_replaced_clipboard_data.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "components/find_in_page/find_notification_details.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
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
#include "ui/base/ozone_buildflags.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_modifiers.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/view_focus_observer.h"
#include "ui/views/interaction/widget_focus_observer.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/widget_activation_waiter.h"
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
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                    kTextSelectedState);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                    kReplacedDataUpdatedState);
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
  LegacyFindInPageTest() = default;
  base::AutoReset<bool> enable_animation_for_test_ =
      FindBarHost::SetEnableAnimationsForTesting(false);

  LegacyFindInPageTest(const LegacyFindInPageTest&) = delete;
  LegacyFindInPageTest& operator=(const LegacyFindInPageTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Some bots are flaky due to slower loading interacting with
    // deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  FindBarHost* GetFindBarHost() {
    FindBar* find_bar =
        browser()->GetFeatures().GetFindBarController()->find_bar();
    return static_cast<FindBarHost*>(find_bar);
  }

  FindBarView* GetFindBarView() {
    return GetFindBarHost()->GetFindBarViewForTesting();
  }

  std::u16string_view GetFindBarText() {
    return GetFindBarHost()->GetFindText();
  }

  std::u16string_view GetFindBarSelectedText() {
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
      if (details.final_update()) {
        return details;
      }
    }
  }
};

class FindBarViewsUiTest : public InteractiveBrowserTest,
                           public ::testing::WithParamInterface<bool> {
 public:
  FindBarViewsUiTest() = default;

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
    return Check([this]() {
             return ui_test_utils::BringBrowserWindowToFront(browser());
           })
        .SetDescription("BringBrowserWindowToFront()");
  }

  auto ShowFindBar() {
    auto result =
        Steps(Do([this]() {
                browser()->GetFeatures().GetFindBarController()->Show();
              }),
              WaitForShow(FindBarView::kElementId));
    AddDescriptionPrefix(result, "ShowFindBar()");
    return result;
  }

  auto HideFindBar() {
    auto result =
        Steps(Do([this]() {
                browser()->GetFeatures().GetFindBarController()->EndFindSession(
                    find_in_page::SelectionAction::kKeep,
                    find_in_page::ResultAction::kKeep);
              }),
              WaitForHide(FindBarView::kElementId));
    AddDescriptionPrefix(result, "HideFindBar()");
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
        ObserveState(views::test::kCurrentWidgetFocus), InstrumentTab(kTabId),
        NavigateWebContents(kTabId, url));
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
                return std::string(focused->GetClassName());
              }
              return "(none)";
            }),
        WaitForState(views::test::kCurrentFocusedViewId,
                     std::forward<M>(matcher)));
    AddDescriptionPrefix(result, "CheckHasFocus()");
    return result;
  }

#define CheckHasFocus(matcher) CheckHasFocusImpl(matcher, #matcher)

  static auto Focus(ui::ElementIdentifier view) {
    auto result =
        Steps(WithView(view, [](views::View* view) { view->RequestFocus(); }),
              WaitForState(views::test::kCurrentFocusedViewId, view));
    AddDescriptionPrefix(result, "Focus()");
    return result;
  }

 protected:
  FindBarHost* GetFindBarHost() {
    FindBar* find_bar =
        browser()->GetFeatures().GetFindBarController()->find_bar();
    return static_cast<FindBarHost*>(find_bar);
  }

  bool IsFindBarVisible() { return GetFindBarHost()->IsFindBarVisible(); }

 private:
  base::AutoReset<bool> enable_animation_for_test_ =
      FindBarHost::SetEnableAnimationsForTesting(false);
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
  browser()->GetFeatures().GetFindBarController()->Show();

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

  ui::AXNodeData root_view_data;
  GetFindBarHost()
      ->GetWidget()
      ->GetRootView()
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&root_view_data);
  EXPECT_EQ(
      root_view_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      GetFindBarHost()->GetAccessibleWindowTitle());
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
  browser()->GetFeatures().GetFindBarController()->Show();
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
          ->GetFeatures()
          .GetFindBarController()
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

// Test for crbug.com/40164081. When a tab has find bar and the web content has
// focus, the web content should retain the focus after switching the tab away
// and then back.
IN_PROC_BROWSER_TEST_F(FindBarViewsUiTest,
                       FocusRetainedOnPageWhenFindBarIsOpenOnTabSwitch) {
  const GURL page_a = embedded_test_server()->GetURL("/a.html");
  const GURL page_b = embedded_test_server()->GetURL("/b.html");

  RunTestSequence(
      // Open tab A and show the Find bar.
      Init(page_a), ShowFindBar(), EnsurePresent(FindBarView::kElementId),
      CheckHasFocus(FindBarView::kTextField),
      // Focus tab A content.
      Focus(ContentsWebView::kContentsWebViewElementId),
      CheckHasFocus(ContentsWebView::kContentsWebViewElementId),
      // Open tab B.
      AddInstrumentedTab(kTabBId, page_b), WaitForHide(FindBarView::kTextField),
      // Switch to tab A
      SelectTab(kTabStripElementId, 0), WaitForShow(FindBarView::kTextField),
      // The browser frame should be active.
      WaitForState(views::test::kCurrentWidgetFocus,
                   [this]() {
                     return BrowserView::GetBrowserViewForBrowser(browser())
                         ->GetWidget();
                   }),
      // The content view should be focused.
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
  browser()->GetFeatures().GetFindBarController()->Show();

  // Search for "a".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_A, false,
                                              false, false, false));

  // We should find "a" here.
  EXPECT_EQ(u"a", GetFindBarText());

  // Delete "a".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_BACK, false,
                                              false, false, false));

  // Validate we have cleared the text.
  EXPECT_EQ(std::u16string(), GetFindBarText());

  // Close the Find box.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));

  // Show the Find bar.
  browser()->GetFeatures().GetFindBarController()->Show();

  // After the Find box has been reopened, it should not have been prepopulated
  // with "a" again.
  EXPECT_EQ(std::u16string(), GetFindBarText());

  // Close the Find box.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));

  // Press F3 to trigger FindNext.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_F3, false,
                                              false, false, false));

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
      // TODO(crbug.com/479732140): improve the test method to simplify the call.
      MoveMouseTo(kReloadButtonElementId,
#if !BUILDFLAG(IS_ANDROID)
                  features::IsWebUIReloadButtonEnabled()
                      ? RelativePositionSpecifier(
                            base::BindOnce([](ui::TrackedElement* el) {
                              return el->GetScreenBounds().CenterPoint();
                            }))
                      : CenterPoint()
#else
                  CenterPoint()
#endif  // !BUILDFLAG(IS_ANDROID)
                      ),
      ClickMouse(), WaitForWebContentsNavigation(kTabId),
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

  browser()->GetFeatures().GetFindBarController()->Show();

  // Search for "link".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_L, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_I, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_N, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_K, false,
                                              false, false, false));
  content::RunUntilInputProcessed(host);

  EXPECT_EQ(u"link", GetFindBarText());

  ui_test_utils::UrlLoadObserver observer(GURL("about:blank"));

  // Send Ctrl-Enter, should cause navigation to about:blank.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN, true,
                                              false, false, false));

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
  browser()->GetFeatures().GetFindBarController()->Show();
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_A, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_SPACE, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_L, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_I, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_N, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_K, false,
                                              false, false, false));
  EXPECT_EQ(u"a link", GetFindBarText());

  browser()->GetFeatures().GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/find_in_page/link.html")));

  browser()->GetFeatures().GetFindBarController()->Show();
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

  auto* find_bar_controller = browser()->GetFeatures().GetFindBarController();
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
  browser()->GetFeatures().GetFindBarController()->Show(false, true);
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
  browser()->GetFeatures().GetFindBarController()->Show(false, true);
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

INSTANTIATE_TEST_SUITE_P(, FindBarViewsUiTest, ::testing::Bool());

IN_PROC_BROWSER_TEST_P(FindBarViewsUiTest, SelectionDuringFindPolicy) {
  const bool clipboard_restricted_by_policy = GetParam();
  if (clipboard_restricted_by_policy) {
    data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "rule_name",
                                   "rule_id": "rule_id",
                                   "destinations": {
                                     "os_clipboard": true
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "BLOCK"}
                                   ]
                                 })"});
  }
  const std::u16string kExpectedText = u"This is some text with a link.";
  const std::u16string text =
      clipboard_restricted_by_policy ? u"42" : kExpectedText;

  const GURL page_url = embedded_test_server()->GetURL("/a.html");

  RunTestSequence(
      Init(page_url), WaitForWebContentsReady(kTabId),

      CheckJsResult(kTabId, "() => document.body.innerText",
                    testing::HasSubstr(base::UTF16ToUTF8(kExpectedText))),

      ShowFindBar(), EnterText(FindBarView::kTextField, u"42"), HideFindBar(),
      Focus(ContentsWebView::kContentsWebViewElementId),

      // Select all text.
      Do([this]() {
        browser()->tab_strip_model()->GetActiveWebContents()->SelectAll();
      }),

      // Verify the selection in the renderer.
      WaitForJsResult(kTabId, "() => window.getSelection().toString()",
                      base::UTF16ToUTF8(kExpectedText)),

      // Wait for the browser to be aware of the selection.
      PollState(kTextSelectedState,
                [this, kExpectedText]() {
                  WebContents* web_contents =
                      browser()->tab_strip_model()->GetActiveWebContents();
                  if (!web_contents) {
                    return false;
                  }
                  auto* host_view = web_contents->GetRenderWidgetHostView();
                  if (!host_view) {
                    return false;
                  }
                  if (host_view->GetSelectedText() != kExpectedText) {
                    return false;
                  }
                  return true;
                }),
      WaitForState(kTextSelectedState, true),

      // Show the Find bar.
      ShowFindBar(), WaitForShow(FindBarView::kTextField),
      // Verify the text in the find bar.
      WithView(FindBarView::kTextField, [text](views::Textfield* textfield) {
        EXPECT_EQ(textfield->GetText(), text);
      }));
}

// Verifies that the find bar widget is not activatable.
//
// The original fix for crbug.com/40616214 added Activatable::kYes to the
// find bar widget(on macOS). However, this caused multiple other issues. We now
// remove Activatable::kYes and instead use ActivateOwnerWidgetIfNecessary to
// activate the browser window when clicking on the find bar textfield.
//
// Removing Activatable::kYes fixes the following bugs:
// - crbug.com/40205173
// - crbug.com/40147557
// - crbug.com/40694525
// - crbug.com/442293378
// - crbug.com/422444253
IN_PROC_BROWSER_TEST_F(FindBarViewsUiTest, FindBarWidgetIsNotActivatable) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Navigate to a simple page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSimplePage)));

  // Show the find bar.
  browser()->GetFeatures().GetFindBarController()->Show();
  ASSERT_TRUE(IsFindBarVisible());

  // Get the find bar widget.
  views::Widget* find_bar_widget = GetFindBarHost()->GetHostWidget();
  ASSERT_NE(nullptr, find_bar_widget);

  // Verify that the find bar widget is not activatable.
  // This is the key assertion - we intentionally do not set
  // Activatable::kYes because doing so causes other bugs.
  EXPECT_FALSE(find_bar_widget->CanActivate());
}

// Verifies that crbug.com/40616214 is not affected by removing
// Activatable::kYes.
//
// After removing Activatable::kYes, we use ActivateOwnerWidgetIfNecessary
// to activate the browser window when clicking on the find bar textfield (on
// macOS). This test verifies that clicking on the find bar textfield in an
// inactive browser window properly activates that window and allows text input.
//
// Disabled on Linux Wayland: Linux Wayland doesn't support window activation.
// See crbug.com/40863331.
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
#define MAYBE_FindBarTextfieldActivatesBrowserOnClick \
  DISABLED_FindBarTextfieldActivatesBrowserOnClick
#else
#define MAYBE_FindBarTextfieldActivatesBrowserOnClick \
  FindBarTextfieldActivatesBrowserOnClick
#endif
IN_PROC_BROWSER_TEST_F(FindBarViewsUiTest,
                       MAYBE_FindBarTextfieldActivatesBrowserOnClick) {
  // Browser A: The browser window that comes with the test fixture.
  Browser* browser_a = browser();
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser_a));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser_a, embedded_test_server()->GetURL(kSimplePage)));

  // Show the find bar in browser A.
  browser_a->GetFeatures().GetFindBarController()->Show();
  ASSERT_TRUE(GetFindBarHost()->IsFindBarVisible());

  // Clear any existing text in the find bar.
  FindBarView* find_bar_view = GetFindBarHost()->GetFindBarViewForTesting();
  views::Textfield* textfield = static_cast<views::Textfield*>(
      find_bar_view->GetViewByID(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  textfield->SetText(std::u16string());
  ASSERT_TRUE(textfield->GetText().empty());

  // Create browser B and make it active with focus in the omnibox.
  Browser* browser_b = CreateBrowser(browser_a->profile());
  ASSERT_NE(nullptr, browser_b);

  views::Widget* browser_a_widget =
      BrowserView::GetBrowserViewForBrowser(browser_a)->GetWidget();
  views::Widget* browser_b_widget =
      BrowserView::GetBrowserViewForBrowser(browser_b)->GetWidget();

  // Position browser windows so they don't overlap. Browser A stays at its
  // current position, browser B is moved to a small size at a different
  // location. This ensures clicks on browser A's find bar actually reach it.
  gfx::Rect browser_a_bounds = browser_a_widget->GetWindowBoundsInScreen();
  browser_b_widget->SetBounds(
      gfx::Rect(browser_a_bounds.right(), browser_a_bounds.y(), 200, 200));

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser_b));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser_b, embedded_test_server()->GetURL(kSimplePage)));

  // Focus browser B's omnibox to simulate user having focus there.
  chrome::FocusLocationBar(browser_b);

  // Verify browser B is now active and browser A is not.
  EXPECT_FALSE(browser_a_widget->IsActive());
  EXPECT_TRUE(browser_b_widget->IsActive());

  // Click on browser A's find bar textfield.
  ui_test_utils::ClickOnView(textfield);

  // After clicking, browser A should be active.
  views::test::WaitForWidgetActive(browser_a_widget, true);

  // Record the text length before sending keyboard input.
  const size_t text_length_before = textfield->GetText().length();

  // Now send keyboard input - it should go to browser A's find bar textfield.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser_a, ui::VKEY_A,
                                              /*control=*/false,
                                              /*shift=*/false,
                                              /*alt=*/false,
                                              /*command=*/false));

  // Verify the text was entered into browser A's find bar textfield,
  // not browser B's omnibox. The key assertion is that the textfield
  // received the input (text length increased), confirming focus is correct.
  EXPECT_GT(textfield->GetText().length(), text_length_before);
}

// When the find bar has focus, Cmd+D (bookmark shortcut) should work and show
// the bookmark bubble.
IN_PROC_BROWSER_TEST_F(FindBarViewsUiTest, BookmarkShortcutWithFindBarFocus) {
  const GURL page_a = embedded_test_server()->GetURL("/a.html");

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

  RunTestSequence(
      Init(page_a), ShowFindBar(), EnterText(FindBarView::kTextField, u"test"),
      CheckHasFocus(FindBarView::kTextField),

      // Verify the page is not bookmarked initially.
      Check([&]() { return !bookmark_model->IsBookmarked(page_a); }),

      // Cmd+D is a main menu command (Bookmark This Tab). It should be routed
      // to the main menu and show the bookmark bubble.
      Check([this]() {
        return ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_D,
#if BUILDFLAG(IS_MAC)
                                               /*control=*/false,
                                               /*shift=*/false,
                                               /*alt=*/false,
                                               /*command=*/true
#else
                                               /*control=*/true,
                                               /*shift=*/false,
                                               /*alt=*/false,
                                               /*command=*/false
#endif
        );
      }),

      // Verify the bookmark bubble is shown. This is the core of the bug fix:
      // without the fix, Cmd+D would be consumed by the text field and the
      // bubble would never appear.
      WaitForShow(kBookmarkNameFieldId));
}

IN_PROC_BROWSER_TEST_P(FindBarViewsUiTest, CopyBlockedByPolicy) {
  const bool clipboard_restricted_by_policy = GetParam();
  if (clipboard_restricted_by_policy) {
    data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "rule_name",
                                   "rule_id": "rule_id",
                                   "destinations": {
                                     "os_clipboard": true
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "BLOCK"}
                                   ]
                                 })"});
  }
  const std::string kExpectedText =
      clipboard_restricted_by_policy
          ? l10n_util::GetStringUTF8(
                IDS_ENTERPRISE_DATA_CONTROLS_COPY_PREVENTION_WARNING_MESSAGE)
          : "text";

  RunTestSequence(
      Init(embedded_test_server()->GetURL("/a.html")),
      WaitForWebContentsReady(kTabId), ShowFindBar(),
      WaitForShow(FindBarView::kTextField),
      EnterText(FindBarView::kTextField, u"some text"),
      WithView(FindBarView::kTextField,
               [](views::Textfield* textfield) {
                 textfield->SelectWord();
                 EXPECT_EQ(textfield->GetSelectedText(), u"text");
                 textfield->ExecuteCommand(
                     std::to_underlying(ui::TouchEditable::MenuCommands::kCopy),
                     0);
               }),
      PollState(
          kTextCopiedState,
          [&]() {
            ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
            std::u16string clipboard_text;
            clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste,
                                /* data_dst = */ nullptr, &clipboard_text);
            return base::EqualsASCII(clipboard_text, kExpectedText);
          }),
      WaitForState(kTextCopiedState, true),
      // When copying to the clipboard is restricted, we have to wait for the
      // internal data tracking to identify the sequence number that will need
      // to be replaced before pasting.
      PollState(
          kReplacedDataUpdatedState,
          [&]() {
            return !clipboard_restricted_by_policy ||
                   ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
                       ui::ClipboardBuffer::kCopyPaste) ==
                       data_controls::GetLastReplacedClipboardData().seqno;
          }),
      WaitForState(kReplacedDataUpdatedState, true),
      // Regardless of whether the copied data made it to the clipboard, pasting
      // it back into the FindBar will result in getting the original text back
      // as the current policy doesn't block it.
      WithView(FindBarView::kTextField, [&](views::Textfield* textfield) {
        textfield->ExecuteCommand(
            std::to_underlying(ui::TouchEditable::MenuCommands::kPaste), 0);
        ASSERT_EQ(textfield->GetText(), u"some text");
      }));
}
