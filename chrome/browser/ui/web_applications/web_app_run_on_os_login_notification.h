// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_RUN_ON_OS_LOGIN_NOTIFICATION_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_RUN_ON_OS_LOGIN_NOTIFICATION_H_

#include <vector>
#include "chrome/browser/profiles/profile.h"

namespace web_app {

extern const char kRunOnOsLoginNotificationId[];
extern const char kRunOnOsLoginNotifierId[];

void DisplayRunOnOsLoginNotification(const std::vector<std::string>& app_names,
                                     base::WeakPtr<Profile> profile);
}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_RUN_ON_OS_LOGIN_NOTIFICATION_H_
