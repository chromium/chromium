// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_CONTROLLER_STATE_TESTS_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_CONTROLLER_STATE_TESTS_H_

#include "build/build_config.h"

// Macros used to create individual tests for all state and event pairs.
// To be included in the middle of a test .cc file just after a definition for
// TEST_EVENT in order to instantiate all the necessary actual tests.  See
// fullscreen_controller_state_interactive_browsertest.cc and
// fullscreen_controller_state_unittest.cc.

#define TEST_ALL_EVENTS(state)            \
  TEST_EVENT(state, TOGGLE_FULLSCREEN)    \
  TEST_EVENT(state, ENTER_TAB_FULLSCREEN) \
  TEST_EVENT(state, EXIT_TAB_FULLSCREEN)  \
  TEST_EVENT(state, BUBBLE_EXIT_LINK)     \
  TEST_EVENT(state, WINDOW_CHANGE)

TEST_ALL_EVENTS(STATE_NORMAL)
TEST_ALL_EVENTS(STATE_BROWSER_FULLSCREEN)
TEST_ALL_EVENTS(STATE_TAB_FULLSCREEN)
TEST_ALL_EVENTS(STATE_TAB_BROWSER_FULLSCREEN)
TEST_ALL_EVENTS(STATE_TO_NORMAL)
TEST_ALL_EVENTS(STATE_TO_BROWSER_FULLSCREEN)
TEST_ALL_EVENTS(STATE_TO_TAB_FULLSCREEN)

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_CONTROLLER_STATE_TESTS_H_
