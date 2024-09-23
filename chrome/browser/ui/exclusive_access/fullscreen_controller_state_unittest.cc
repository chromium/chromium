// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_state_test.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_constants.h"

// The FullscreenControllerStateUnitTest unit test suite exhastively tests
// the FullscreenController through all permutations of events. The behavior
// of the BrowserWindow is mocked via FullscreenControllerTestWindow.

namespace {

// FullscreenControllerTestWindow ----------------------------------------------

// A BrowserWindow used for testing FullscreenController. The behavior of this
// mock is verified manually by running
// FullscreenControllerStateInteractiveTest.
class FullscreenControllerTestWindow : public TestBrowserWindow,
                                       ExclusiveAccessContext {
 public:
  // Simulate the window state with an enumeration.
  enum WindowState {
    NORMAL,
    FULLSCREEN,
    TO_NORMAL,
    TO_FULLSCREEN,
  };

  FullscreenControllerTestWindow();
  ~FullscreenControllerTestWindow() override {}

  // BrowserWindow Interface:
  bool ShouldHideUIForFullscreen() const override;
  bool IsFullscreen() const override;
  static const char* GetWindowStateString(WindowState state);
  WindowState state() const { return state_; }
  void set_browser(Browser* browser) { browser_ = browser; }
  ExclusiveAccessContext* GetExclusiveAccessContext() override;

  // ExclusiveAccessContext Interface:
  Profile* GetProfile() override;
  content::WebContents* GetWebContentsForExclusiveAccess() override;
  void EnterFullscreen(const GURL& url,
                       ExclusiveAccessBubbleType type,
                       int64_t display_id) override;
  void ExitFullscreen() override;
  void UpdateExclusiveAccessBubble(
      const ExclusiveAccessBubbleParams& params,
      ExclusiveAccessBubbleHideCallback first_hide_callback) override;
  bool IsExclusiveAccessBubbleDisplayed() const override;
  void OnExclusiveAccessUserInput() override;
  bool CanUserExitFullscreen() const override;

  // Simulates the window changing state.
  void ChangeWindowFullscreenState();

 private:
  void EnterFullscreen();

  // Returns true if ChangeWindowFullscreenState() should be called as a result
  // of updating the current fullscreen state to the passed in state.
  bool IsTransitionReentrant(bool new_fullscreen);

  WindowState state_;
  raw_ptr<Browser, DanglingUntriaged> browser_;
};

FullscreenControllerTestWindow::FullscreenControllerTestWindow()
    : state_(NORMAL), browser_(nullptr) {}

void FullscreenControllerTestWindow::EnterFullscreen(
    const GURL& url,
    ExclusiveAccessBubbleType type,
    int64_t display_id) {
  EnterFullscreen();
}

void FullscreenControllerTestWindow::ExitFullscreen() {
  if (IsFullscreen()) {
    state_ = TO_NORMAL;

    if (IsTransitionReentrant(false))
      ChangeWindowFullscreenState();
  }
}

bool FullscreenControllerTestWindow::ShouldHideUIForFullscreen() const {
  return IsFullscreen();
}

bool FullscreenControllerTestWindow::IsFullscreen() const {
#if BUILDFLAG(IS_MAC)
  return state_ == FULLSCREEN || state_ == TO_FULLSCREEN;
#else
  return state_ == FULLSCREEN || state_ == TO_NORMAL;
#endif
}

// static
const char* FullscreenControllerTestWindow::GetWindowStateString(
    WindowState state) {
  switch (state) {
    ENUM_TO_STRING(NORMAL);
    ENUM_TO_STRING(FULLSCREEN);
    ENUM_TO_STRING(TO_FULLSCREEN);
    ENUM_TO_STRING(TO_NORMAL);
    default:
      NOTREACHED_IN_MIGRATION() << "No string for state " << state;
      return "WindowState-Unknown";
  }
}

void FullscreenControllerTestWindow::ChangeWindowFullscreenState() {
  // Most states result in "no operation" intentionally. The tests
  // assume that all possible states and event pairs can be tested, even
  // though window managers will not generate all of these.
  if (state_ == TO_FULLSCREEN)
      state_ = FULLSCREEN;
  else if (state_ == TO_NORMAL)
      state_ = NORMAL;

  // Emit a change event from every state to ensure the Fullscreen Controller
  // handles it in all circumstances.
  browser_->WindowFullscreenStateChanged();
}

void FullscreenControllerTestWindow::EnterFullscreen() {
  bool reentrant = IsTransitionReentrant(true);

  if (!IsFullscreen())
    state_ = TO_FULLSCREEN;

  if (reentrant)
    ChangeWindowFullscreenState();
}

bool FullscreenControllerTestWindow::IsTransitionReentrant(
    bool new_fullscreen) {
  bool fullscreen_changed = (new_fullscreen != IsFullscreen());

  if (!fullscreen_changed)
    return false;

  if (FullscreenControllerStateTest::IsWindowFullscreenStateChangedReentrant())
    return true;

  // BrowserWindowCocoa::EnterFullscreen() and
  // BrowserWindowCocoa::EnterFullscreenWithToolbar() are reentrant when
  // switching between fullscreen with chrome and fullscreen without chrome.
  return state_ == FULLSCREEN && !fullscreen_changed;
}

ExclusiveAccessContext*
FullscreenControllerTestWindow::GetExclusiveAccessContext() {
  return this;
}

Profile* FullscreenControllerTestWindow::GetProfile() {
  return browser_->profile();
}

content::WebContents*
FullscreenControllerTestWindow::GetWebContentsForExclusiveAccess() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void FullscreenControllerTestWindow::UpdateExclusiveAccessBubble(
    const ExclusiveAccessBubbleParams& params,
    ExclusiveAccessBubbleHideCallback first_hide_callback) {}

bool FullscreenControllerTestWindow::IsExclusiveAccessBubbleDisplayed() const {
  return false;
}

void FullscreenControllerTestWindow::OnExclusiveAccessUserInput() {}

bool FullscreenControllerTestWindow::CanUserExitFullscreen() const {
  return true;
}

}  // namespace

