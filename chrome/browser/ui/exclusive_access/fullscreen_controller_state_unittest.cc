// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_state_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// The FullscreenControllerStateUnitTest unit test suite exhastively tests
// the FullscreenController through all permutations of events. The behavior
// of the BrowserWindow is mocked via FullscreenControllerTestWindow.

namespace {

const char kFullscreenReshowHistogramName[] =
    "ExclusiveAccess.BubbleReshowsPerSession.Fullscreen";

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
  content::WebContents* GetActiveWebContents() override;
  void HideDownloadShelf() override;
  void UnhideDownloadShelf() override;
  void EnterFullscreen(const GURL& url,
                       ExclusiveAccessBubbleType type) override;
  void ExitFullscreen() override;
  void UpdateExclusiveAccessExitBubbleContent(
      const GURL& url,
      ExclusiveAccessBubbleType bubble_type,
      ExclusiveAccessBubbleHideCallback bubble_first_hide_callback,
      bool force_update) override;
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
  Browser* browser_;
};

FullscreenControllerTestWindow::FullscreenControllerTestWindow()
    : state_(NORMAL),
      browser_(NULL) {
}

void FullscreenControllerTestWindow::EnterFullscreen(
    const GURL& url,
    ExclusiveAccessBubbleType type) {
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
#if defined(OS_MACOSX)
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
      NOTREACHED() << "No string for state " << state;
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

content::WebContents* FullscreenControllerTestWindow::GetActiveWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void FullscreenControllerTestWindow::UnhideDownloadShelf() {
  GetDownloadShelf()->Unhide();
}

void FullscreenControllerTestWindow::HideDownloadShelf() {
  GetDownloadShelf()->Hide();
}

void FullscreenControllerTestWindow::UpdateExclusiveAccessExitBubbleContent(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type,
    ExclusiveAccessBubbleHideCallback bubble_first_hide_callback,
    bool force_update) {}

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
  FullscreenControllerTestWindow* window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FullscreenControllerStateUnitTest);
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
      NOTREACHED() << GetAndClearDebugLog();
  }

  FullscreenControllerStateTest::VerifyWindowState();
}

bool FullscreenControllerStateUnitTest::ShouldSkipStateAndEventPair(
    State state, Event event) {
#if defined(OS_MACOSX)
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

  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_FALSE)) << GetAndClearDebugLog();
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

  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_FALSE)) << GetAndClearDebugLog();
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

// Test that switching tabs takes the browser out of tab fullscreen.
TEST_F(FullscreenControllerStateUnitTest, ExitTabFullscreenViaSwitchingTab) {
  base::HistogramTester histogram_tester;

  AddTab(browser(), GURL(url::kAboutBlankURL));
  AddTab(browser(), GURL(url::kAboutBlankURL));
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(browser()->window()->IsFullscreen());
  histogram_tester.ExpectTotalCount(kFullscreenReshowHistogramName, 0);

  browser()->tab_strip_model()->SelectNextTab();
  ChangeWindowFullscreenState();
  EXPECT_FALSE(browser()->window()->IsFullscreen());

  // Do a simple test that histograms are being recorded upon exiting the
  // fullscreen session (when simplified-fullscreen-ui is enabled).
  histogram_tester.ExpectUniqueSample(kFullscreenReshowHistogramName, 0, 1);
}

// Test that switching tabs via detaching the active tab (which is in tab
// fullscreen) takes the browser out of tab fullscreen. This case can
// occur if the user is in both tab fullscreen and immersive browser fullscreen.
TEST_F(FullscreenControllerStateUnitTest, ExitTabFullscreenViaDetachingTab) {
  AddTab(browser(), GURL(url::kAboutBlankURL));
  AddTab(browser(), GURL(url::kAboutBlankURL));
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(browser()->window()->IsFullscreen());

  std::unique_ptr<content::WebContents> web_contents =
      browser()->tab_strip_model()->DetachWebContentsAt(0);
  ChangeWindowFullscreenState();
  EXPECT_FALSE(browser()->window()->IsFullscreen());
}

