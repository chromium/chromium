// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_RUN_ON_OS_LOGIN_NOTIFICATION_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_RUN_ON_OS_LOGIN_NOTIFICATION_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace web_app {

extern const char kRunOnOsLoginNotificationId[];
extern const char kRunOnOsLoginNotifierId[];

void DisplayRunOnOsLoginNotification(
    const base::flat_map<webapps::AppId,
                         WebAppUiManager::RoolNotificationBehavior>& apps,
    base::WeakPtr<Profile> profile);
}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_RUN_ON_OS_LOGIN_NOTIFICATION_H_
