// Copyright 2012 The Chromium Authors
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
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "ui/base/l10n/l10n_util.h"

std::vector<TabAlertState> GetTabAlertStatesForContents(
    content::WebContents* contents) {
  std::vector<TabAlertState> states;
  if (!contents)
    return states;

  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()->
          GetMediaStreamCaptureIndicator();
  if (indicator.get()) {
    // Currently we only show the icon and tooltip of the highest-priority
    // alert on a tab.
    // TODO(crbug.com/40584226): To show the icon of the highest-priority alert
    // with tooltip that notes all the states in play.
    if (indicator->IsCapturingWindow(contents) ||
        indicator->IsCapturingDisplay(contents)) {
      states.push_back(TabAlertState::DESKTOP_CAPTURING);
    }
    if (indicator->IsBeingMirrored(contents))
      states.push_back(TabAlertState::TAB_CAPTURING);

    if (indicator->IsCapturingAudio(contents) &&
        indicator->IsCapturingVideo(contents)) {
      states.push_back(TabAlertState::MEDIA_RECORDING);
    } else if (indicator->IsCapturingAudio(contents)) {
      states.push_back(TabAlertState::AUDIO_RECORDING);
    } else if (indicator->IsCapturingVideo(contents)) {
      states.push_back(TabAlertState::VIDEO_RECORDING);
    }
  }

  if (contents->IsConnectedToBluetoothDevice())
    states.push_back(TabAlertState::BLUETOOTH_CONNECTED);

  if (contents->IsScanningForBluetoothDevices())
    states.push_back(TabAlertState::BLUETOOTH_SCAN_ACTIVE);

  if (contents->IsConnectedToUsbDevice())
    states.push_back(TabAlertState::USB_CONNECTED);

  if (contents->IsConnectedToHidDevice())
    states.push_back(TabAlertState::HID_CONNECTED);

  if (contents->IsConnectedToSerialPort())
    states.push_back(TabAlertState::SERIAL_CONNECTED);

  // Check if VR content is being presented in a headset.
  // NOTE: This icon must take priority over the audio alert ones
  // because most VR content has audio and its usage is implied by the VR
  // icon.
  if (vr::VrTabHelper::IsContentDisplayedInHeadset(contents))
    states.push_back(TabAlertState::VR_PRESENTING_IN_HEADSET);

  if (contents->HasPictureInPictureVideo() ||
      contents->HasPictureInPictureDocument())
    states.push_back(TabAlertState::PIP_PLAYING);

  // Only tabs have a RecentlyAudibleHelper, but this function is abused for
  // some non-tab WebContents. In that case fall back to using the realtime
  // notion of audibility.
  bool audible = contents->IsCurrentlyAudible();
  auto* audible_helper = RecentlyAudibleHelper::FromWebContents(contents);
  if (audible_helper)
    audible = audible_helper->WasRecentlyAudible();
  if (audible) {
    if (contents->IsAudioMuted())
      states.push_back(TabAlertState::AUDIO_MUTING);
    states.push_back(TabAlertState::AUDIO_PLAYING);
  }

  return states;
}

std::u16string GetTabAlertStateText(const TabAlertState alert_state) {
  switch (alert_state) {
    case TabAlertState::AUDIO_PLAYING:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_AUDIO_PLAYING);
    case TabAlertState::AUDIO_MUTING:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_AUDIO_MUTING);
    case TabAlertState::MEDIA_RECORDING:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_MEDIA_RECORDING);
    case TabAlertState::AUDIO_RECORDING:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_AUDIO_RECORDING);
    case TabAlertState::VIDEO_RECORDING:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_VIDEO_RECORDING);
    case TabAlertState::TAB_CAPTURING:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_TAB_CAPTURING);
    case TabAlertState::BLUETOOTH_CONNECTED:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_BLUETOOTH_CONNECTED);
    case TabAlertState::BLUETOOTH_SCAN_ACTIVE:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_BLUETOOTH_SCAN_ACTIVE);
    case TabAlertState::USB_CONNECTED:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_USB_CONNECTED);
    case TabAlertState::HID_CONNECTED:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_HID_CONNECTED);
    case TabAlertState::SERIAL_CONNECTED:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_SERIAL_CONNECTED);
    case TabAlertState::PIP_PLAYING:
      return l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ALERT_STATE_PIP_PLAYING);
    case TabAlertState::DESKTOP_CAPTURING:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_DESKTOP_CAPTURING);
    case TabAlertState::VR_PRESENTING_IN_HEADSET:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_VR_PRESENTING);
  }
  NOTREACHED_IN_MIGRATION();
  return std::u16string();
}

TabMutedReason GetTabAudioMutedReason(content::WebContents* contents) {
  LastMuteMetadata::CreateForWebContents(contents);  // Ensures metadata exists.
  LastMuteMetadata* const metadata =
      LastMuteMetadata::FromWebContents(contents);
  return metadata->reason;
}

bool SetTabAudioMuted(content::WebContents* contents,
                      bool mute,
                      TabMutedReason reason,
                      const std::string& extension_id) {
  DCHECK(contents);
  DCHECK(TabMutedReason::NONE != reason);

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
  return settings->GetContentSetting(url, url, ContentSettingsType::SOUND) ==
         CONTENT_SETTING_BLOCK;
}

bool AreAllSitesMuted(const TabStripModel& tab_strip,
                      const std::vector<int>& indices) {
  for (int tab_index : indices) {
    if (!IsSiteMuted(tab_strip, tab_index))
      return false;
  }
  return true;
}

LastMuteMetadata::LastMuteMetadata(content::WebContents* contents)
    : content::WebContentsUserData<LastMuteMetadata>(*contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LastMuteMetadata);
