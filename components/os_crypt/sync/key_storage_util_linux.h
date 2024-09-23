// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_SYNC_KEY_STORAGE_UTIL_LINUX_H_
#define COMPONENTS_OS_CRYPT_SYNC_KEY_STORAGE_UTIL_LINUX_H_

#include <string>

#include "base/component_export.h"
#include "base/nix/xdg_util.h"

namespace base {
class FilePath;
}

namespace os_crypt {

// The supported Linux backends for storing passwords.
enum class SelectedLinuxBackend {
  DEFER,  // No selection
  BASIC_TEXT,
  GNOME_LIBSECRET,
  KWALLET,
  KWALLET5,
  KWALLET6,
};

// Decide which backend to target. |type| is checked first. If it does not
// match a supported backend and |use_backend| is true, |desktop_env| will be
// used to decide.
// TODO(crbug.com/40449930): This is exposed as a utility only for password
// manager to use. It should be merged into key_storage_linux, once no longer
// needed in password manager.
SelectedLinuxBackend COMPONENT_EXPORT(OS_CRYPT)
    SelectBackend(const std::string& type,
                  bool use_backend,
                  base::nix::DesktopEnvironment desktop_env);

// Set the setting that disables using OS-level encryption. If |use| is true,
// a backend will be used.
bool COMPONENT_EXPORT(OS_CRYPT)
    WriteBackendUse(const base::FilePath& user_data_dir, bool use);

// Decide whether the backend should be used based on the setting.
bool COMPONENT_EXPORT(OS_CRYPT)
    GetBackendUse(const base::FilePath& user_data_dir);

}  // namespace os_crypt

#endif  // COMPONENTS_OS_CRYPT_SYNC_KEY_STORAGE_UTIL_LINUX_H_
