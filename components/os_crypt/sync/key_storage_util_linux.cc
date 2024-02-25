// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/sync/key_storage_util_linux.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"

namespace {

// OSCrypt has a setting that determines whether a backend will be used.
// The presense of this file in the file system means that the backend
// should be ignored. It's absence means we should use the backend.
constexpr const char kPreferenceFileName[] = "Disable Local Encryption";

bool ReadBackendUse(const base::FilePath& user_data_dir, bool* use) {
  if (user_data_dir.empty())
    return false;
  base::FilePath pref_path = user_data_dir.Append(kPreferenceFileName);
  *use = !base::PathExists(pref_path);
  return true;
}

}  // namespace

namespace os_crypt {

SelectedLinuxBackend SelectBackend(const std::string& type,
                                   bool use_backend,
                                   base::nix::DesktopEnvironment desktop_env) {
  // Explicitly requesting a store overrides other production logic.
  if (type == "kwallet")
    return SelectedLinuxBackend::KWALLET;
  if (type == "kwallet5")
    return SelectedLinuxBackend::KWALLET5;
  if (type == "kwallet6")
    return SelectedLinuxBackend::KWALLET6;
  if (type == "gnome-libsecret")
    return SelectedLinuxBackend::GNOME_LIBSECRET;
  if (type == "basic")
    return SelectedLinuxBackend::BASIC_TEXT;

  // Ignore the backends if requested to.
  if (!use_backend)
    return SelectedLinuxBackend::BASIC_TEXT;

  // Detect the store to use automatically.
  const char* name = base::nix::GetDesktopEnvironmentName(desktop_env);
  VLOG(1) << "Password storage detected desktop environment: "
          << (name ? name : "(unknown)");
  switch (desktop_env) {
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
      return SelectedLinuxBackend::KWALLET;
    case base::nix::DESKTOP_ENVIRONMENT_KDE5:
      return SelectedLinuxBackend::KWALLET5;
    case base::nix::DESKTOP_ENVIRONMENT_KDE6:
      return SelectedLinuxBackend::KWALLET6;
    case base::nix::DESKTOP_ENVIRONMENT_CINNAMON:
    case base::nix::DESKTOP_ENVIRONMENT_DEEPIN:
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
    case base::nix::DESKTOP_ENVIRONMENT_PANTHEON:
    case base::nix::DESKTOP_ENVIRONMENT_UKUI:
    case base::nix::DESKTOP_ENVIRONMENT_UNITY:
    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
      return SelectedLinuxBackend::GNOME_LIBSECRET;
    // KDE3 didn't use DBus, which our KWallet store uses.
    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
    case base::nix::DESKTOP_ENVIRONMENT_LXQT:
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      return SelectedLinuxBackend::BASIC_TEXT;
  }
}

bool WriteBackendUse(const base::FilePath& user_data_dir, bool use) {
  if (user_data_dir.empty())
    return false;
  base::FilePath pref_path = user_data_dir.Append(kPreferenceFileName);
  if (use)
    return base::DeleteFile(pref_path);
  FILE* f = base::OpenFile(pref_path, "w");
  return f != nullptr && base::CloseFile(f);
}

bool GetBackendUse(const base::FilePath& user_data_dir) {
  bool setting;
  if (ReadBackendUse(user_data_dir, &setting))
    return setting;
  return true;
}

}  // namespace os_crypt
