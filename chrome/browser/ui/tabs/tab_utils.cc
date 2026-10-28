// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_utils.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_muted_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

bool IsSiteMuted(tabs::TabHandle tab_handle) {
  tabs::TabInterface* tab = tab_handle.Get();
  if (!tab) {
    return false;
  }
  content::WebContents* web_contents = tab->GetContents();

  // Prevent crashes with null WebContents (https://crbug.com/41362347).
  if (!web_contents) {
    return false;
  }

  GURL url = web_contents->GetLastCommittedURL();

  // chrome:// URLs don't have content settings but can be muted, so just check
  // the current muted state and TabMutedReason of the WebContents.
  if (url.SchemeIs(content::kChromeUIScheme)) {
    return web_contents->IsAudioMuted() &&
           GetTabAudioMutedReason(web_contents) ==
               TabMutedReason::kContentSettingChrome;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* settings =
      HostContentSettingsMapFactory::GetForProfile(profile);
  return settings->GetContentSetting(url, url, ContentSettingsType::SOUND) ==
         CONTENT_SETTING_BLOCK;
}

bool AreAllSitesMuted(const std::vector<tabs::TabHandle>& tabs) {
  for (tabs::TabHandle tab : tabs) {
    if (!IsSiteMuted(tab)) {
      return false;
    }
  }
  return true;
}
