// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_BEACONS_H_
#define CHROME_INSTALLER_UTIL_BEACONS_H_

#include <windows.h>

#include <memory>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "chrome/installer/util/shell_util.h"

namespace base {
class FilePath;
}

// Checks the default state of the browser for the current user and updates the
// appropriate beacon (last was default or first not default).
void UpdateDefaultBrowserBeaconForPath(const base::FilePath& chrome_exe);

// Updates the last was default or first not default beacon for the current user
// based on |default_state|.
void UpdateDefaultBrowserBeaconWithState(ShellUtil::DefaultState default_state);

// Updates the last OS upgrade beacon for the install.
void UpdateOsUpgradeBeacon();

namespace installer_util {

class Beacon;

// Returns a Beacon representing the last time the machine's OS was ugpraded.
std::unique_ptr<Beacon> MakeLastOsUpgradeBeacon();

// Returns a Beacon representing the last time Chrome was the user's default
// browser.
std::unique_ptr<Beacon> MakeLastWasDefaultBeacon();

// Returns a Beacon representing the first time Chrome was not the user's
// default browser.
std::unique_ptr<Beacon> MakeFirstNotDefaultBeacon();

// A named beacon stored in the registry representing the first or last time at
// which some event took place. A beacon may apply to a per-user event or a
// per-install event. In general, beacons should be created via factory methods
// such as those above.
class Beacon {
 public:
  enum class BeaconType {
    // A beacon that marks the first occurrence of an event.
    FIRST,
    // A beacon that marks the last occurrence of an event.
    LAST,
  };

  enum class BeaconScope {
    // A beacon that applies to a per-user event.
    PER_USER,
    // A beacon that applies to a per-install event.
    PER_INSTALL,
  };

  Beacon(std::wstring_view name, BeaconType type, BeaconScope scope);

  Beacon(const Beacon&) = delete;
  Beacon& operator=(const Beacon&) = delete;

  ~Beacon();

  // Updates the beacon. For a type LAST beacon, the current time is written
  // unconditionally. For a type FIRST beacon, the beacon is only updated if it
  // does not already exist.
  void Update();

  // Removes the beacon.
  void Remove();

  // Returns the beacon's value or a null time if not found.
  base::Time Get();

 private:
  // Initializes the key_path_ and value_name_ fields of the beacon.
  void Initialize(std::wstring_view name);

  // The type of beacon.
  const BeaconType type_;

  // The root key in the registry where this beacon is stored.
  const HKEY root_;

  // The scope of the beacon.
  const BeaconScope scope_;

  // The path to the registry key holding the beacon.
  std::wstring key_path_;

  // The name of the registry value holding the beacon.
  std::wstring value_name_;
};

}  // namespace installer_util

#endif  // CHROME_INSTALLER_UTIL_BEACONS_H_
