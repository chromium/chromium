// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_WINDOW_MANAGEMENT_CONTENT_SETTING_OBSERVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_WINDOW_MANAGEMENT_CONTENT_SETTING_OBSERVER_H_

#include "base/scoped_observation.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace web_app {

// Observes changes of ContentSetting::WINDOW_MANAGEMENT and notifies
// WebContents of change. This is necessary because whether window.close is
// allowed to be called by scripts depends on the content setting. Only needed
// for IsolatedWebApps.
class WindowManagementContentSettingObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          WindowManagementContentSettingObserver>,
      public content_settings::Observer {
 public:
  WindowManagementContentSettingObserver(
      WindowManagementContentSettingObserver& observer) = delete;
  WindowManagementContentSettingObserver& operator=(
      WindowManagementContentSettingObserver& observer) = delete;
  ~WindowManagementContentSettingObserver() override;

  // content_settings::Observer implementation.
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

 private:
  explicit WindowManagementContentSettingObserver(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      WindowManagementContentSettingObserver>;

  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      observation_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_WINDOW_MANAGEMENT_CONTENT_SETTING_OBSERVER_H_
