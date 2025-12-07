// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_PER_INSTALL_VALUES_H_
#define CHROME_INSTALLER_UTIL_PER_INSTALL_VALUES_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/values.h"
#include "base/win/win_handle_types.h"

namespace installer {

// `PerInstallValue` is used to store named persisted `base::Value` objects on a
// per-install basis. i.e., a single `base::Value` stored in a system-wide
// location for system installs, and a single `base::Value` stored per-user for
// user installs. The values are stored in the registry under
// `Google\Update\ClientState{Medium}\{ChromeAppId}\PerInstallValue` for branded
// installs.
class PerInstallValue {
 public:
  explicit PerInstallValue(std::wstring_view name);
  PerInstallValue(const PerInstallValue&) = delete;
  PerInstallValue& operator=(const PerInstallValue&) = delete;
  ~PerInstallValue();

  // Sets/gets/deletes the PerInstallValue.
  void Set(const base::Value& value);
  std::optional<base::Value> Get();
  void Delete();

 private:
  // HKLM for branded system installs, else HKCU.
  const HKEY root_;

  // `Google\Update\ClientState{Medium}\{ChromeAppId}\PerInstallValue` for
  // branded installs.
  const std::wstring key_path_;

  // The registry value for the PerInstallValue, same as `name` in the
  // constructor.
  const std::wstring value_name_;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_PER_INSTALL_VALUES_H_