// Test that replacing the web contents for a tab which is in tab fullscreen
// takes the browser out of tab fullscreen. This can occur if the user
// navigates to a prerendered page from a page which is tab fullscreen.
TEST_F(FullscreenControllerStateUnitTest, ExitTabFullscreenViaReplacingTab) {
  AddTab(browser(), GURL(url::kAboutBlankURL));
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(browser()->window()->IsFullscreen());

  std::unique_ptr<content::WebContents> new_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile()));
  std::unique_ptr<content::WebContents> old_web_contents =
      browser()->tab_strip_model()->ReplaceWebContentsAt(
          0, std::move(new_web_contents));
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
  content::WebContentsDelegate* const wc_delegate =
      static_cast<content::WebContentsDelegate*>(browser());
  ASSERT_TRUE(wc_delegate->EmbedsFullscreenWidget());

  AddTab(browser(), GURL(url::kAboutBlankURL));
  AddTab(browser(), GURL(url::kAboutBlankURL));
  content::WebContents* const first_tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  content::WebContents* const second_tab =
      browser()->tab_strip_model()->GetWebContentsAt(1);

  // Activate the first tab and tell its WebContents it is being captured.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  const gfx::Size kCaptureSize(1280, 720);
  first_tab->IncrementCapturerCount(kCaptureSize, /* stay_hidden */ false);
  ASSERT_FALSE(browser()->window()->IsFullscreen());
  ASSERT_FALSE(wc_delegate->IsFullscreenForTabOrPending(first_tab));
  ASSERT_FALSE(wc_delegate->IsFullscreenForTabOrPending(second_tab));
  ASSERT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Enter tab fullscreen.  Since the tab is being captured, the browser window
  // should not expand to fill the screen.
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(wc_delegate->IsFullscreenForTabOrPending(first_tab));
  EXPECT_FALSE(wc_delegate->IsFullscreenForTabOrPending(second_tab));
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Switch to the other tab.  Check that the first tab was resized to the
  // WebContents' preferred size.
  browser()->tab_strip_model()->ActivateTabAt(
      1, {TabStripModel::GestureType::kOther});
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(wc_delegate->IsFullscreenForTabOrPending(first_tab));
  EXPECT_FALSE(wc_delegate->IsFullscreenForTabOrPending(second_tab));
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  // TODO(miu): Need to make an adjustment to content::WebContentsViewMac for
  // the following to work:
#if !defined(OS_MACOSX)
  EXPECT_EQ(kCaptureSize, first_tab->GetViewBounds().size());
