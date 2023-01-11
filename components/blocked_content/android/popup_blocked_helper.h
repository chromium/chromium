// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOCKED_CONTENT_ANDROID_POPUP_BLOCKED_HELPER_H_
#define COMPONENTS_BLOCKED_CONTENT_ANDROID_POPUP_BLOCKED_HELPER_H_

#include "base/functional/callback.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace blocked_content {

void ShowBlockedPopups(content::WebContents* web_contents);
bool PopupSettingManagedByPolicy(HostContentSettingsMap* map, const GURL& url);
}  // namespace blocked_content
#endif  // COMPONENTS_BLOCKED_CONTENT_ANDROID_POPUP_BLOCKED_HELPER_H_
