// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_ENUMS_H_
#define CHROME_BROWSER_UI_TABS_TAB_ENUMS_H_

// Alert states for a tab. Any number of these (or none) may apply at once.
enum class TabAlertState {
  MEDIA_RECORDING,        // Audio/Video [both] being recorded, consumed by tab.
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
  AUDIO_RECORDING,           // Audio [only] being recorded, consumed by tab.
  VIDEO_RECORDING,           // Video [only] being recorded, consumed by tab.
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

// A BitField used to specify what should happen when the tab is closed.
enum TabCloseTypes {
  CLOSE_NONE = 0,

  // Indicates the tab was closed by the user. If true,
  // WebContents::SetClosedByUserGesture(true) is invoked.
  CLOSE_USER_GESTURE = 1 << 0,

  // If true the history is recorded so that the tab can be reopened later. You
  // almost always want to set this.
  CLOSE_CREATE_HISTORICAL_TAB = 1 << 1,

};

// Constants used when adding tabs.
enum AddTabTypes {
  // Used to indicate nothing special should happen to the newly inserted tab.
  ADD_NONE = 0,

  // The tab should be active.
  ADD_ACTIVE = 1 << 0,

  // The tab should be pinned.
  ADD_PINNED = 1 << 1,

  // If not set the insertion index of the WebContents is left up to the Order
  // Controller associated, so the final insertion index may differ from the
  // specified index. Otherwise the index supplied is used.
  ADD_FORCE_INDEX = 1 << 2,

  // If set the newly inserted tab's opener is set to the active tab. If not
  // set the tab may still inherit the opener under certain situations.
  ADD_INHERIT_OPENER = 1 << 3,
};

// Enumerates different ways to open a new tab. Does not apply to opening
// existing links or searches in a new tab, only to brand new empty tabs.
// KEEP IN SYNC WITH THE NewTabType ENUM IN enums.xml.
// NEW VALUES MUST BE APPENDED AND AVOID CHANGING ANY PRE-EXISTING VALUES.
enum NewTabTypes {
  // New tab was opened using the new tab button on the tab strip.
  NEW_TAB_BUTTON = 0,

  // New tab was opened using the menu command - either through the keyboard
  // shortcut, or by opening the menu and selecting the command. Applies to
  // both app menu and the menu bar's File menu (on platforms that have one).
  NEW_TAB_COMMAND = 1,

  // New tab was opened through the context menu on the tab strip.
  NEW_TAB_CONTEXT_MENU = 2,

  // New tab was opened through the new tab button in the toolbar for the
  // WebUI touch-optimized tab strip.
  NEW_TAB_BUTTON_IN_TOOLBAR_FOR_TOUCH = 3,

  // New tab was opened through the new tab button inside of the WebUI tab
  // strip.
  NEW_TAB_BUTTON_IN_WEBUI_TAB_STRIP = 4,

  // Number of enum entries, used for UMA histogram reporting macros.
  NEW_TAB_ENUM_COUNT = 5,
};

// Enumerates different types of tab activation. Mainly used for
// comparison between classic tab strip and WebUI tab strip.
// KEEP IN SYNC WITH THE TabActivationTypes ENUM IN enums.xml.
// NEW VALUES MUST BE APPENDED AND AVOID CHANGING ANY PRE-EXISTING VALUES.
enum class TabActivationTypes {
  // Switch to a tab.
  kTab = 0,

  // Open the context menu of a tab.
  kContextMenu = 1,

  kMaxValue = kContextMenu,
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_ENUMS_H_
