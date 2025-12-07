// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/window_management/window_management_content_setting_observer.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/webapps/isolated_web_apps/scheme.h"

namespace web_app {

WindowManagementContentSettingObserver::WindowManagementContentSettingObserver(
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<WindowManagementContentSettingObserver>(
          *contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  observation_.Observe(HostContentSettingsMapFactory::GetForProfile(profile));
}

WindowManagementContentSettingObserver::
    ~WindowManagementContentSettingObserver() = default;

void WindowManagementContentSettingObserver::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (content_type_set.Contains(ContentSettingsType::WINDOW_MANAGEMENT) &&
      web_contents()->GetLastCommittedURL().SchemeIs(
          webapps::kIsolatedAppScheme) &&
      primary_pattern.Matches(web_contents()->GetLastCommittedURL())) {
    web_contents()->OnWebPreferencesChanged();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WindowManagementContentSettingObserver);
}  // namespace web_app