// FullscreenControllerStateUnitTest -------------------------------------------

// Unit test fixture testing Fullscreen Controller through its states. Most of
// the test logic comes from FullscreenControllerStateTest.
class FullscreenControllerStateUnitTest : public BrowserWithTestWindowTest,
                                          public FullscreenControllerStateTest {
 public:
  FullscreenControllerStateUnitTest();

  FullscreenControllerStateUnitTest(const FullscreenControllerStateUnitTest&) =
      delete;
  FullscreenControllerStateUnitTest& operator=(
      const FullscreenControllerStateUnitTest&) = delete;

  // FullscreenControllerStateTest:
  void SetUp() override;
  void TearDown() override;
  std::unique_ptr<BrowserWindow> CreateBrowserWindow() override;
  void ChangeWindowFullscreenState() override;
  const char* GetWindowStateString() override;
  void VerifyWindowState() override;

 protected:
  // FullscreenControllerStateTest:
  bool ShouldSkipStateAndEventPair(State state, Event event) override;
  Browser* GetBrowser() override;
  raw_ptr<FullscreenControllerTestWindow, DanglingUntriaged> window_ = nullptr;
};

FullscreenControllerStateUnitTest::FullscreenControllerStateUnitTest() =
    default;

void FullscreenControllerStateUnitTest::SetUp() {
  BrowserWithTestWindowTest::SetUp();
  window_->set_browser(browser());
}

void FullscreenControllerStateUnitTest::TearDown() {
  FullscreenControllerStateTest::TearDown();
  BrowserWithTestWindowTest::TearDown();
}

std::unique_ptr<BrowserWindow>
FullscreenControllerStateUnitTest::CreateBrowserWindow() {
  auto window = std::make_unique<FullscreenControllerTestWindow>();
  window_ = window.get();
  return window;
}

void FullscreenControllerStateUnitTest::ChangeWindowFullscreenState() {
  window_->ChangeWindowFullscreenState();
}

const char* FullscreenControllerStateUnitTest::GetWindowStateString() {
  return FullscreenControllerTestWindow::GetWindowStateString(window_->state());
}

