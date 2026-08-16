// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_state_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/display/types/display_constants.h"

// The FullscreenControllerStateUnitTest unit test suite exhaustively tests
// the FullscreenController through all permutations of events. The behavior
// of the ExclusiveAccessContext is mocked via FullscreenControllerTestWindow.

namespace {

// FullscreenControllerTestWindow ----------------------------------------------

// An ExclusiveAccessContext used for testing FullscreenController. The behavior
// of this mock is verified manually by running
// FullscreenControllerStateInteractiveTest.
class FullscreenControllerTestWindow : public ExclusiveAccessContext,
                                       public content::WebContentsDelegate {
 public:
  // Simulate the window state with an enumeration.
  enum WindowState {
    kNormal,
    kFullscreen,
    kToNormal,
    kToFullscreen,
  };

  FullscreenControllerTestWindow() = default;
  ~FullscreenControllerTestWindow() override = default;

  // Window state:
  static const char* GetWindowStateString(WindowState state);
  WindowState state() const { return state_; }
  void set_exclusive_access_manager(ExclusiveAccessManager* manager) {
    exclusive_access_manager_ = manager;
  }
  void set_profile(Profile* profile) { profile_ = profile; }
  void set_active_web_contents(content::WebContents* web_contents) {
    active_web_contents_ = web_contents;
  }
  void set_reentrant(bool reentrant) { reentrant_ = reentrant; }

  // ExclusiveAccessContext Interface:
  Profile* GetProfile() override { return profile_; }
  bool IsFullscreen() const override;
  void EnterFullscreen(const url::Origin& origin,
                       ExclusiveAccessBubbleType type,
                       FullscreenTabParams fullscreen_tab_params) override;
  void ExitFullscreen() override;
  void UpdateExclusiveAccessBubble(
      const ExclusiveAccessBubbleParams& params,
      ExclusiveAccessBubbleHideCallback first_hide_callback) override {}
  bool IsExclusiveAccessBubbleDisplayed() const override { return false; }
  void OnExclusiveAccessUserInput() override {}
  content::WebContents* GetWebContentsForExclusiveAccess() override {
    return active_web_contents_;
  }
  bool CanUserEnterFullscreen() const override { return true; }
  bool CanUserExitFullscreen() const override { return true; }

  // content::WebContentsDelegate Interface:
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) override {
    if (!exclusive_access_manager_) {
      return false;
    }
    const content::FullscreenState fullscreen_state =
        exclusive_access_manager_->fullscreen_controller()->GetFullscreenState(
            web_contents);
    return fullscreen_state.target_mode == content::FullscreenMode::kContent ||
           fullscreen_state.target_mode ==
               content::FullscreenMode::kPseudoContent;
  }

  content::FullscreenState GetFullscreenState(
      const content::WebContents* web_contents) const override {
    if (!exclusive_access_manager_) {
      return {};
    }
    return exclusive_access_manager_->fullscreen_controller()
        ->GetFullscreenState(web_contents);
  }

  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override {
    if (exclusive_access_manager_) {
      exclusive_access_manager_->fullscreen_controller()
          ->EnterFullscreenModeForTab(requesting_frame,
                                      FullscreenTabParams{options.display_id});
    }
  }

  void ExitFullscreenModeForTab(content::WebContents* web_contents) override {
    if (exclusive_access_manager_) {
      exclusive_access_manager_->fullscreen_controller()
          ->ExitFullscreenModeForTab(web_contents);
    }
  }

  // Simulates the window changing state.
  void ChangeWindowFullscreenState();

 private:
  void EnterFullscreen();

  // Returns true if ChangeWindowFullscreenState() should be called as a result
  // of updating the current fullscreen state to the passed in state.
  bool IsTransitionReentrant(bool new_fullscreen);

  WindowState state_ = kNormal;
  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<content::WebContents> active_web_contents_ = nullptr;
  raw_ptr<ExclusiveAccessManager> exclusive_access_manager_ = nullptr;
  bool reentrant_ =
      FullscreenControllerStateTest::IsWindowFullscreenStateChangedReentrant();
};

void FullscreenControllerTestWindow::EnterFullscreen(
    const url::Origin& origin,
    ExclusiveAccessBubbleType type,
    FullscreenTabParams fullscreen_tab_params) {
  EnterFullscreen();
}

