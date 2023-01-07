// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_run_on_os_login.h"

#include "chrome/browser/web_applications/web_app_constants.h"

namespace web_app {

namespace internals {

// This boilerplate function is used for platforms that don't support Run On OS
// Login.
bool RegisterRunOnOsLogin(const ShortcutInfo& shortcut_info) {
  return false;
}

// This boilerplate function is used for platforms that don't support Run On OS
// Login.
Result UnregisterRunOnOsLogin(const std::string& app_id,
                              const base::FilePath& profile_path,
                              const std::u16string& shortcut_title) {
  return Result::kOk;
}

}  // namespace internals

}  // namespace web_app