void FullscreenControllerStateUnitTest::VerifyWindowState() {
  switch (state()) {
    case STATE_NORMAL:
      EXPECT_EQ(FullscreenControllerTestWindow::NORMAL,
                window_->state()) << GetAndClearDebugLog();
      break;

    case STATE_BROWSER_FULLSCREEN:
    case STATE_TAB_FULLSCREEN:
    case STATE_TAB_BROWSER_FULLSCREEN:
      EXPECT_EQ(FullscreenControllerTestWindow::FULLSCREEN,
                window_->state()) << GetAndClearDebugLog();
      break;

    case STATE_TO_NORMAL:
      EXPECT_EQ(FullscreenControllerTestWindow::TO_NORMAL,
                window_->state()) << GetAndClearDebugLog();
      break;

    case STATE_TO_BROWSER_FULLSCREEN:
    case STATE_TO_TAB_FULLSCREEN:
      EXPECT_EQ(FullscreenControllerTestWindow::TO_FULLSCREEN,
                window_->state()) << GetAndClearDebugLog();
      break;

    default:
      NOTREACHED_IN_MIGRATION() << GetAndClearDebugLog();
  }

  FullscreenControllerStateTest::VerifyWindowState();
}

bool FullscreenControllerStateUnitTest::ShouldSkipStateAndEventPair(
    State state, Event event) {
#if BUILDFLAG(IS_MAC)
  // TODO(scheib) Toggle, Window Event, Toggle, Toggle on Mac as exposed by
  // test *.STATE_TO_NORMAL__TOGGLE_FULLSCREEN runs interactively and exits to
  // Normal. This doesn't appear to be the desired result, and would add
  // too much complexity to mimic in our simple FullscreenControllerTestWindow.
  // http://crbug.com/156968
  if ((state == STATE_TO_BROWSER_FULLSCREEN ||
       state == STATE_TO_TAB_FULLSCREEN) &&
      event == TOGGLE_FULLSCREEN)
    return true;
#endif

  return FullscreenControllerStateTest::ShouldSkipStateAndEventPair(state,
                                                                    event);
}

Browser* FullscreenControllerStateUnitTest::GetBrowser() {
  return BrowserWithTestWindowTest::browser();
}

// Soak tests ------------------------------------------------------------------

// Tests all states with all permutations of multiple events to detect lingering
// state issues that would bleed over to other states.
// I.E. for each state test all combinations of events E1, E2, E3.
//
// This produces coverage for event sequences that may happen normally but
// would not be exposed by traversing to each state via TransitionToState().
// TransitionToState() always takes the same path even when multiple paths
// exist.
TEST_F(FullscreenControllerStateUnitTest, TransitionsForEachState) {
  // A tab is needed for tab fullscreen.
  AddTab(browser(), GURL(url::kAboutBlankURL));
  TestTransitionsForEachState();
  // Progress of test can be examined via LOG(INFO) << GetAndClearDebugLog();
}


// Individual tests for each pair of state and event ---------------------------

#define TEST_EVENT(state, event)                                \
  TEST_F(FullscreenControllerStateUnitTest, state##__##event) { \
    AddTab(browser(), GURL(url::kAboutBlankURL));               \
    ASSERT_NO_FATAL_FAILURE(TestStateAndEvent(state, event))    \
        << GetAndClearDebugLog();                               \
  }
    // Progress of tests can be examined by inserting the following line:
    // LOG(INFO) << GetAndClearDebugLog(); }

#include "chrome/browser/ui/exclusive_access/fullscreen_controller_state_tests.h"


// Specific one-off tests for known issues -------------------------------------

// TODO(scheib) Toggling Tab fullscreen while pending Tab or
// Browser fullscreen is broken currently http://crbug.com/154196
TEST_F(FullscreenControllerStateUnitTest,
       DISABLED_ToggleTabWhenPendingBrowser) {
  // Only possible without reentrancy.
  if (FullscreenControllerStateTest::IsWindowFullscreenStateChangedReentrant())
    return;
  AddTab(browser(), GURL(url::kAboutBlankURL));
  ASSERT_NO_FATAL_FAILURE(
      TransitionToState(STATE_TO_BROWSER_FULLSCREEN))
      << GetAndClearDebugLog();

  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(EXIT_TAB_FULLSCREEN)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE)) << GetAndClearDebugLog();
}

// TODO(scheib) Toggling Tab fullscreen while pending Tab or
// Browser fullscreen is broken currently http://crbug.com/154196
TEST_F(FullscreenControllerStateUnitTest, DISABLED_ToggleTabWhenPendingTab) {
  // Only possible without reentrancy.
  if (FullscreenControllerStateTest::IsWindowFullscreenStateChangedReentrant())
    return;
  AddTab(browser(), GURL(url::kAboutBlankURL));
  ASSERT_NO_FATAL_FAILURE(
      TransitionToState(STATE_TO_TAB_FULLSCREEN))
      << GetAndClearDebugLog();

  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(EXIT_TAB_FULLSCREEN)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE)) << GetAndClearDebugLog();
}