#endif

  // Switch back to the first tab and exit fullscreen.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(wc_delegate->IsFullscreenForTabOrPending(first_tab));
  EXPECT_FALSE(wc_delegate->IsFullscreenForTabOrPending(second_tab));
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_FALSE));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_FALSE(wc_delegate->IsFullscreenForTabOrPending(first_tab));
  EXPECT_FALSE(wc_delegate->IsFullscreenForTabOrPending(second_tab));
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
  content::WebContentsDelegate* const wc_delegate =
      static_cast<content::WebContentsDelegate*>(browser());
  ASSERT_TRUE(wc_delegate->EmbedsFullscreenWidget());

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
      0, {TabStripModel::GestureType::kOther});
  const gfx::Size kCaptureSize(1280, 720);
  first_tab->IncrementCapturerCount(kCaptureSize, /* stay_hidden */ false);
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(wc_delegate->IsFullscreenForTabOrPending(first_tab));
  EXPECT_FALSE(wc_delegate->IsFullscreenForTabOrPending(second_tab));
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  browser()->tab_strip_model()->ActivateTabAt(
      1, {TabStripModel::GestureType::kOther});
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  EXPECT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(wc_delegate->IsFullscreenForTabOrPending(first_tab));
  EXPECT_TRUE(wc_delegate->IsFullscreenForTabOrPending(second_tab));
  EXPECT_TRUE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Now exit fullscreen while still in the second tab.  The browser window
  // should no longer be fullscreened.
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_FALSE));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(wc_delegate->IsFullscreenForTabOrPending(first_tab));
  EXPECT_FALSE(wc_delegate->IsFullscreenForTabOrPending(second_tab));
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Finally, exit fullscreen on the captured tab.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_FALSE));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_FALSE(wc_delegate->IsFullscreenForTabOrPending(first_tab));
  EXPECT_FALSE(wc_delegate->IsFullscreenForTabOrPending(second_tab));
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
  content::WebContentsDelegate* const wc_delegate =
      static_cast<content::WebContentsDelegate*>(browser());
  ASSERT_TRUE(wc_delegate->EmbedsFullscreenWidget());

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
      0, {TabStripModel::GestureType::kOther});
  const gfx::Size kCaptureSize(1280, 720);
  first_tab->IncrementCapturerCount(kCaptureSize, /* stay_hidden */ false);
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(wc_delegate->IsFullscreenForTabOrPending(first_tab));
  EXPECT_FALSE(wc_delegate->IsFullscreenForTabOrPending(second_tab));
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  browser()->tab_strip_model()->ActivateTabAt(
      1, {TabStripModel::GestureType::kOther});
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  EXPECT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(wc_delegate->IsFullscreenForTabOrPending(first_tab));
  EXPECT_TRUE(wc_delegate->IsFullscreenForTabOrPending(second_tab));
  EXPECT_TRUE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Now, the first tab (backgrounded) exits fullscreen.  This should not affect
  // the second tab's fullscreen, nor the state of the browser window.
  GetFullscreenController()->ExitFullscreenModeForTab(first_tab);
  EXPECT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_FALSE(wc_delegate->IsFullscreenForTabOrPending(first_tab));
  EXPECT_TRUE(wc_delegate->IsFullscreenForTabOrPending(second_tab));
  EXPECT_TRUE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Finally, exit fullscreen on the second tab.
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_FALSE));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_FALSE(wc_delegate->IsFullscreenForTabOrPending(first_tab));
  EXPECT_FALSE(wc_delegate->IsFullscreenForTabOrPending(second_tab));
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
  content::WebContentsDelegate* const wc_delegate =
      static_cast<content::WebContentsDelegate*>(browser());
  ASSERT_TRUE(wc_delegate->EmbedsFullscreenWidget());

  AddTab(browser(), GURL(url::kAboutBlankURL));
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  // Start capturing the tab and fullscreen it.  The state of the browser window
  // should remain unchanged.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  const gfx::Size kCaptureSize(1280, 720);
  tab->IncrementCapturerCount(kCaptureSize, /* stay_hidden */ false);
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE));
  EXPECT_TRUE(wc_delegate->IsFullscreenForTabOrPending(tab));
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  EXPECT_FALSE(GetFullscreenController()->IsFullscreenForBrowser());

  // Now, toggle into Browser Fullscreen mode.  The browser window should now be
  // fullscreened.
  ASSERT_TRUE(InvokeEvent(TOGGLE_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  EXPECT_TRUE(wc_delegate->IsFullscreenForTabOrPending(tab));
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  EXPECT_TRUE(GetFullscreenController()->IsFullscreenForBrowser());

  // Now, toggle back out of Browser Fullscreen mode.  The browser window exits
  // fullscreen mode, but the tab stays in fullscreen mode.
  ASSERT_TRUE(InvokeEvent(TOGGLE_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  EXPECT_TRUE(wc_delegate->IsFullscreenForTabOrPending(tab));
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  EXPECT_FALSE(GetFullscreenController()->IsFullscreenForBrowser());

  // Finally, toggle back into Browser Fullscreen mode and then toggle out of
  // tab fullscreen mode.  The browser window should stay fullscreened, while
  // the tab exits fullscreen mode.
  ASSERT_TRUE(InvokeEvent(TOGGLE_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_FALSE));
  EXPECT_FALSE(wc_delegate->IsFullscreenForTabOrPending(tab));
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  EXPECT_TRUE(GetFullscreenController()->IsFullscreenForBrowser());
}

class FullscreenChangeObserver : public content::WebContentsObserver {
 public:
  explicit FullscreenChangeObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {
  }

  MOCK_METHOD2(DidToggleFullscreenModeForTab, void(bool, bool));

 private:
  DISALLOW_COPY_AND_ASSIGN(FullscreenChangeObserver);
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
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE));
  ASSERT_TRUE(browser()->window()->IsFullscreen());

  // The second parameter in DidToggleFullscreenModeForTab should be false,
  // indicating that the fullscreen change will *not* cause a resize.
  EXPECT_CALL(fullscreenObserver,
              DidToggleFullscreenModeForTab(false, false));
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_FALSE));
  testing::Mock::VerifyAndClearExpectations(&fullscreenObserver);

  ASSERT_TRUE(InvokeEvent(TOGGLE_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_FALSE(browser()->window()->IsFullscreen());

  // Go into tab fullscreen only. Exiting tab fullscreen should *not* cause
  // a call to WasResized since the window will change size and we want the
  // fullscreen change and size change to be in one Resize message.
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(browser()->window()->IsFullscreen());

  // The second parameter in DidToggleFullscreenModeForTab should now be true,
  // indicating that the fullscreen change *will* cause a resize.
  EXPECT_CALL(fullscreenObserver,
              DidToggleFullscreenModeForTab(false, true));
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_FALSE));
  ASSERT_FALSE(browser()->window()->IsFullscreen());
  testing::Mock::VerifyAndClearExpectations(&fullscreenObserver);
}

