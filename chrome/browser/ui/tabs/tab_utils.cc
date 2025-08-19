// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_utils.h"

#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"

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
LastMuteMetadata::~LastMuteMetadata() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(LastMuteMetadata);
