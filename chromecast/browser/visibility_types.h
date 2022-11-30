// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_VISIBILITY_TYPES_H_
#define CHROMECAST_BROWSER_VISIBILITY_TYPES_H_

namespace chromecast {

// Describes visual context of the window within the UI.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chromecast.shell
enum class VisibilityType {
  // Unknown visibility state.
  UNKNOWN = 0,

  // Window is occupying the entire screen and can be interacted with.
  FULL_SCREEN = 1,

  // Window occupies a portion of the screen, supporting user interaction.
  PARTIAL_OUT = 2,

  // Window is hidden after dismissal by back gesture, and cannot be interacted
  // with via touch.
  HIDDEN = 3,

  // Window is being displayed as a small visible tile.
  TILE = 4,

  // Window is covered by other activities and cannot be interacted with via
  // touch.
  TRANSIENTLY_HIDDEN = 5
};

// Represents requested activity windowing behavior. Behavior includes:
// 1. How long the activity should show
// 2. Whether the window should become immediately visible
// 3. How much screen space the window should occupy
// 4. What state to return to when the activity is completed
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chromecast.shell
enum class VisibilityPriority {
  // Default priority. It is up to system to decide how to show the activity.
  DEFAULT = 0,

  // The activity wants to occupy the full screen for some period of time and
  // then become hidden after a timeout. When timeout, it returns to the
  // previous activity.
  TRANSIENT_TIMEOUTABLE = 1,

  // A high priority interruption occupies half of the screen if a sticky
  // activity is showing on the screen. Otherwise, it occupies the full screen.
  HIGH_PRIORITY_INTERRUPTION = 2,

  // The activity takes place of other activity and won't be timeout.
  STICKY_ACTIVITY = 3,

  // The activity stays on top of others (transient) but won't be timeout.
  // When the activity finishes, it returns to the previous one.
  TRANSIENT_STICKY = 4,

  // The activity should not be visible.
  HIDDEN = 5,

  // The activity should not be visible, but the activity will consider itself
  // to be visible. This is useful for opaque overlays while the activity is
  // still active.
  HIDDEN_STICKY = 6,
};

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chromecast.shell
enum class GestureType {
  NO_GESTURE = 0,
  GO_BACK = 1,
  TAP = 2,
  TAP_DOWN = 3,
  TOP_DRAG = 4,
  RIGHT_DRAG = 5,
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_VISIBILITY_TYPES_H_