void FullscreenControllerTestWindow::ExitFullscreen() {
  if (IsFullscreen()) {
    state_ = kToNormal;

    if (IsTransitionReentrant(false)) {
      ChangeWindowFullscreenState();
    }
  }
}

bool FullscreenControllerTestWindow::IsFullscreen() const {
#if BUILDFLAG(IS_MAC)
  return state_ == kFullscreen || state_ == kToFullscreen;
#else
  return state_ == kFullscreen || state_ == kToNormal;
#endif
}

// static
const char* FullscreenControllerTestWindow::GetWindowStateString(
    WindowState state) {
  switch (state) {
    ENUM_TO_STRING(kNormal);
    ENUM_TO_STRING(kFullscreen);
    ENUM_TO_STRING(kToFullscreen);
    ENUM_TO_STRING(kToNormal);
    default:
      NOTREACHED() << "No string for state " << state;
  }
}

void FullscreenControllerTestWindow::ChangeWindowFullscreenState() {
  // Most states result in "no operation" intentionally. The tests
  // assume that all possible states and event pairs can be tested, even
  // though window managers will not generate all of these.
  if (state_ == kToFullscreen) {
    state_ = kFullscreen;
  } else if (state_ == kToNormal) {
    state_ = kNormal;
  }

  // Emit a change event from every state to ensure the Fullscreen Controller
  // handles it in all circumstances.
  if (exclusive_access_manager_) {
    exclusive_access_manager_->fullscreen_controller()
        ->WindowFullscreenStateChanged();
  }
}

void FullscreenControllerTestWindow::EnterFullscreen() {
  bool reentrant = IsTransitionReentrant(true);

  if (!IsFullscreen()) {
    state_ = kToFullscreen;
  }

  if (reentrant) {
    ChangeWindowFullscreenState();
  }
}

bool FullscreenControllerTestWindow::IsTransitionReentrant(
    bool new_fullscreen) {
  bool fullscreen_changed = (new_fullscreen != IsFullscreen());

  if (!fullscreen_changed) {
    return false;
  }

  if (reentrant_) {
    return true;
  }

  // BrowserWindowCocoa::EnterFullscreen() and
  // BrowserWindowCocoa::EnterFullscreenWithToolbar() are reentrant when
  // switching between fullscreen with chrome and fullscreen without chrome.
  return state_ == kFullscreen && !fullscreen_changed;
}

}  // namespace

// FullscreenControllerStateUnitTest -------------------------------------------

// Unit test fixture testing Fullscreen Controller through its states. Most of
// the test logic comes from FullscreenControllerStateTest.
class FullscreenControllerStateUnitTest
    : public ChromeRenderViewHostTestHarness,
      public FullscreenControllerStateTest {
 public:
  FullscreenControllerStateUnitTest() = default;

  FullscreenControllerStateUnitTest(const FullscreenControllerStateUnitTest&) =
      delete;
  FullscreenControllerStateUnitTest& operator=(
      const FullscreenControllerStateUnitTest&) = delete;

  // ChromeRenderViewHostTestHarness:
  void SetUp() override;
  void TearDown() override;

  // FullscreenControllerStateTest:
  void ChangeWindowFullscreenState() override;
  const char* GetWindowStateString() override;
  void VerifyWindowState() override;

  content::WebContents* AddTab(const GURL& url = GURL(url::kAboutBlankURL));
  void ActivateTab(size_t index);
  void SelectNextTab();
  std::unique_ptr<content::WebContents> DetachTab(size_t index);
  void AttachTab(std::unique_ptr<content::WebContents> contents,
                 size_t index,
                 bool active = true);
  content::WebContents* GetTab(size_t index) {
    CHECK_LT(index, tabs_.size());
    return tabs_[index].get();
  }

 protected:
  // FullscreenControllerStateTest:
  bool ShouldSkipStateAndEventPair(State state, Event event) override;
  FullscreenController* GetFullscreenController() override;
  content::WebContents* GetActiveWebContents() override;

  std::unique_ptr<FullscreenControllerTestWindow> window_;
  std::unique_ptr<ExclusiveAccessManager> exclusive_access_manager_;
  std::vector<std::unique_ptr<content::WebContents>> tabs_;
  size_t active_tab_index_ = 0;
};

void FullscreenControllerStateUnitTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  window_ = std::make_unique<FullscreenControllerTestWindow>();
  window_->set_profile(profile());
  exclusive_access_manager_ =
      std::make_unique<ExclusiveAccessManager>(window_.get());
  window_->set_exclusive_access_manager(exclusive_access_manager_.get());
}

