// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_TEST_INPUT_H_
#define CHROME_BROWSER_VR_UI_TEST_INPUT_H_

#include "base/time/time.h"
#include "ui/gfx/geometry/point_f.h"

namespace vr {

// These are used to map user-friendly names, e.g. URL_BAR, to the underlying
// element names for interaction during testing.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr
enum class UserFriendlyElementName : int {
  kNone = 0,         // A special "element" that causes the controller to point
                     // straight forward.
  kCurrentPosition,  // A special "element" that causes the controller to
                     // remain where it is.
  kUrl,              // URL bar
  kBackButton,       // Back button on the URL bar
  kForwardButton,    // Forward button in the overflow menu
  kReloadButton,     // Reload button in the overflow menu
  kOverflowMenu,     // Overflow menu
  kPageInfoButton,   // Page info button on the URL bar
  kBrowsingDialog,   // 2D fallback UI, e.g. permission prompts
  kContentQuad,      // Main content quad showing web contents
  kNewIncognitoTab,  // Button to open a new Incognito tab in the overflow menu
  kCloseIncognitoTabs,  // Button to close all Incognito tabs in the overflow
                        // menu
  kExitPrompt,          // DOFF prompt/request to exit VR
  kSuggestionBox,       // Box containing the omnibox suggestions
  kOmniboxTextField,    // The Omnibox's text input field that shows up when the
                        // URL bar is clicked.
  kOmniboxCloseButton,  // The button the exits the omnibox's text input mode.
  kOmniboxVoiceInputButton,  // The button next to the omnibox text field that
                             // enters voice input mode.
  kVoiceInputCloseButton,  // The button present in voice input mode that brings
                           // the user back to text entry mode.
  kAppButtonExitToast,     // The "Press app button to exit" toast when entring
                           // an immersive session.
  kWebXrAudioIndicator,  // Toast in WebXR indicating the microphone permission
                         // is in use.
  kWebXrHostedContent,   // Hosted content in a WebXR immersive session, e.g.
                         // permission prompts.
  kMicrophonePermissionIndicator,    // The microphone icon that appears when a
                                     // page is using the microphone permission.
  kWebXrExternalPromptNotification,  // The notification shown in the headset
                                     // if a permission is requested while in
                                     // immersive WebXR session.
  kCameraPermissionIndicator,    // The camera icon that appears when a page is
                                 // using the camera permission.
  kLocationPermissionIndicator,  // The location icon that appears when a page
                                 // is using the high accuracy location
                                 // permission.
  kWebXrLocationPermissionIndicator,  // The location icon that appears when a
                                      // page is using the location permission.
  kWebXrVideoPermissionIndicator,     // Toast in WebXR indicating the camera
                                      // permission is in use.
};

// These are the types of actions that Java can request callbacks for once
// they are complete.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr
enum class UiTestOperationType : int {
  kUiActivityResult = 0,     // Result after being told to wait for quiescence
  kFrameBufferDumped,        // Signal that the frame buffer was dumped to disk
  kElementVisibilityStatus,  // Signal that a watched element matches the
                             // desired visibility.
  kNumUiTestOperationTypes,  // Must be last
};

// These are used to report the result of a UI test operation.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr
enum class UiTestOperationResult : int {
  kUnreported,      // The result has not yet been reported
  kQuiescent,       // The UI reached quiescence (kUiActivityResult)
  kTimeoutNoStart,  // Timed out, UI activity never started (kUiActivityResult)
  kTimeoutNoEnd,    // Timed out, UI activity never finished (kUiActivityResult)
  kVisibilityMatch,  // The watched element's visibility matches the expectation
                     // (kElementVisibilityStatus)
  kTimeoutNoVisibilityMatch,  // Timed out, visibility doesn't match expectation
                              // (kElementVisibilityStatus)
};

// These are used to specify what type of action should be performed on a UI
// element using simulated controller input during testing.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr
enum class VrControllerTestAction : int {
  kHover,
  kEnableMockedInput,
  kRevertToRealInput,
  kClickDown,
  kClickUp,
  kMove,
  kAppDown,
  kAppUp,
  kTouchDown,
  kTouchUp,
};

// These are used to specify what type of keyboard input should be performed
// for a frame during testing.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr
enum class KeyboardTestAction : int {
  kInputText,
  kBackspace,
  kEnter,
  kEnableMockedKeyboard,
  kRevertToRealKeyboard,
};

// Holds all information necessary to perform a simulated controller action on
// a UI element.
struct ControllerTestInput {
  UserFriendlyElementName element_name;
  VrControllerTestAction action;
  gfx::PointF position;
};

// Holds all the information necessary to perform simulated keyboard input.
struct KeyboardTestInput {
  KeyboardTestAction action;
  std::string input_text;
};

struct UiTestActivityExpectation {
  int quiescence_timeout_ms;
};

struct VisibilityChangeExpectation {
  UserFriendlyElementName element_name;
  int timeout_ms;
  bool visibility;
};

// Holds all the information necessary to keep track of and report whether the
// UI reacted to test input.
struct UiTestState {
  // Whether the UI has started updating/reacting since we started tracking
  bool activity_started = false;
  // The number of frames to wait for the UI to stop having activity before
  // timing out.
  base::TimeDelta quiescence_timeout_ms = base::TimeDelta::Min();
  // The total number of frames that have been rendered since tracking started.
  base::TimeTicks start_time = base::TimeTicks::Now();
};

// Holds all the information necessary to keep track of and report whether a
// UI element changed visibility in the allotted time.
struct UiVisibilityState {
  // The UI element being watched.
  UserFriendlyElementName element_to_watch = UserFriendlyElementName::kUrl;
  // The desired visibility state of the element.
  bool expected_visibile = false;
  // How long to wait for a visibility change before timing out.
  base::TimeDelta timeout_ms = base::TimeDelta::Min();
  // The point in time that we started watching for visibility changes.
  base::TimeTicks start_time = base::TimeTicks::Now();
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_TEST_INPUT_H_
