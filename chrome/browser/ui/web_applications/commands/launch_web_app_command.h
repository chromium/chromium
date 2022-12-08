// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_COMMANDS_LAUNCH_WEB_APP_COMMAND_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_COMMANDS_LAUNCH_WEB_APP_COMMAND_H_

#include "chrome/browser/web_applications/web_app_ui_manager.h"

class Profile;

namespace apps {
struct AppLaunchParams;
}

namespace web_app {

class AppLock;

base::Value LaunchWebApp(apps::AppLaunchParams params,
                         LaunchWebAppWindowSetting launch_setting,
                         Profile& profile,
                         LaunchWebAppCallback callback,
                         AppLock& lock);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_COMMANDS_LAUNCH_WEB_APP_COMMAND_H_