void FullscreenControllerStateUnitTest::TearDown() {
  if (window_) {
    window_->set_exclusive_access_manager(nullptr);
    window_->set_active_web_contents(nullptr);
  }
  exclusive_access_manager_.reset();
  window_.reset();
  tabs_.clear();
  FullscreenControllerStateTest::TearDown();
  ChromeRenderViewHostTestHarness::TearDown();
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
      EXPECT_EQ(FullscreenControllerTestWindow::kNormal, window_->state())
          << GetAndClearDebugLog();
      break;

    case STATE_BROWSER_FULLSCREEN:
    case STATE_TAB_FULLSCREEN:
    case STATE_TAB_BROWSER_FULLSCREEN:
      EXPECT_EQ(FullscreenControllerTestWindow::kFullscreen, window_->state())
          << GetAndClearDebugLog();
      break;

    case STATE_TO_NORMAL:
      EXPECT_EQ(FullscreenControllerTestWindow::kToNormal, window_->state())
          << GetAndClearDebugLog();
      break;

    case STATE_TO_BROWSER_FULLSCREEN:
    case STATE_TO_TAB_FULLSCREEN:
      EXPECT_EQ(FullscreenControllerTestWindow::kToFullscreen, window_->state())
          << GetAndClearDebugLog();
      break;

    default:
      NOTREACHED() << GetAndClearDebugLog();
  }

  FullscreenControllerStateTest::VerifyWindowState();
}

bool FullscreenControllerStateUnitTest::ShouldSkipStateAndEventPair(
    State state,
    Event event) {
#if BUILDFLAG(IS_MAC)
  // TODO(scheib) Toggle, Window Event, Toggle, Toggle on Mac as exposed by
  // test *.STATE_TO_NORMAL__TOGGLE_FULLSCREEN runs interactively and exits to
  // Normal. This doesn't appear to be the desired result, and would add
  // too much complexity to mimic in our simple FullscreenControllerTestWindow.
  // http://crbug.com/40952626
  if ((state == STATE_TO_BROWSER_FULLSCREEN ||
       state == STATE_TO_TAB_FULLSCREEN) &&
      event == TOGGLE_FULLSCREEN) {
    return true;
  }
#endif

  return FullscreenControllerStateTest::ShouldSkipStateAndEventPair(state,
                                                                    event);
}

FullscreenController*
FullscreenControllerStateUnitTest::GetFullscreenController() {
  return exclusive_access_manager_->fullscreen_controller();
}

content::WebContents*
FullscreenControllerStateUnitTest::GetActiveWebContents() {
  return tabs_.empty() ? nullptr : tabs_[active_tab_index_].get();
}

content::WebContents* FullscreenControllerStateUnitTest::AddTab(
    const GURL& url) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  content::WebContents* raw_contents = contents.get();
  raw_contents->SetDelegate(window_.get());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(raw_contents, url);
  tabs_.push_back(std::move(contents));
  ActivateTab(tabs_.size() - 1);
  return raw_contents;
}

void FullscreenControllerStateUnitTest::ActivateTab(size_t index) {
  CHECK_LT(index, tabs_.size());
  content::WebContents* old_tab = GetActiveWebContents();
  active_tab_index_ = index;
  content::WebContents* new_tab = tabs_[index].get();
  window_->set_active_web_contents(new_tab);
  if (old_tab && old_tab != new_tab) {
    exclusive_access_manager_->OnTabDeactivated(old_tab);
    exclusive_access_manager_->OnTabDetachedFromView(old_tab);
  }
}

void FullscreenControllerStateUnitTest::SelectNextTab() {
  CHECK(!tabs_.empty());
  size_t next_index = (active_tab_index_ + 1) % tabs_.size();
  ActivateTab(next_index);
}

std::unique_ptr<content::WebContents>
FullscreenControllerStateUnitTest::DetachTab(size_t index) {
  CHECK_LT(index, tabs_.size());
  std::unique_ptr<content::WebContents> detached = std::move(tabs_[index]);
  tabs_.erase(tabs_.begin() + index);
  detached->SetDelegate(nullptr);
  if (index < active_tab_index_) {
    --active_tab_index_;
  }
  if (active_tab_index_ >= tabs_.size()) {
    active_tab_index_ = tabs_.empty() ? 0 : tabs_.size() - 1;
  }
  content::WebContents* new_active =
      tabs_.empty() ? nullptr : tabs_[active_tab_index_].get();
  window_->set_active_web_contents(new_active);
  exclusive_access_manager_->OnTabDeactivated(detached.get());
  exclusive_access_manager_->OnTabDetachedFromView(detached.get());
  return detached;
}

