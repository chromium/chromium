// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_utils.h"

#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "ui/base/l10n/l10n_util.h"

std::u16string GetTabAlertStateText(const tabs::TabAlert alert_state) {
  switch (alert_state) {
    case tabs::TabAlert::AUDIO_PLAYING:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_AUDIO_PLAYING);
    case tabs::TabAlert::AUDIO_MUTING:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_AUDIO_MUTING);
    case tabs::TabAlert::MEDIA_RECORDING:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_MEDIA_RECORDING);
    case tabs::TabAlert::AUDIO_RECORDING:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_AUDIO_RECORDING);
    case tabs::TabAlert::VIDEO_RECORDING:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_VIDEO_RECORDING);
    case tabs::TabAlert::TAB_CAPTURING:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_TAB_CAPTURING);
    case tabs::TabAlert::BLUETOOTH_CONNECTED:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_BLUETOOTH_CONNECTED);
    case tabs::TabAlert::BLUETOOTH_SCAN_ACTIVE:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_BLUETOOTH_SCAN_ACTIVE);
    case tabs::TabAlert::USB_CONNECTED:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_USB_CONNECTED);
    case tabs::TabAlert::HID_CONNECTED:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_HID_CONNECTED);
    case tabs::TabAlert::SERIAL_CONNECTED:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_SERIAL_CONNECTED);
    case tabs::TabAlert::PIP_PLAYING:
      return l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ALERT_STATE_PIP_PLAYING);
    case tabs::TabAlert::DESKTOP_CAPTURING:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_DESKTOP_CAPTURING);
    case tabs::TabAlert::VR_PRESENTING_IN_HEADSET:
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_VR_PRESENTING);
    // TODO(crbug.com/422538779) Create new resources for ACTOR_ACCESSING of
    // relying on GLIC_ACCESSING resources below.
    case tabs::TabAlert::ACTOR_ACCESSING:
    case tabs::TabAlert::GLIC_ACCESSING:
#if BUILDFLAG(ENABLE_GLIC)
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_GLIC_ACCESSING);
#else
      return u"";
#endif
    case tabs::TabAlert::GLIC_SHARING:
#if BUILDFLAG(ENABLE_GLIC)
      return l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_GLIC_SHARING);
#else
      return u"";
#endif
  }
  NOTREACHED();
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
  if (!web_contents) {
    return false;
  }

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
    if (!IsSiteMuted(tab_strip, tab_index)) {
      return false;
    }
  }
  return true;
}

LastMuteMetadata::LastMuteMetadata(content::WebContents* contents)
    : content::WebContentsUserData<LastMuteMetadata>(*contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LastMuteMetadata);
