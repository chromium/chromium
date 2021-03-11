// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_UTILS_H_
#define CHROME_BROWSER_UI_TABS_TAB_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "content/public/browser/web_contents_user_data.h"
#include "third_party/skia/include/core/SkColor.h"

class TabStripModel;

namespace content {
class WebContents;
}  // namespace content

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

enum class TabMutedReason {
  NONE,                    // The tab has never been muted or unmuted.
  CONTEXT_MENU,            // Mute/Unmute chosen from tab context menu.
  EXTENSION,               // Mute state changed via extension API.
  CONTENT_SETTING,         // The sound content setting was set to BLOCK.
  CONTENT_SETTING_CHROME,  // Mute toggled on chrome:// URL.
};

struct LastMuteMetadata
    : public content::WebContentsUserData<LastMuteMetadata> {
  TabMutedReason reason = TabMutedReason::NONE;
  std::string extension_id;  // Only valid when |reason| is EXTENSION.

 private:
  explicit LastMuteMetadata(content::WebContents* contents) {}
  friend class content::WebContentsUserData<LastMuteMetadata>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

namespace chrome {

// Returns the alert states to be shown by the tab's alert indicator.
// The returned list is in descending order of importance to user
// privacy, i.e. if only one is to be shown, it should be the first.
// TabAlertState::NONE will never be present in the list; an empty list
// is returned instead.
std::vector<TabAlertState> GetTabAlertStatesForContents(
    content::WebContents* contents);

// Returns a localized string describing the |alert_state|.
std::u16string GetTabAlertStateText(const TabAlertState alert_state);

// Sets whether all audio output from |contents| is muted, along with the
// |reason| it is to be muted/unmuted (via UI or extension API).  When |reason|
// is TAB_MUTED_REASON_EXTENSION, |extension_id| must be provided; otherwise, it
// is ignored.  Returns whether the tab was actually muted.
bool SetTabAudioMuted(content::WebContents* contents,
                      bool mute,
                      TabMutedReason reason,
                      const std::string& extension_id);

// Returns the last reason a tab's mute state was changed.
TabMutedReason GetTabAudioMutedReason(content::WebContents* contents);

// Returns true if the site at |index| in |tab_strip| is muted.
bool IsSiteMuted(const TabStripModel& tab_strip, const int index);

// Returns true if the sites at the |indices| in |tab_strip| are all muted.
bool AreAllSitesMuted(const TabStripModel& tab_strip,
                      const std::vector<int>& indices);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_TABS_TAB_UTILS_H_
