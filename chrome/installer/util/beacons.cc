// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/beacons.h"

#include <stdint.h>

#include <string_view>
#include <tuple>

#include "base/notreached.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"

void UpdateDefaultBrowserBeaconForPath(const base::FilePath& chrome_exe) {
  // Getting Chrome's default state causes the beacon to be updated via a call
  // to UpdateDefaultBrowserBeaconWithState below.
  std::ignore = ShellUtil::GetChromeDefaultStateFromPath(chrome_exe);
}

void UpdateDefaultBrowserBeaconWithState(
    ShellUtil::DefaultState default_state) {
  switch (default_state) {
    case ShellUtil::UNKNOWN_DEFAULT:
      break;
    case ShellUtil::NOT_DEFAULT:
      installer_util::MakeFirstNotDefaultBeacon()->Update();
      break;
    case ShellUtil::IS_DEFAULT:
    case ShellUtil::OTHER_MODE_IS_DEFAULT:
      installer_util::MakeLastWasDefaultBeacon()->Update();
      installer_util::MakeFirstNotDefaultBeacon()->Remove();
      break;
  }
}

void UpdateOsUpgradeBeacon() {
  installer_util::MakeLastOsUpgradeBeacon()->Update();
}

namespace installer_util {

std::unique_ptr<Beacon> MakeLastOsUpgradeBeacon() {
  return std::make_unique<Beacon>(L"LastOsUpgrade", Beacon::BeaconType::LAST,
                                  Beacon::BeaconScope::PER_INSTALL);
}

std::unique_ptr<Beacon> MakeLastWasDefaultBeacon() {
  return std::make_unique<Beacon>(L"LastWasDefault", Beacon::BeaconType::LAST,
                                  Beacon::BeaconScope::PER_USER);
}

std::unique_ptr<Beacon> MakeFirstNotDefaultBeacon() {
  return std::make_unique<Beacon>(L"FirstNotDefault", Beacon::BeaconType::FIRST,
                                  Beacon::BeaconScope::PER_USER);
}

// Beacon ----------------------------------------------------------------------

Beacon::Beacon(std::wstring_view name, BeaconType type, BeaconScope scope)
    : type_(type),
      root_(install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                              : HKEY_CURRENT_USER),
      scope_(scope) {
  Initialize(name);
}

Beacon::~Beacon() {}

void Beacon::Update() {
  const REGSAM kAccess = KEY_WOW64_32KEY | KEY_QUERY_VALUE | KEY_SET_VALUE;
  base::win::RegKey key;

  // Nothing to update if the key couldn't be created. This should only be the
  // case for a developer build.
  if (key.Create(root_, key_path_.c_str(), kAccess) != ERROR_SUCCESS)
    return;

  // Nothing to do if this beacon is tracking the first occurrence of an event
  // that has already been recorded.
  if (type_ == BeaconType::FIRST && key.HasValue(value_name_.c_str()))
    return;

  int64_t now(base::Time::Now().ToInternalValue());
  key.WriteValue(value_name_.c_str(), &now, sizeof(now), REG_QWORD);
}

void Beacon::Remove() {
  const REGSAM kAccess = KEY_WOW64_32KEY | KEY_SET_VALUE;
  base::win::RegKey key;

  if (key.Open(root_, key_path_.c_str(), kAccess) == ERROR_SUCCESS)
    key.DeleteValue(value_name_.c_str());
}

base::Time Beacon::Get() {
  const REGSAM kAccess = KEY_WOW64_32KEY | KEY_QUERY_VALUE;
  base::win::RegKey key;
  int64_t now;

  if (key.Open(root_, key_path_.c_str(), kAccess) != ERROR_SUCCESS ||
      key.ReadInt64(value_name_.c_str(), &now) != ERROR_SUCCESS) {
    return base::Time();
  }

  return base::Time::FromInternalValue(now);
}

void Beacon::Initialize(std::wstring_view name) {
  const install_static::InstallDetails& install_details =
      install_static::InstallDetails::Get();

  // When possible, beacons are located in the app's ClientState key. Per-user
  // beacons for a per-machine install are located in a beacon-specific sub-key
  // of the app's ClientStateMedium key.
  if (scope_ == BeaconScope::PER_INSTALL ||
      !install_static::IsSystemInstall()) {
    key_path_ = install_details.GetClientStateKeyPath();
    value_name_.assign(name);
  } else {
    key_path_ = install_details.GetClientStateMediumKeyPath();
    key_path_.push_back(L'\\');
    key_path_.append(name);
    // This should never fail. If it does, the beacon will be written in the
    // key's default value, which is okay since the majority case is likely a
    // machine with a single user.
    if (!base::win::GetUserSidString(&value_name_))
      NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace installer_util