void FullscreenControllerStateUnitTest::AttachTab(
    std::unique_ptr<content::WebContents> contents,
    size_t index,
    bool active) {
  CHECK_LE(index, tabs_.size());
  contents->SetDelegate(window_.get());
  tabs_.insert(tabs_.begin() + index, std::move(contents));
  if (active) {
    ActivateTab(index);
  }
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
  AddTab(GURL(url::kAboutBlankURL));
  TestTransitionsForEachState();
  // Progress of test can be examined via LOG(INFO) << GetAndClearDebugLog();
}

// Individual tests for each pair of state and event ---------------------------

#define TEST_EVENT(state, event)                                \
  TEST_F(FullscreenControllerStateUnitTest, state##__##event) { \
    AddTab(GURL(url::kAboutBlankURL));                          \
    ASSERT_NO_FATAL_FAILURE(TestStateAndEvent(state, event))    \
        << GetAndClearDebugLog();                               \
  }
// Progress of tests can be examined by inserting the following line:
// LOG(INFO) << GetAndClearDebugLog(); }

#include "chrome/browser/ui/exclusive_access/fullscreen_controller_state_tests.h"

// Specific one-off tests for known issues -------------------------------------

// TODO(scheib) Toggling Tab fullscreen while pending Tab or
// Browser fullscreen is broken currently http://crbug.com/40951066
TEST_F(FullscreenControllerStateUnitTest,
       DISABLED_ToggleTabWhenPendingBrowser) {
  // Only possible without reentrancy.
  if (FullscreenControllerStateTest::
          IsWindowFullscreenStateChangedReentrant()) {
    return;
  }
  AddTab(GURL(url::kAboutBlankURL));
  ASSERT_NO_FATAL_FAILURE(TransitionToState(STATE_TO_BROWSER_FULLSCREEN))
      << GetAndClearDebugLog();

  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(EXIT_TAB_FULLSCREEN)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE)) << GetAndClearDebugLog();
}

// TODO(scheib) Toggling Tab fullscreen while pending Tab or
// Browser fullscreen is broken currently http://crbug.com/40951066
TEST_F(FullscreenControllerStateUnitTest, DISABLED_ToggleTabWhenPendingTab) {
  // Only possible without reentrancy.
  if (FullscreenControllerStateTest::
          IsWindowFullscreenStateChangedReentrant()) {
    return;
  }
  AddTab(GURL(url::kAboutBlankURL));
  ASSERT_NO_FATAL_FAILURE(TransitionToState(STATE_TO_TAB_FULLSCREEN))
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
          ShouldSkipStateAndEventPair(state2, EVENT_INVALID)) {
        continue;
      }
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
  AddTab(GURL(url::kAboutBlankURL));
  ASSERT_TRUE(InvokeEvent(TOGGLE_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(window_->IsFullscreen());
  // Exit fullscreen without going through fullscreen controller.
  window_->ExitFullscreen();
  ChangeWindowFullscreenState();
  EXPECT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE,
            exclusive_access_manager_->GetExclusiveAccessExitBubbleType());
}

// Tests that RunOrDeferUntilTransitionIsComplete runs the lambda when nothing
// is happening (no transition in progress).
TEST_F(FullscreenControllerStateUnitTest,
       RunOrDeferUntilTransitionIsCompleteNow) {
  AddTab(GURL(url::kAboutBlankURL));
  bool lambda_called = false;
  GetFullscreenController()->RunOrDeferUntilTransitionIsComplete(
      base::BindLambdaForTesting([&lambda_called]() { lambda_called = true; }));
  EXPECT_TRUE(lambda_called);
}

// Tests that RunOrDeferUntilTransitionIsComplete does not run the lambda while
// a transition is in progress and runs it after the transition completes.
TEST_F(FullscreenControllerStateUnitTest,
       RunOrDeferUntilTransitionIsCompleteDefer) {
  AddTab(GURL(url::kAboutBlankURL));
  window_->set_reentrant(false);
  GetFullscreenController()->ToggleBrowserFullscreenMode(
      /*user_initiated=*/false);
  bool lambda_called = false;
  GetFullscreenController()->RunOrDeferUntilTransitionIsComplete(
      base::BindLambdaForTesting([&lambda_called]() { lambda_called = true; }));
  EXPECT_FALSE(lambda_called);
  GetFullscreenController()->WindowFullscreenStateChanged();
  EXPECT_TRUE(lambda_called);
}

