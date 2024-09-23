// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/android/popup_blocked_helper.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "url/gurl.h"

namespace blocked_content {

void ShowBlockedPopups(content::WebContents* web_contents) {
  blocked_content::PopupBlockerTabHelper* popup_blocker_helper =
      blocked_content::PopupBlockerTabHelper::FromWebContents(web_contents);
  DCHECK(popup_blocker_helper);
  popup_blocker_helper->ShowAllBlockedPopups();
}

bool PopupSettingManagedByPolicy(HostContentSettingsMap* map, const GURL& url) {
  content_settings::SettingInfo setting_info;
  const base::Value setting = map->GetWebsiteSetting(
      url, url, ContentSettingsType::POPUPS, &setting_info);
  return setting_info.source == content_settings::SettingSource::kPolicy;
}
}  // namespace blocked_content
