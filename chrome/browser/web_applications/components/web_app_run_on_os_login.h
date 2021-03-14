// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_RUN_ON_OS_LOGIN_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_RUN_ON_OS_LOGIN_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"

namespace base {
class FilePath;
}

namespace web_app {

struct ShortcutInfo;

// Callback made when RegisterRunOnOsLogin has finished trying to register the
// app to the OS Startup indicating whether or not it was successfully
// registered.
using RegisterRunOnOsLoginCallback = base::OnceCallback<void(bool success)>;

// Callback made when UnregisterRunOnOslogin has finished indicating whether or
// not it was successfully unregistered.
using UnregisterRunOnOsLoginCallback = base::OnceCallback<void(bool success)>;

namespace internals {

// Registers the app with the OS to run on OS login. Platform specific
// implementations are required for this.
// See web_app_run_on_os_login_win.cc for Windows implementation as example.
bool RegisterRunOnOsLogin(const ShortcutInfo& shortcut_info);

// Unregisters the app with the OS from running on startup. Platform specific
// implementations are required for this.
// See web_app_run_on_os_login_win.cc for Windows implementation as example.
bool UnregisterRunOnOsLogin(const std::string& app_id,
                            const base::FilePath& profile_path,
                            const std::u16string& shortcut_title);

}  // namespace internals

// Schedules a call to |RegisterRunOnOsLogin| on the Shortcut IO thread and
// invokes |callback| when complete. This function must be called from the UI
// thread.
void ScheduleRegisterRunOnOsLogin(std::unique_ptr<ShortcutInfo> shortcut_info,
                                  RegisterRunOnOsLoginCallback callback);

// Schedules a call to |UnregisterRunOnOsLogin| on the Shortcut IO thread and
// invokes |callback| when complete. This function must be called from the UI
// thread.
void ScheduleUnregisterRunOnOsLogin(const std::string& app_id,
                                    const base::FilePath& profile_path,
                                    const std::u16string& shortcut_title,
                                    UnregisterRunOnOsLoginCallback callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_RUN_ON_OS_LOGIN_H_