TEST_F(FullscreenControllerStateUnitTest, GetFullscreenState) {
  AddTab(GURL(url::kAboutBlankURL));
  content::WebContents* const tab = GetTab(0);
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
  AddTab(GURL(url::kAboutBlankURL));
  AddTab(GURL(url::kAboutBlankURL));
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(window_->IsFullscreen());

  SelectNextTab();
  ChangeWindowFullscreenState();
  EXPECT_FALSE(window_->IsFullscreen());
}

// Test that switching tabs via detaching the active tab (which is in tab
// fullscreen) takes the browser out of tab fullscreen. This case can
// occur if the user is in both tab fullscreen and immersive browser fullscreen.
TEST_F(FullscreenControllerStateUnitTest, ExitTabFullscreenViaDetachingTab) {
  AddTab(GURL(url::kAboutBlankURL));
  AddTab(GURL(url::kAboutBlankURL));
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(window_->IsFullscreen());

  DetachTab(active_tab_index_);
  ChangeWindowFullscreenState();
  EXPECT_FALSE(window_->IsFullscreen());
}

// Tests that, in a browser configured for Fullscreen-Within-Tab mode,
// fullscreening a screen-captured tab will NOT cause any fullscreen state
// change to the browser window. Furthermore, the test switches between tabs to
// confirm a captured tab will be resized by FullscreenController to the capture
// video resolution once the widget is detached from the UI.
//
// See 'FullscreenWithinTab Note' in fullscreen_controller.h.
TEST_F(FullscreenControllerStateUnitTest, OneCapturedFullscreenedTab) {
  AddTab(GURL(url::kAboutBlankURL));
  AddTab(GURL(url::kAboutBlankURL));
  content::WebContents* const first_tab = GetTab(0);
  content::WebContents* const second_tab = GetTab(1);

  // Activate the first tab and tell its WebContents it is being captured.
  ActivateTab(0);
  const gfx::Size kCaptureSize(1280, 720);
  auto capture_handle = first_tab->IncrementCapturerCount(
      kCaptureSize, /*stay_hidden=*/false,
      /*stay_awake=*/true, /*is_activity=*/true);
  ASSERT_FALSE(window_->IsFullscreen());
  ASSERT_FALSE(first_tab->IsFullscreen());
  ASSERT_FALSE(second_tab->IsFullscreen());
  ASSERT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Enter tab fullscreen.  Since the tab is being captured, the browser window
  // should not expand to fill the screen.
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  EXPECT_FALSE(window_->IsFullscreen());
  EXPECT_TRUE(first_tab->IsFullscreen());
  EXPECT_FALSE(second_tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Switch to the other tab.  Check that the first tab was resized to the
  // WebContents' preferred size.
  ActivateTab(1);
  EXPECT_FALSE(window_->IsFullscreen());
  EXPECT_TRUE(first_tab->IsFullscreen());
  EXPECT_FALSE(second_tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  EXPECT_EQ(kCaptureSize, first_tab->GetViewBounds().size());

  // Switch back to the first tab and exit fullscreen.
  ActivateTab(0);
  EXPECT_FALSE(window_->IsFullscreen());
  EXPECT_TRUE(first_tab->IsFullscreen());
  EXPECT_FALSE(second_tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  ASSERT_TRUE(InvokeEvent(EXIT_TAB_FULLSCREEN));
  EXPECT_FALSE(window_->IsFullscreen());
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
  AddTab(GURL(url::kAboutBlankURL));
  AddTab(GURL(url::kAboutBlankURL));
  content::WebContents* const first_tab = GetTab(0);
  content::WebContents* const second_tab = GetTab(1);

  // Start capturing the first tab, fullscreen it, then switch to the second tab
  // and fullscreen that.  The second tab will cause the browser window to
  // expand to fill the screen.
  ActivateTab(0);
  const gfx::Size kCaptureSize(1280, 720);
  auto capture_handle = first_tab->IncrementCapturerCount(
      kCaptureSize, /*stay_hidden=*/false,
      /*stay_awake=*/true, /*is_activity=*/true);
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  EXPECT_FALSE(window_->IsFullscreen());
  EXPECT_TRUE(first_tab->IsFullscreen());
  EXPECT_FALSE(second_tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  ActivateTab(1);
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  EXPECT_TRUE(window_->IsFullscreen());
  EXPECT_TRUE(first_tab->IsFullscreen());
  EXPECT_TRUE(second_tab->IsFullscreen());
  EXPECT_TRUE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Now exit fullscreen while still in the second tab.  The browser window
  // should no longer be fullscreened.
  ASSERT_TRUE(InvokeEvent(EXIT_TAB_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  EXPECT_FALSE(window_->IsFullscreen());
  EXPECT_TRUE(first_tab->IsFullscreen());
  EXPECT_FALSE(second_tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Finally, exit fullscreen on the captured tab.
  ActivateTab(0);
  ASSERT_TRUE(InvokeEvent(EXIT_TAB_FULLSCREEN));
  EXPECT_FALSE(window_->IsFullscreen());
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
  AddTab(GURL(url::kAboutBlankURL));
  AddTab(GURL(url::kAboutBlankURL));
  content::WebContents* const first_tab = GetTab(0);
  content::WebContents* const second_tab = GetTab(1);

  // Start capturing the first tab, fullscreen it, then switch to the second tab
  // and fullscreen that.  The second tab will cause the browser window to
  // expand to fill the screen.
  ActivateTab(0);
  const gfx::Size kCaptureSize(1280, 720);
  auto capture_handle = first_tab->IncrementCapturerCount(
      kCaptureSize, /*stay_hidden=*/false,
      /*stay_awake=*/true, /*is_activity=*/true);
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  EXPECT_FALSE(window_->IsFullscreen());
  EXPECT_TRUE(first_tab->IsFullscreen());
  EXPECT_FALSE(second_tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  ActivateTab(1);
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  EXPECT_TRUE(window_->IsFullscreen());
  EXPECT_TRUE(first_tab->IsFullscreen());
  EXPECT_TRUE(second_tab->IsFullscreen());
  EXPECT_TRUE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Now, the first tab (backgrounded) exits fullscreen.  This should not affect
  // the second tab's fullscreen, nor the state of the browser window.
  GetFullscreenController()->ExitFullscreenModeForTab(first_tab);
  EXPECT_TRUE(window_->IsFullscreen());
  EXPECT_FALSE(first_tab->IsFullscreen());
  EXPECT_TRUE(second_tab->IsFullscreen());
  EXPECT_TRUE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Finally, exit fullscreen on the second tab.
  ASSERT_TRUE(InvokeEvent(EXIT_TAB_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  EXPECT_FALSE(window_->IsFullscreen());
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
  AddTab(GURL(url::kAboutBlankURL));
  content::WebContents* const tab = GetTab(0);

  // Start capturing the tab and fullscreen it.  The state of the browser window
  // should remain unchanged.
  ActivateTab(0);
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
  AddTab(GURL(url::kAboutBlankURL));
  content::WebContents* const tab = GetTab(0);

  // Start capturing the tab with stay_hidden==true, and fullscreen it.
  // The the browser window should enter fullscreen.
  ActivateTab(0);
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
      : WebContentsObserver(web_contents) {}

  FullscreenChangeObserver(const FullscreenChangeObserver&) = delete;
  FullscreenChangeObserver& operator=(const FullscreenChangeObserver&) = delete;

  MOCK_METHOD(void, DidToggleFullscreenModeForTab, (bool, bool));
};

// Tests that going from tab fullscreen -> browser fullscreen causes an explicit
// WasResized to be called on ExitFullscreen while going from tab fullscreen ->
// Normal does not. This ensures that the Resize message we get in the renderer
// will have both the fullscreen change and size change in the same message.
// crbug.com/40260339.
TEST_F(FullscreenControllerStateUnitTest, TabToBrowserFullscreenCausesResize) {
  AddTab(GURL(url::kAboutBlankURL));
  content::WebContents* const tab = GetTab(0);

  FullscreenChangeObserver fullscreenObserver(tab);

  // Go into browser fullscreen, then tab fullscreen. Exiting tab fullscreen
  // should call WasResized since the fullscreen change won't cause a size
  // change itself.
  ASSERT_TRUE(InvokeEvent(TOGGLE_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  ASSERT_TRUE(window_->IsFullscreen());

  // The second parameter in DidToggleFullscreenModeForTab should be false,
  // indicating that the fullscreen change will *not* cause a resize.
  EXPECT_CALL(fullscreenObserver, DidToggleFullscreenModeForTab(false, false));
  ASSERT_TRUE(InvokeEvent(EXIT_TAB_FULLSCREEN));
  testing::Mock::VerifyAndClearExpectations(&fullscreenObserver);

  ASSERT_TRUE(InvokeEvent(TOGGLE_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_FALSE(window_->IsFullscreen());

  // Go into tab fullscreen only. Exiting tab fullscreen should *not* cause
  // a call to WasResized since the window will change size and we want the
  // fullscreen change and size change to be in one Resize message.
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(window_->IsFullscreen());

  // The second parameter in DidToggleFullscreenModeForTab should now be true,
  // indicating that the fullscreen change *will* cause a resize.
  EXPECT_CALL(fullscreenObserver, DidToggleFullscreenModeForTab(false, true));
  ASSERT_TRUE(InvokeEvent(EXIT_TAB_FULLSCREEN));
  ASSERT_FALSE(window_->IsFullscreen());
  testing::Mock::VerifyAndClearExpectations(&fullscreenObserver);
}

// Tests that the state of a fullscreened, screen-captured tab is preserved if
// the tab is detached from one Browser window and attached to another.
//
// See 'FullscreenWithinTab Note' in fullscreen_controller.h.
TEST_F(FullscreenControllerStateUnitTest,
       CapturedFullscreenedTabTransferredBetweenBrowserWindows) {
  AddTab(GURL(url::kAboutBlankURL));
  AddTab(GURL(url::kAboutBlankURL));
  content::WebContents* const tab = GetTab(0);

  // Activate the first tab and tell its WebContents it is being captured.
  ActivateTab(0);
  const gfx::Size kCaptureSize(1280, 720);
  auto capture_handle =
      tab->IncrementCapturerCount(kCaptureSize, /*stay_hidden=*/false,
                                  /*stay_awake=*/true, /*is_activity=*/true);
  ASSERT_FALSE(window_->IsFullscreen());
  ASSERT_FALSE(tab->IsFullscreen());
  ASSERT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Enter tab fullscreen.  Since the tab is being captured, the browser window
  // should not expand to fill the screen.
  ASSERT_TRUE(InvokeEvent(ENTER_TAB_FULLSCREEN));
  EXPECT_FALSE(window_->IsFullscreen());
  EXPECT_TRUE(tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());

  // Create the second test window and exclusive access manager.
  FullscreenControllerTestWindow second_window;
  second_window.set_profile(profile());
  ExclusiveAccessManager second_exclusive_access_manager(&second_window);
  second_window.set_exclusive_access_manager(&second_exclusive_access_manager);
  FullscreenController* second_fullscreen_controller =
      second_exclusive_access_manager.fullscreen_controller();

  // Detach the tab from the first window and attach it to the second.
  // The tab should remain in fullscreen mode and neither browser window should
  // have expanded. It is correct for both FullscreenControllers to agree the
  // tab is in fullscreen mode.
  std::unique_ptr<content::WebContents> detached_tab = DetachTab(0);
  detached_tab->SetDelegate(&second_window);
  second_window.set_active_web_contents(detached_tab.get());
  EXPECT_FALSE(window_->IsFullscreen());
  EXPECT_FALSE(second_window.IsFullscreen());
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
  detached_tab->SetDelegate(nullptr);
  second_window.set_active_web_contents(nullptr);
  AttachTab(std::move(detached_tab), 0, /*active=*/true);
  EXPECT_FALSE(window_->IsFullscreen());
  EXPECT_FALSE(second_window.IsFullscreen());
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
  EXPECT_FALSE(window_->IsFullscreen());
  EXPECT_FALSE(tab->IsFullscreen());
  EXPECT_FALSE(GetFullscreenController()->IsWindowFullscreenForTabOrPending());
  EXPECT_EQ(GetFullscreenController()->GetFullscreenState(tab).target_mode,
            content::FullscreenMode::kWindowed);
  EXPECT_FALSE(
      second_fullscreen_controller->IsWindowFullscreenForTabOrPending());
  EXPECT_EQ(second_fullscreen_controller->GetFullscreenState(tab).target_mode,
            content::FullscreenMode::kWindowed);
  second_window.set_exclusive_access_manager(nullptr);
}