// Debugging utility: Display the transition tables. Intentionally disabled
TEST_F(FullscreenControllerStateUnitTest, DISABLED_DebugLogStateTables) {
  std::ostringstream output;
  output << "\n\nTransition Table:";
  output << GetTransitionTableAsString();

  output << "\n\nInitial transitions:";
  output << GetStateTransitionsAsString();

  // Calculate all transition pairs.
  for (int state1_int = 0; state1_int < NUM_STATES; ++state1_int) {
    State state1 = static_cast<State>(state1_int);
    for (int state2_int = 0; state2_int < NUM_STATES; ++state2_int) {
      State state2 = static_cast<State>(state2_int);
      if (ShouldSkipStateAndEventPair(state1, EVENT_INVALID) ||
          ShouldSkipStateAndEventPair(state2, EVENT_INVALID))
        continue;
      // Compute the transition
      if (NextTransitionInShortestPath(state1, state2, NUM_STATES).state ==
          STATE_INVALID) {
        LOG(ERROR) << "Should be skipping state transitions for: "
            << GetStateString(state1) << " " << GetStateString(state2);
      }
    }
  }

  output << "\n\nAll transitions:";
  output << GetStateTransitionsAsString();
  LOG(INFO) << output.str();
}

// Test that the fullscreen exit bubble is closed by
// WindowFullscreenStateChanged() if fullscreen is exited via the
// ExclusiveAccessContext interface.
TEST_F(FullscreenControllerStateUnitTest,
       ExitFullscreenViaExclusiveAccessContext) {
  AddTab(browser(), GURL(url::kAboutBlankURL));
  ASSERT_TRUE(InvokeEvent(TOGGLE_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(browser()->window()->IsFullscreen());
  // Exit fullscreen without going through fullscreen controller.
  window_->ExitFullscreen();
  ChangeWindowFullscreenState();
  EXPECT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE,
            browser()
                ->exclusive_access_manager()
                ->GetExclusiveAccessExitBubbleType());
}

// Tests that RunOrDeferUntilTransitionIsComplete runs the lambda when nothing
// is happening (no transition in progress).
TEST_F(FullscreenControllerStateUnitTest,
       RunOrDeferUntilTransitionIsCompleteNow) {
  AddTab(browser(), GURL(url::kAboutBlankURL));
  bool lambda_called = false;
  GetFullscreenController()->RunOrDeferUntilTransitionIsComplete(
      base::BindLambdaForTesting([&lambda_called]() { lambda_called = true; }));
  EXPECT_TRUE(lambda_called);
}

// Tests that RunOrDeferUntilTransitionIsComplete does not run the lambda while
// a transition is in progress and runs it after the transition completes.
TEST_F(FullscreenControllerStateUnitTest,
       RunOrDeferUntilTransitionIsCompleteDefer) {
  AddTab(browser(), GURL(url::kAboutBlankURL));
  GetFullscreenController()->ToggleBrowserFullscreenMode();
  bool lambda_called = false;
  GetFullscreenController()->RunOrDeferUntilTransitionIsComplete(
      base::BindLambdaForTesting([&lambda_called]() { lambda_called = true; }));
  EXPECT_FALSE(lambda_called);
  GetFullscreenController()->FullscreenTransitionCompleted();
  EXPECT_TRUE(lambda_called);
}

TEST_F(FullscreenControllerStateUnitTest, GetFullscreenState) {
  AddTab(browser(), GURL(url::kAboutBlankURL));
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  auto state = GetFullscreenController()->GetFullscreenState(tab);
  EXPECT_EQ(state.target_mode, content::FullscreenMode::kWindowed);
  EXPECT_EQ(state.target_display_id, display::kInvalidDisplayId);

  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  state = GetFullscreenController()->GetFullscreenState(tab);
  EXPECT_EQ(state.target_mode, content::FullscreenMode::kContent);
  EXPECT_NE(state.target_display_id, display::kInvalidDisplayId);

  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  state = GetFullscreenController()->GetFullscreenState(tab);
  EXPECT_EQ(state.target_mode, content::FullscreenMode::kContent);
  EXPECT_NE(state.target_display_id, display::kInvalidDisplayId);

  ASSERT_TRUE(InvokeEvent(EXIT_TAB_FULLSCREEN));
  state = GetFullscreenController()->GetFullscreenState(tab);
  EXPECT_EQ(state.target_mode, content::FullscreenMode::kWindowed);
  EXPECT_EQ(state.target_display_id, display::kInvalidDisplayId);

  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  state = GetFullscreenController()->GetFullscreenState(tab);
  EXPECT_EQ(state.target_mode, content::FullscreenMode::kWindowed);
  EXPECT_EQ(state.target_display_id, display::kInvalidDisplayId);
}

// Test that switching tabs takes the browser out of tab fullscreen.
TEST_F(FullscreenControllerStateUnitTest, ExitTabFullscreenViaSwitchingTab) {
  AddTab(browser(), GURL(url::kAboutBlankURL));
  AddTab(browser(), GURL(url::kAboutBlankURL));
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(browser()->window()->IsFullscreen());

  browser()->tab_strip_model()->SelectNextTab();
  ChangeWindowFullscreenState();
  EXPECT_FALSE(browser()->window()->IsFullscreen());
}

// Test that switching tabs via detaching the active tab (which is in tab
// fullscreen) takes the browser out of tab fullscreen. This case can
// occur if the user is in both tab fullscreen and immersive browser fullscreen.
TEST_F(FullscreenControllerStateUnitTest, ExitTabFullscreenViaDetachingTab) {
  AddTab(browser(), GURL(url::kAboutBlankURL));
  AddTab(browser(), GURL(url::kAboutBlankURL));
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(browser()->window()->IsFullscreen());

  std::unique_ptr<tabs::TabModel> detached_tab =
      browser()->tab_strip_model()->DetachTabAtForInsertion(0);
  ChangeWindowFullscreenState();
  EXPECT_FALSE(browser()->window()->IsFullscreen());
}

// Tests that, in a browser configured for Fullscreen-Within-Tab mode,
// fullscreening a screen-captured tab will NOT cause any fullscreen state
// change to the browser window. Furthermore, the test switches between tabs to
// confirm a captured tab will be resized by FullscreenController to the capture
// video resolution once the widget is detached from the UI.
//
// See 'FullscreenWithinTab Note' in fullscreen_controller.h.
TEST_F(FullscreenControllerStateUnitTest, OneCapturedFullscreenedTab) {
  AddTab(browser(), GURL(url::kAboutBlankURL));
  AddTab(browser(), GURL(url::kAboutBlankURL));
  content::WebContents* const first_tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  content::WebContents* const second_tab =
      browser()->tab_strip_model()->GetWebContentsAt(1);

  // Activate the first tab and tell its WebContents it is being captured.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  const gfx::Size kCaptureSize(1280, 720);
  auto capture_handle = first_tab->IncrementCapturerCount(
      kCaptureSize, /*stay_hidden=*/false,
      /*stay_awake=*/true, /*is_activity=*/true);
  ASSERT_FALSE(browser()->window()->IsFullscreen());
  ASSERT_FALSE(first_tab->IsFullscreen());
  ASSERT_FALSE(second_tab->IsFullscreen());
  ASSERT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Enter tab fullscreen.  Since the tab is being captured, the browser window
  // should not expand to fill the screen.
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(first_tab->IsFullscreen());
  EXPECT_FALSE(second_tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Switch to the other tab.  Check that the first tab was resized to the
  // WebContents' preferred size.
  browser()->tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(first_tab->IsFullscreen());
  EXPECT_FALSE(second_tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  EXPECT_EQ(kCaptureSize, first_tab->GetViewBounds().size());

  // Switch back to the first tab and exit fullscreen.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(first_tab->IsFullscreen());
  EXPECT_FALSE(second_tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  ASSERT_TRUE(InvokeEvent(EXIT_TAB_FULLSCREEN));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_FALSE(first_tab->IsFullscreen());
  EXPECT_FALSE(second_tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
}

// Tests that, in a browser configured for Fullscreen-Within-Tab mode, more than
// one tab can be in fullscreen mode at the same time without interfering with
// each other.  One tab is being screen-captured and is toggled into fullscreen
// mode, and then the user switches to another tab not being screen-captured and
// fullscreens it.  The first tab's fullscreen toggle does not affect the
// browser window fullscreen, while the second one's does.  Then, the order of
// operations is reversed.
//
// See 'FullscreenWithinTab Note' in fullscreen_controller.h.
TEST_F(FullscreenControllerStateUnitTest, TwoFullscreenedTabsOneCaptured) {
  AddTab(browser(), GURL(url::kAboutBlankURL));
  AddTab(browser(), GURL(url::kAboutBlankURL));
  content::WebContents* const first_tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  content::WebContents* const second_tab =
      browser()->tab_strip_model()->GetWebContentsAt(1);

  // Start capturing the first tab, fullscreen it, then switch to the second tab
  // and fullscreen that.  The second tab will cause the browser window to
  // expand to fill the screen.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  const gfx::Size kCaptureSize(1280, 720);
  auto capture_handle = first_tab->IncrementCapturerCount(
      kCaptureSize, /*stay_hidden=*/false,
      /*stay_awake=*/true, /*is_activity=*/true);
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(first_tab->IsFullscreen());
  EXPECT_FALSE(second_tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  browser()->tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  EXPECT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(first_tab->IsFullscreen());
  EXPECT_TRUE(second_tab->IsFullscreen());
  EXPECT_TRUE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Now exit fullscreen while still in the second tab.  The browser window
  // should no longer be fullscreened.
  ASSERT_TRUE(InvokeEvent(EXIT_TAB_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(first_tab->IsFullscreen());
  EXPECT_FALSE(second_tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Finally, exit fullscreen on the captured tab.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_TRUE(InvokeEvent(EXIT_TAB_FULLSCREEN));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_FALSE(first_tab->IsFullscreen());
  EXPECT_FALSE(second_tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
}

// Tests that, in a browser configured for Fullscreen-Within-Tab mode, more than
// one tab can be in fullscreen mode at the same time.  This is like the
// TwoFullscreenedTabsOneCaptured test above, except that the screen-captured
// tab exits fullscreen mode while the second tab is still in the foreground.
// When the first tab exits fullscreen, the fullscreen state of the second tab
// and the browser window should remain unchanged.
//
// See 'FullscreenWithinTab Note' in fullscreen_controller.h.
TEST_F(FullscreenControllerStateUnitTest,
       BackgroundCapturedTabExitsFullscreen) {
  AddTab(browser(), GURL(url::kAboutBlankURL));
  AddTab(browser(), GURL(url::kAboutBlankURL));
  content::WebContents* const first_tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  content::WebContents* const second_tab =
      browser()->tab_strip_model()->GetWebContentsAt(1);

  // Start capturing the first tab, fullscreen it, then switch to the second tab
  // and fullscreen that.  The second tab will cause the browser window to
  // expand to fill the screen.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  const gfx::Size kCaptureSize(1280, 720);
  auto capture_handle = first_tab->IncrementCapturerCount(
      kCaptureSize, /*stay_hidden=*/false,
      /*stay_awake=*/true, /*is_activity=*/true);
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(first_tab->IsFullscreen());
  EXPECT_FALSE(second_tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  browser()->tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  EXPECT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(first_tab->IsFullscreen());
  EXPECT_TRUE(second_tab->IsFullscreen());
  EXPECT_TRUE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Now, the first tab (backgrounded) exits fullscreen.  This should not affect
  // the second tab's fullscreen, nor the state of the browser window.
  GetFullscreenController()->ExitFullscreenModeForTab(first_tab);
  EXPECT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_FALSE(first_tab->IsFullscreen());
  EXPECT_TRUE(second_tab->IsFullscreen());
  EXPECT_TRUE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Finally, exit fullscreen on the second tab.
  ASSERT_TRUE(InvokeEvent(EXIT_TAB_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_FALSE(first_tab->IsFullscreen());
  EXPECT_FALSE(second_tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
}

// Tests that, in a browser configured for Fullscreen-Within-Tab mode,
// fullscreening a screen-captured tab will NOT cause any fullscreen state
// change to the browser window. Then, toggling Browser Fullscreen mode should
// fullscreen the browser window, but this should behave fully independently of
// the tab's fullscreen state.
//
// See 'FullscreenWithinTab Note' in fullscreen_controller.h.
TEST_F(FullscreenControllerStateUnitTest,
       OneCapturedTabFullscreenedBeforeBrowserFullscreen) {
  AddTab(browser(), GURL(url::kAboutBlankURL));
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  // Start capturing the tab and fullscreen it.  The state of the browser window
  // should remain unchanged.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  const gfx::Size kCaptureSize(1280, 720);
  auto capture_handle =
      tab->IncrementCapturerCount(kCaptureSize, /*stay_hidden=*/false,
                                  /*stay_awake=*/true, /*is_activity=*/true);
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  EXPECT_TRUE(tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  EXPECT_FALSE(GetFullscreenController()->IsFullscreenForBrowser());

  // Now, toggle into Browser Fullscreen mode.  The browser window should now be
  // fullscreened.
  ASSERT_TRUE(InvokeEvent(TOGGLE_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  EXPECT_TRUE(tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  EXPECT_TRUE(GetFullscreenController()->IsFullscreenForBrowser());

  // Now, toggle back out of Browser Fullscreen mode.  The browser window exits
  // fullscreen mode, but the tab stays in fullscreen mode.
  ASSERT_TRUE(InvokeEvent(TOGGLE_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  EXPECT_TRUE(tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  EXPECT_FALSE(GetFullscreenController()->IsFullscreenForBrowser());

  // Finally, toggle back into Browser Fullscreen mode and then toggle out of
  // tab fullscreen mode.  The browser window should stay fullscreened, while
  // the tab exits fullscreen mode.
  ASSERT_TRUE(InvokeEvent(TOGGLE_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(InvokeEvent(EXIT_TAB_FULLSCREEN));
  EXPECT_FALSE(tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  EXPECT_TRUE(GetFullscreenController()->IsFullscreenForBrowser());
}

// Tests that the tab doesn't enter Fullscreen-Within-Tab mode for hidden
// capture (stay_hidden == true).
TEST_F(FullscreenControllerStateUnitTest, HiddenlyCapturedTabFullscreened) {
  AddTab(browser(), GURL(url::kAboutBlankURL));
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  // Start capturing the tab with stay_hidden==true, and fullscreen it.
  // The the browser window should enter fullscreen.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  auto capture_handle =
      tab->IncrementCapturerCount(gfx::Size(), /*stay_hidden=*/true,
                                  /*stay_awake=*/true, /*is_activity=*/true);
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  EXPECT_TRUE(tab->IsFullscreen());
  EXPECT_TRUE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  EXPECT_FALSE(GetFullscreenController()->IsFullscreenForBrowser());
}

class FullscreenChangeObserver : public content::WebContentsObserver {
 public:
  explicit FullscreenChangeObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {
  }

  FullscreenChangeObserver(const FullscreenChangeObserver&) = delete;
  FullscreenChangeObserver& operator=(const FullscreenChangeObserver&) = delete;

  MOCK_METHOD(void, DidToggleFullscreenModeForTab, (bool, bool));
};

// Tests that going from tab fullscreen -> browser fullscreen causes an explicit
// WasResized to be called on ExitFullscreen while going from tab fullscreen ->
// Normal does not. This ensures that the Resize message we get in the renderer
// will have both the fullscreen change and size change in the same message.
// crbug.com/142427.
TEST_F(FullscreenControllerStateUnitTest, TabToBrowserFullscreenCausesResize) {
  AddTab(browser(), GURL(url::kAboutBlankURL));
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  FullscreenChangeObserver fullscreenObserver(tab);

  // Go into browser fullscreen, then tab fullscreen. Exiting tab fullscreen
  // should call WasResized since the fullscreen change won't cause a size
  // change itself.
  ASSERT_TRUE(InvokeEvent(TOGGLE_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  ASSERT_TRUE(browser()->window()->IsFullscreen());

  // The second parameter in DidToggleFullscreenModeForTab should be false,
  // indicating that the fullscreen change will *not* cause a resize.
  EXPECT_CALL(fullscreenObserver, DidToggleFullscreenModeForTab(false, false));
  ASSERT_TRUE(InvokeEvent(EXIT_TAB_FULLSCREEN));
  testing::Mock::VerifyAndClearExpectations(&fullscreenObserver);

  ASSERT_TRUE(InvokeEvent(TOGGLE_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_FALSE(browser()->window()->IsFullscreen());

  // Go into tab fullscreen only. Exiting tab fullscreen should *not* cause
  // a call to WasResized since the window will change size and we want the
  // fullscreen change and size change to be in one Resize message.
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(browser()->window()->IsFullscreen());

  // The second parameter in DidToggleFullscreenModeForTab should now be true,
  // indicating that the fullscreen change *will* cause a resize.
  EXPECT_CALL(fullscreenObserver, DidToggleFullscreenModeForTab(false, true));
  ASSERT_TRUE(InvokeEvent(EXIT_TAB_FULLSCREEN));
  ASSERT_FALSE(browser()->window()->IsFullscreen());
  testing::Mock::VerifyAndClearExpectations(&fullscreenObserver);
}

// Tests that the state of a fullscreened, screen-captured tab is preserved if
// the tab is detached from one Browser window and attached to another.
//
// See 'FullscreenWithinTab Note' in fullscreen_controller.h.
TEST_F(FullscreenControllerStateUnitTest,
       CapturedFullscreenedTabTransferredBetweenBrowserWindows) {
  AddTab(browser(), GURL(url::kAboutBlankURL));
  AddTab(browser(), GURL(url::kAboutBlankURL));
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  // Activate the first tab and tell its WebContents it is being captured.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  const gfx::Size kCaptureSize(1280, 720);
  auto capture_handle =
      tab->IncrementCapturerCount(kCaptureSize, /*stay_hidden=*/false,
                                  /*stay_awake=*/true, /*is_activity=*/true);
  ASSERT_FALSE(browser()->window()->IsFullscreen());
  ASSERT_FALSE(tab->IsFullscreen());
  ASSERT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Enter tab fullscreen.  Since the tab is being captured, the browser window
  // should not expand to fill the screen.
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Create the second browser window.
  const std::unique_ptr<BrowserWindow> second_browser_window(
      CreateBrowserWindow());
  const std::unique_ptr<Browser> second_browser(
      CreateBrowser(browser()->profile(), browser()->type(), false,
                    second_browser_window.get()));
  AddTab(second_browser.get(), GURL(url::kAboutBlankURL));
  FullscreenController* second_fullscreen_controller =
      second_browser->exclusive_access_manager()->fullscreen_controller();

  // Detach the tab from the first browser window and attach it to the second.
  // The tab should remain in fullscreen mode and neither browser window should
  // have expanded. It is correct for both FullscreenControllers to agree the
  // tab is in fullscreen mode.
  std::unique_ptr<tabs::TabModel> detached_tab =
      browser()->tab_strip_model()->DetachTabAtForInsertion(0);
  second_browser->tab_strip_model()->InsertDetachedTabAt(
      0, std::move(detached_tab), AddTabTypes::ADD_ACTIVE);
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_FALSE(second_browser->window()->IsFullscreen());
  EXPECT_TRUE(tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  EXPECT_EQ(GetFullscreenController()->GetFullscreenState(tab).target_mode,
            content::FullscreenMode::kPseudoContent);
  EXPECT_FALSE(
      second_fullscreen_controller->IsWindowFullscreenForTabOrPending());
  EXPECT_EQ(second_fullscreen_controller->GetFullscreenState(tab).target_mode,
            content::FullscreenMode::kPseudoContent);

  // Now, detach and reattach it back to the first browser window.  Again, the
  // tab should remain in fullscreen mode and neither browser window should have
  // expanded.
  detached_tab = second_browser->tab_strip_model()->DetachTabAtForInsertion(0);
  browser()->tab_strip_model()->InsertDetachedTabAt(0, std::move(detached_tab),
                                                    AddTabTypes::ADD_ACTIVE);
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_FALSE(second_browser->window()->IsFullscreen());
  EXPECT_TRUE(tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  EXPECT_EQ(GetFullscreenController()->GetFullscreenState(tab).target_mode,
            content::FullscreenMode::kPseudoContent);
  EXPECT_FALSE(
      second_fullscreen_controller->IsWindowFullscreenForTabOrPending());
  EXPECT_EQ(second_fullscreen_controller->GetFullscreenState(tab).target_mode,
            content::FullscreenMode::kPseudoContent);

  // Exit fullscreen.
  ASSERT_TRUE(InvokeEvent(EXIT_TAB_FULLSCREEN));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_FALSE(tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  EXPECT_EQ(GetFullscreenController()->GetFullscreenState(tab).target_mode,
            content::FullscreenMode::kWindowed);
  EXPECT_FALSE(
      second_fullscreen_controller->IsWindowFullscreenForTabOrPending());
  EXPECT_EQ(second_fullscreen_controller->GetFullscreenState(tab).target_mode,
            content::FullscreenMode::kWindowed);

  // Required tear-down specific to this test.
  second_browser->tab_strip_model()->CloseAllTabs();
}
