// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_ENUMS_H_
#define CHROME_BROWSER_UI_TABS_TAB_ENUMS_H_

// Alert states for a tab. Any number of these (or none) may apply at once.
enum class TabAlertState {
  MEDIA_RECORDING,        // Audio/Video being recorded, consumed by tab.
  TAB_CAPTURING,          // Tab contents being captured.
  AUDIO_PLAYING,          // Audible audio is playing from the tab.
  AUDIO_MUTING,           // Tab audio is being muted.
  BLUETOOTH_CONNECTED,    // Tab is connected to a BT Device.
  BLUETOOTH_SCAN_ACTIVE,  // Tab is actively scanning for BT devices.
  USB_CONNECTED,          // Tab is connected to a USB device.
  HID_CONNECTED,          // Tab is connected to a HID device.
  SERIAL_CONNECTED,       // Tab is connected to a serial device.
  PIP_PLAYING,            // Tab contains a video in Picture-in-Picture mode.
  DESKTOP_CAPTURING,      // Desktop contents being recorded, consumed by tab.
  VR_PRESENTING_IN_HEADSET,  // VR content is being presented in a headset.
};

// State indicating if the user is following the web feed of the site loaded in
// a tab.
enum class TabWebFeedFollowState {
  kUnknown,      // The initial state before the follow state is determined.
  kFollowed,     // The web feed is followed.
  kNotFollowed,  // The web feed is not followed.
};

// The Service, UI, or Setting which muted the tab.
enum class TabMutedReason {
  NONE,                    // The tab has never been muted or unmuted.
  EXTENSION,               // Mute state changed via extension API.
  AUDIO_INDICATOR,         // Mute toggled via tab-strip audio icon.
  CONTENT_SETTING,         // The sound content setting was set to BLOCK.
  CONTENT_SETTING_CHROME,  // Mute toggled on chrome:// URL.
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_ENUMS_H_
