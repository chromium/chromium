// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_CONTROLLER_STATE_TEST_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_CONTROLLER_STATE_TEST_H_

#include <memory>
#include <sstream>

#include "build/build_config.h"

class Browser;
class FullscreenController;

// Utility definition for mapping enum values to strings in switch statements.
#define ENUM_TO_STRING(enum) case enum: return #enum

// Test fixture used to test Fullscreen Controller through exhaustive sequences
// of events in unit and interactive tests.
//
// Because operating system window managers are too unreliable (they result in
// flakiness at around 1 out of 1000 runs) this fixture is designed to be run
// on testing infrastructure in unit tests mocking out the platforms' behavior.
// To verify that behavior interactive tests exist but are left disabled and
// only run manually when verifying the consistency of the
// FullscreenControllerTestWindow.
class FullscreenControllerStateTest {
 public:
  // Events names for FullscreenController methods.
  enum Event {
    TOGGLE_FULLSCREEN,     // ToggleBrowserFullscreenMode()
    ENTER_TAB_FULLSCREEN,  // EnterFullscreenModeForTab()
    EXIT_TAB_FULLSCREEN,   // ExitFullscreenModeForTab()
    BUBBLE_EXIT_LINK,      // ExitTabOrBrowserFullscreenToPreviousState()
    WINDOW_CHANGE,         // ChangeWindowFullscreenState()
    NUM_EVENTS,
    EVENT_INVALID,
  };

  // Conceptual states of the Fullscreen Controller, these do not correspond
  // to particular implemenation details.
  enum State {
    // The window is not in fullscreen.
    STATE_NORMAL,
    // User-initiated fullscreen.
    STATE_BROWSER_FULLSCREEN,
    // HTML5 tab-initiated fullscreen.
    STATE_TAB_FULLSCREEN,
    // Both tab and browser fullscreen.
    STATE_TAB_BROWSER_FULLSCREEN,
    // TO_ states are asynchronous states waiting for window state change
    // before transitioning to their named state.
    STATE_TO_NORMAL,
    STATE_TO_BROWSER_FULLSCREEN,
    STATE_TO_TAB_FULLSCREEN,
    NUM_STATES,
    STATE_INVALID,
  };

  static const int kMaxStateNameLength = 39;

  FullscreenControllerStateTest();

  FullscreenControllerStateTest(const FullscreenControllerStateTest&) = delete;
  FullscreenControllerStateTest& operator=(
      const FullscreenControllerStateTest&) = delete;

  virtual ~FullscreenControllerStateTest();

  static const char* GetStateString(State state);
  static const char* GetEventString(Event event);

  // Returns true if FullscreenController::WindowFullscreenStateChanged()
  // will be called and re-enter FullscreenController before
  // FullscreenController methods complete.
  static bool IsWindowFullscreenStateChangedReentrant();

  // Causes Fullscreen Controller to transition to an arbitrary state.
  void TransitionToState(State state);

  // Makes one state change to approach |destination_state| via shortest path.
  // Returns true if a state change is made.
  // Repeated calls are needed to reach the destination.
  bool TransitionAStepTowardState(State destination_state);

  // Calls FullscreenController::ChangeWindowFullscreenState if needed because
  // a mock BrowserWindow is being used.
  virtual void ChangeWindowFullscreenState() {}

  // Returns a description of the window's state, may return NULL.
  // FullscreenControllerStateTest owns the returned pointer.
  virtual const char* GetWindowStateString();

  // Causes the |event| to occur and return true on success.
  virtual bool InvokeEvent(Event event);

  // Checks that window state matches the expected controller state.
  virtual void VerifyWindowState();

  // Tests all states with all permutations of multiple events to detect
  // lingering state issues that would bleed over to other states.
  // I.E. for each state test all combinations of events E1, E2, E3.
  //
  // This produces coverage for event sequences that may happen normally but
  // would not be exposed by traversing to each state via TransitionToState().
  // TransitionToState() always takes the same path even when multiple paths
  // exist.
  void TestTransitionsForEachState();

  // Log transition_table_ to a string for debugging.
  std::string GetTransitionTableAsString() const;
  // Log state_transitions_ to a string for debugging.
  std::string GetStateTransitionsAsString() const;

 protected:
  // Set of enumerations (created with a helper macro) for _FALSE, _TRUE, and
  // _NO_EXPECTATION values to be passed to VerifyWindowStateExpectations().
  #define EXPECTATION_ENUM(enum_name, enum_prefix) \
      enum enum_name { \
        enum_prefix##_FALSE, \
        enum_prefix##_TRUE, \
        enum_prefix##_NO_EXPECTATION \
      }
  EXPECTATION_ENUM(FullscreenForBrowserExpectation, FULLSCREEN_FOR_BROWSER);
  EXPECTATION_ENUM(FullscreenForTabExpectation, FULLSCREEN_FOR_TAB);

  // Generated information about the transitions between states.
  struct StateTransitionInfo {
    StateTransitionInfo()
        : event(EVENT_INVALID),
          state(STATE_INVALID),
          distance(NUM_STATES) {}
    Event event;  // The |Event| that will cause the state transition.
    State state;  // The adjacent |State| transitioned to; not the final state.
    int distance;  // Steps to final state. NUM_STATES represents unknown.
  };

  // Returns next transition info for shortest path from source to destination.
  StateTransitionInfo NextTransitionInShortestPath(State source,
                                                   State destination,
                                                   int search_limit);

  // Returns a detailed log of what FullscreenControllerStateTest has done
  // up to this point, to be reported when tests fail.
  std::string GetAndClearDebugLog();

  // Returns true if the |state| & |event| pair should be skipped.
  virtual bool ShouldSkipStateAndEventPair(State state, Event event);

  // Returns true if a test should be skipped entirely, e.g. due to platform.
  virtual bool ShouldSkipTest(State state, Event event);

  // Runs one test of transitioning to a state and invoking an event.
  virtual void TestStateAndEvent(State state, Event event);

  // Checks that window state matches the expected controller state.
  virtual void VerifyWindowStateExpectations(
      FullscreenForBrowserExpectation fullscreen_for_browser,
      FullscreenForTabExpectation fullscreen_for_tab);

  void TearDown();

  virtual Browser* GetBrowser() = 0;
  FullscreenController* GetFullscreenController();

  // The state the FullscreenController is expected to be in.
  State state() const { return state_; }

 private:
  // The state the FullscreenController is expected to be in.
  State state_ = STATE_NORMAL;

  // Human defined |State| that results given each [state][event] pair.
  State transition_table_[NUM_STATES][NUM_EVENTS];

  // Generated information about the transitions between states [from][to].
  // View generated data with: out/Release/unit_tests
  //     --gtest_filter="FullscreenController*DebugLogStateTables"
  //     --gtest_also_run_disabled_tests
  StateTransitionInfo state_transitions_[NUM_STATES][NUM_STATES];

  // Log of operations reported on errors via GetAndClearDebugLog().
  std::ostringstream debugging_log_;
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_CONTROLLER_STATE_TEST_H_
