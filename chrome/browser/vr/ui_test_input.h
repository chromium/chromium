// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_TEST_INPUT_H_
#define CHROME_BROWSER_VR_UI_TEST_INPUT_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/vr/vr_base_export.h"
#include "ui/gfx/geometry/point_f.h"

namespace vr {

// These are used to map user-friendly names, e.g. URL_BAR, to the underlying
// element names for interaction during testing.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr
enum class UserFriendlyElementName : int {
  kWebXrAudioIndicator,  // Toast in WebXR indicating the microphone permission
                         // is in use.
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

// Holds all the information necessary to keep track of and report whether a
// UI element changed visibility in the allotted time.
struct VR_BASE_EXPORT UiVisibilityState {
  // The UI element being watched.
  UserFriendlyElementName element_to_watch =
      UserFriendlyElementName::kWebXrExternalPromptNotification;
  // The desired visibility state of the element.
  bool expected_visibile = false;
  // How long to wait for a visibility change before timing out.
  base::TimeDelta timeout_ms = base::TimeDelta::Min();
  // The point in time that we started watching for visibility changes.
  base::TimeTicks start_time = base::TimeTicks::Now();
  // Reports whether the visibility matched the expectation or timed out.
  base::OnceCallback<void(bool)> on_visibility_change_result;

  UiVisibilityState();
  ~UiVisibilityState();
  UiVisibilityState(UiVisibilityState&& other);
  UiVisibilityState& operator=(UiVisibilityState&& other);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_TEST_INPUT_H_
