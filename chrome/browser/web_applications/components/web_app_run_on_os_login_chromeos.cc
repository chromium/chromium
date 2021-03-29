// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_run_on_os_login.h"

namespace web_app {

namespace internals {

// This boilerplate function is used for platforms that don't support Run On OS
// Login.
bool RegisterRunOnOsLogin(const ShortcutInfo& shortcut_info) {
  return false;
}

// This boilerplate function is used for platforms that don't support Run On OS
// Login.
bool UnregisterRunOnOsLogin(const std::string& app_id,
                            const base::FilePath& profile_path,
                            const std::u16string& shortcut_title) {
  return true;
}

}  // namespace internals

}  // namespace web_app