// Tests that the state of a fullscreened, screen-captured tab is preserved if
// the tab is detached from one Browser window and attached to another.
//
// See 'FullscreenWithinTab Note' in fullscreen_controller.h.
TEST_F(FullscreenControllerStateUnitTest,
       CapturedFullscreenedTabTransferredBetweenBrowserWindows) {
  content::WebContentsDelegate* const wc_delegate =
      static_cast<content::WebContentsDelegate*>(browser());
  ASSERT_TRUE(wc_delegate->EmbedsFullscreenWidget());

  AddTab(browser(), GURL(url::kAboutBlankURL));
  AddTab(browser(), GURL(url::kAboutBlankURL));
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  // Activate the first tab and tell its WebContents it is being captured.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  const gfx::Size kCaptureSize(1280, 720);
  tab->IncrementCapturerCount(kCaptureSize, /* stay_hidden */ false);
  ASSERT_FALSE(browser()->window()->IsFullscreen());
  ASSERT_FALSE(wc_delegate->IsFullscreenForTabOrPending(tab));
  ASSERT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Enter tab fullscreen.  Since the tab is being captured, the browser window
  // should not expand to fill the screen.
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(wc_delegate->IsFullscreenForTabOrPending(tab));
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Create the second browser window.
  const std::unique_ptr<BrowserWindow> second_browser_window(
      CreateBrowserWindow());
  const std::unique_ptr<Browser> second_browser(
      CreateBrowser(browser()->profile(), browser()->type(), false,
                    second_browser_window.get()));
  AddTab(second_browser.get(), GURL(url::kAboutBlankURL));
  content::WebContentsDelegate* const second_wc_delegate =
      static_cast<content::WebContentsDelegate*>(second_browser.get());

  // Detach the tab from the first browser window and attach it to the second.
  // The tab should remain in fullscreen mode and neither browser window should
  // have expanded. It is correct for both FullscreenControllers to agree the
  // tab is in fullscreen mode.
  std::unique_ptr<content::WebContents> owned_wc =
      browser()->tab_strip_model()->DetachWebContentsAt(0);
  second_browser->tab_strip_model()->InsertWebContentsAt(
      0, std::move(owned_wc), TabStripModel::ADD_ACTIVE);
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_FALSE(second_browser->window()->IsFullscreen());
  EXPECT_TRUE(wc_delegate->IsFullscreenForTabOrPending(tab));
  EXPECT_TRUE(second_wc_delegate->IsFullscreenForTabOrPending(tab));
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  EXPECT_FALSE(second_browser->exclusive_access_manager()
                   ->fullscreen_controller()
                   ->IsWindowFullscreenForTabOrPending());

  // Now, detach and reattach it back to the first browser window.  Again, the
  // tab should remain in fullscreen mode and neither browser window should have
  // expanded.
  owned_wc = second_browser->tab_strip_model()->DetachWebContentsAt(0);
  browser()->tab_strip_model()->InsertWebContentsAt(0, std::move(owned_wc),
                                                    TabStripModel::ADD_ACTIVE);
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_FALSE(second_browser->window()->IsFullscreen());
  EXPECT_TRUE(wc_delegate->IsFullscreenForTabOrPending(tab));
  EXPECT_TRUE(second_wc_delegate->IsFullscreenForTabOrPending(tab));
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  EXPECT_FALSE(second_browser->exclusive_access_manager()
                   ->fullscreen_controller()
                   ->IsWindowFullscreenForTabOrPending());

  // Exit fullscreen.
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_FALSE));
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_FALSE(wc_delegate->IsFullscreenForTabOrPending(tab));
  EXPECT_FALSE(second_wc_delegate->IsFullscreenForTabOrPending(tab));
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  EXPECT_FALSE(second_browser->exclusive_access_manager()
                   ->fullscreen_controller()
                   ->IsWindowFullscreenForTabOrPending());

  // Required tear-down specific to this test.
  second_browser->tab_strip_model()->CloseAllTabs();
}
