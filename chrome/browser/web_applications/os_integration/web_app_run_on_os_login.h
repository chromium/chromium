// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_RUN_ON_OS_LOGIN_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_RUN_ON_OS_LOGIN_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/webapps/common/web_app_id.h"

namespace base {
class FilePath;
}

namespace web_app {

struct ShortcutInfo;

namespace internals {

// Registers the app with the OS to run on OS login. Platform specific
// implementations are required for this.
// Invoke `callback` on the Shortcut IO thread when the work is complete.
// See web_app_run_on_os_login_win.cc for Windows implementation as example.
void RegisterRunOnOsLogin(const ShortcutInfo& shortcut_info,
                          ResultCallback callback);

// Unregisters the app with the OS from running on startup. Platform specific
// implementations are required for this.
// See web_app_run_on_os_login_win.cc for Windows implementation as example.
Result UnregisterRunOnOsLogin(const std::string& app_id,
                              const base::FilePath& profile_path,
                              const std::u16string& shortcut_title);

}  // namespace internals

// Schedules a call to |RegisterRunOnOsLogin| on the Shortcut IO thread and
// invokes |callback| on the UI thread when complete. This function must be
// called from the UI thread.
void ScheduleRegisterRunOnOsLogin(std::unique_ptr<ShortcutInfo> shortcut_info,
                                  ResultCallback callback);

// Schedules a call to |UnregisterRunOnOsLogin| on the Shortcut IO thread and
// invokes |callback| when complete. This function must be called from the UI
// thread.
void ScheduleUnregisterRunOnOsLogin(const std::string& app_id,
                                    const base::FilePath& profile_path,
                                    const std::u16string& shortcut_title,
                                    ResultCallback callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_RUN_ON_OS_LOGIN_H_
