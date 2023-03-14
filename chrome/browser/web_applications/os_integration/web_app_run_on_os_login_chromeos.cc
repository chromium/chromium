// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_run_on_os_login.h"

#include "chrome/browser/web_applications/web_app_constants.h"

namespace web_app {

namespace internals {

bool RegisterRunOnOsLogin(const ShortcutInfo& shortcut_info) {
  // `web_app::ScheduleRegisterRunOnOsLogin` already updates the
  // RunOnOsLoginMode of the WebApp. On ChromeOS, the RunOnOsLoginMode is
  // checked after Login to start the WebApp as no platform shortcuts can be
  // used. Return true to ensure that InstallOsHooks does not fail here.
  return true;
}

Result UnregisterRunOnOsLogin(const std::string& app_id,
                              const base::FilePath& profile_path,
                              const std::u16string& shortcut_title) {
  // `web_app::ScheduleRegisterRunOnOsLogin` already updates the
  // RunOnOsLoginMode of the WebApp. No shortcuts needs to be removed.
  return Result::kOk;
}

}  // namespace internals

}  // namespace web_app
