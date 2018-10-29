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

// Alert state for a tab.  In reality, more than one of these may apply.  See
// comments for GetTabAlertStateForContents() below.
enum class TabAlertState {
  NONE,
  MEDIA_RECORDING,      // Audio/Video being recorded, consumed by tab.
  TAB_CAPTURING,        // Tab contents being captured.
  AUDIO_PLAYING,        // Audible audio is playing from the tab.
  AUDIO_MUTING,         // Tab audio is being muted.
  BLUETOOTH_CONNECTED,  // Tab is connected to a BT Device.
  USB_CONNECTED,        // Tab is connected to a USB device.
  PIP_PLAYING,          // Tab contains a video in Picture-in-Picture mode.
  DESKTOP_CAPTURING,    // Desktop contents being recorded, consumed by tab.
};

enum class TabMutedReason {
  NONE,                    // The tab has never been muted or unmuted.
  CONTEXT_MENU,            // Mute/Unmute chosen from tab context menu.
  MEDIA_CAPTURE,           // Media recording/capture was started.
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
};

namespace chrome {

// Returns the alert state to be shown by the tab's alert indicator.  When
// multiple states apply (e.g., tab capture with audio playback), the one most
// relevant to user privacy concerns is selected.
TabAlertState GetTabAlertStateForContents(content::WebContents* contents);

// Returns true if audio mute can be activated/deactivated for the given
// |contents|.
bool CanToggleAudioMute(content::WebContents* contents);

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

// Returns true if the tabs at the |indices| in |tab_strip| are all muted.
bool AreAllTabsMuted(const TabStripModel& tab_strip,
                     const std::vector<int>& indices);

// Returns true if the site at |index| in |tab_strip| is muted.
bool IsSiteMuted(const TabStripModel& tab_strip, const int index);

// Returns true if the sites at the |indices| in |tab_strip| are all muted.
bool AreAllSitesMuted(const TabStripModel& tab_strip,
                      const std::vector<int>& indices);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_TABS_TAB_UTILS_H_
