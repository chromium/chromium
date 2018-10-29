// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_utils.h"

#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/usb/usb_tab_helper.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"

namespace chrome {

TabAlertState GetTabAlertStateForContents(content::WebContents* contents) {
  if (!contents)
    return TabAlertState::NONE;

  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()->
          GetMediaStreamCaptureIndicator();
  if (indicator.get()) {
    // Currently we only show the icon and tooltip of the highest-priority
    // alert on a tab.
    // TODO(crbug.com/861961): To show the icon of the highest-priority alert
    // with tooltip that notes all the states in play.
    if (indicator->IsCapturingDesktop(contents))
      return TabAlertState::DESKTOP_CAPTURING;
    if (indicator->IsBeingMirrored(contents))
      return TabAlertState::TAB_CAPTURING;
    if (indicator->IsCapturingUserMedia(contents))
      return TabAlertState::MEDIA_RECORDING;
  }

  if (contents->IsConnectedToBluetoothDevice())
    return TabAlertState::BLUETOOTH_CONNECTED;

  UsbTabHelper* usb_tab_helper = UsbTabHelper::FromWebContents(contents);
  if (usb_tab_helper && usb_tab_helper->IsDeviceConnected())
    return TabAlertState::USB_CONNECTED;

  if (contents->HasPictureInPictureVideo())
    return TabAlertState::PIP_PLAYING;

  // Only tabs have a RecentlyAudibleHelper, but this function is abused for
  // some non-tab WebContents. In that case fall back to using the realtime
  // notion of audibility.
  bool audible = contents->IsCurrentlyAudible();
  auto* audible_helper = RecentlyAudibleHelper::FromWebContents(contents);
  if (audible_helper)
    audible = audible_helper->WasRecentlyAudible();
  if (audible) {
    if (contents->IsAudioMuted())
      return TabAlertState::AUDIO_MUTING;
    return TabAlertState::AUDIO_PLAYING;
  }

  return TabAlertState::NONE;
}

bool CanToggleAudioMute(content::WebContents* contents) {
  switch (chrome::GetTabAlertStateForContents(contents)) {
    case TabAlertState::NONE:
    case TabAlertState::AUDIO_PLAYING:
    case TabAlertState::AUDIO_MUTING:
    case TabAlertState::PIP_PLAYING:
      return true;
    case TabAlertState::MEDIA_RECORDING:
    case TabAlertState::TAB_CAPTURING:
    case TabAlertState::BLUETOOTH_CONNECTED:
    case TabAlertState::USB_CONNECTED:
    case TabAlertState::DESKTOP_CAPTURING:
      // The new Audio Service implements muting separately from the tab audio
      // capture infrastructure; so the mute state can be toggled independently
      // at all times.
      //
      // TODO(crbug.com/672469): Remove this method once the Audio Service is
      // launched.
      return base::FeatureList::IsEnabled(features::kAudioServiceAudioStreams);
  }
  NOTREACHED();
  return false;
}

TabMutedReason GetTabAudioMutedReason(content::WebContents* contents) {
  LastMuteMetadata::CreateForWebContents(contents);  // Ensures metadata exists.
  LastMuteMetadata* const metadata =
      LastMuteMetadata::FromWebContents(contents);
  if (GetTabAlertStateForContents(contents) == TabAlertState::TAB_CAPTURING &&
      !base::FeatureList::IsEnabled(features::kAudioServiceAudioStreams)) {
    // The legacy tab audio capture implementation in libcontent forces muting
    // off because it requires using the same infrastructure.
    //
    // TODO(crbug.com/672469): Remove this once the Audio Service is launched.
    // See comments in CanToggleAudioMute().
    metadata->reason = TabMutedReason::MEDIA_CAPTURE;
    metadata->extension_id.clear();
  }
  return metadata->reason;
}

bool SetTabAudioMuted(content::WebContents* contents,
                      bool mute,
                      TabMutedReason reason,
                      const std::string& extension_id) {
  DCHECK(contents);
  DCHECK(TabMutedReason::NONE != reason);

  if (!chrome::CanToggleAudioMute(contents))
    return false;

  contents->SetAudioMuted(mute);

  LastMuteMetadata::CreateForWebContents(contents);  // Ensures metadata exists.
  LastMuteMetadata* const metadata =
      LastMuteMetadata::FromWebContents(contents);
  metadata->reason = reason;
  if (reason == TabMutedReason::EXTENSION) {
    DCHECK(!extension_id.empty());
    metadata->extension_id = extension_id;
  } else {
    metadata->extension_id.clear();
  }

  return true;
}

bool AreAllTabsMuted(const TabStripModel& tab_strip,
                     const std::vector<int>& indices) {
  for (auto i = indices.begin(); i != indices.end(); ++i) {
    if (!tab_strip.GetWebContentsAt(*i)->IsAudioMuted())
      return false;
  }
  return true;
}

bool IsSiteMuted(const TabStripModel& tab_strip, const int index) {
  content::WebContents* web_contents = tab_strip.GetWebContentsAt(index);

  // Prevent crashes with null WebContents (https://crbug.com/797647).
  if (!web_contents)
    return false;

  GURL url = web_contents->GetLastCommittedURL();

  // chrome:// URLs don't have content settings but can be muted, so just check
  // the current muted state and TabMutedReason of the WebContents.
  if (url.SchemeIs(content::kChromeUIScheme)) {
    return web_contents->IsAudioMuted() &&
           GetTabAudioMutedReason(web_contents) ==
               TabMutedReason::CONTENT_SETTING_CHROME;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* settings =
      HostContentSettingsMapFactory::GetForProfile(profile);
  return settings->GetContentSetting(url, url, CONTENT_SETTINGS_TYPE_SOUND,
                                     std::string()) == CONTENT_SETTING_BLOCK;
}

bool AreAllSitesMuted(const TabStripModel& tab_strip,
                      const std::vector<int>& indices) {
  for (int tab_index : indices) {
    if (!IsSiteMuted(tab_strip, tab_index))
      return false;
  }
  return true;
}

}  // namespace chrome
