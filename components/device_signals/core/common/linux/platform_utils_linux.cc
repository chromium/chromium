// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/platform_utils.h"

#if defined(USE_GIO)
#include <gio/gio.h>
#endif  // defined(USE_GIO)
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <algorithm>
#include <optional>
#include <string>

#include "base/environment.h"
#include "base/files/dir_reader_posix.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/nix/xdg_util.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "components/device_signals/core/common/signals_constants.h"
#if defined(USE_GIO)
#include "ui/base/glib/gsettings.h"
#endif  // defined(USE_GIO)

namespace {
std::string ReadFile(std::string path_str) {
  base::FilePath path(path_str);
  std::string output;
  if (base::PathExists(path) && base::ReadFileToString(path, &output)) {
    base::TrimWhitespaceASCII(output, base::TrimPositions::TRIM_ALL, &output);
  }

  return output;
}
}  // namespace

namespace device_signals {

std::string GetDeviceModel() {
  return ReadFile("/sys/class/dmi/id/product_name");
}

std::string GetSerialNumber() {
  return ReadFile("/sys/class/dmi/id/product_serial");
}

base::FilePath GetCrowdStrikeZtaFilePath() {
  // ZTA files currently are not stored locally on linux platforms.
  return base::FilePath();
}

// Implements the logic from the native client setup script. It reads the
// setting value straight from gsettings but picks the schema relevant to the
// currently active desktop environment.
// The current implementation support Gnone and Cinnamon only.
SettingValue GetScreenlockSecured() {
#if defined(USE_GIO)
  static constexpr char kLockScreenKey[] = "lock-enabled";

  std::unique_ptr<base::Environment> env(base::Environment::Create());
  const base::nix::DesktopEnvironment desktop_env =
      base::nix::GetDesktopEnvironment(env.get());
  if (desktop_env != base::nix::DESKTOP_ENVIRONMENT_CINNAMON &&
      desktop_env != base::nix::DESKTOP_ENVIRONMENT_GNOME) {
    return SettingValue::UNKNOWN;
  }

  const std::string settings_schema = base::StringPrintf(
      "org.%s.desktop.screensaver",
      desktop_env == base::nix::DESKTOP_ENVIRONMENT_CINNAMON ? "cinnamon"
                                                             : "gnome");

  auto screensaver_settings = ui::GSettingsNew(settings_schema.c_str());
  if (!screensaver_settings) {
    return SettingValue::UNKNOWN;
  }
  GSettingsSchema* screensaver_schema = g_settings_schema_source_lookup(
      g_settings_schema_source_get_default(), settings_schema.c_str(), true);
  if (!g_settings_schema_has_key(screensaver_schema, kLockScreenKey)) {
    return SettingValue::UNKNOWN;
  }
  g_settings_schema_unref(screensaver_schema);
  gboolean lock_screen_enabled =
      g_settings_get_boolean(screensaver_settings, kLockScreenKey);

  return lock_screen_enabled ? SettingValue::ENABLED : SettingValue::DISABLED;
#else
  return SettingValue::UNKNOWN;
#endif  // defined(USE_GIO)
}

// Implements the logic from the native host installation script. First find the
// root device identifier, then locate its parent and get its type.
SettingValue GetDiskEncrypted() {
  struct stat info;
  // First figure out the device identifier. Fail fast if this fails.
  if (stat("/", &info) != 0) {
    return SettingValue::UNKNOWN;
  }
  int dev_major = major(info.st_dev);
  // The parent identifier will have the same major and minor 0. If and only if
  // it is a dm device can it also be an encrypted device (as evident from the
  // source code of the lsblk command).
  base::FilePath dev_uuid(
      base::StringPrintf("/sys/dev/block/%d:0/dm/uuid", dev_major));
  std::string uuid;
  if (base::PathExists(dev_uuid)) {
    if (base::ReadFileToStringWithMaxSize(dev_uuid, &uuid, 1024)) {
      // The device uuid starts with the driver type responsible for it. If it
      // is the "crypt" driver then it is an encrypted device.
      bool is_encrypted = base::StartsWith(
          uuid, "crypt-", base::CompareCase::INSENSITIVE_ASCII);
      return is_encrypted ? SettingValue::ENABLED : SettingValue::DISABLED;
    }
    return SettingValue::UNKNOWN;
  }
  return SettingValue::DISABLED;
}

std::vector<std::string> internal::GetMacAddressesImpl() {
  std::vector<std::string> result;
  base::DirReaderPosix reader("/sys/class/net");
  if (!reader.IsValid()) {
    return result;
  }
  while (reader.Next()) {
    std::string name = reader.name();
    if (name == "." || name == "..") {
      continue;
    }
    std::string address;
    base::FilePath address_file(
        base::StringPrintf("/sys/class/net/%s/address", name.c_str()));
    // Filter out the loopback interface here.
    if (!base::PathExists(address_file) ||
        !base::ReadFileToStringWithMaxSize(address_file, &address, 1024) ||
        base::StartsWith(address, "00:00:00:00:00:00",
                         base::CompareCase::SENSITIVE)) {
      continue;
    }

    base::TrimWhitespaceASCII(address, base::TrimPositions::TRIM_TRAILING,
                              &address);
    result.push_back(address);
  }
  return result;
}

std::optional<std::string> GetDistributionVersion() {
  base::FilePath os_release_file("/etc/os-release");
  std::string release_info;
  base::StringPairs values;
  if (base::PathExists(os_release_file) &&
      base::ReadFileToStringWithMaxSize(os_release_file, &release_info, 8192) &&
      base::SplitStringIntoKeyValuePairs(release_info, '=', '\n', &values)) {
    auto version_id = std::ranges::find(
        values, "VERSION_ID", &std::pair<std::string, std::string>::first);
    if (version_id != values.end()) {
      return std::string(
          base::TrimString(version_id->second, "\"", base::TRIM_ALL));
    }
  }

  return std::nullopt;
}

std::optional<CrowdStrikeSignals> GetCrowdStrikeSignals() {
  return std::nullopt;
}

}  // namespace device_signals
