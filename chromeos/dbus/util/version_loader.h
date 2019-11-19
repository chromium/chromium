// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_UTIL_VERSION_LOADER_H_
#define CHROMEOS_DBUS_UTIL_VERSION_LOADER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/component_export.h"

namespace chromeos {
namespace version_loader {

enum VersionFormat {
  VERSION_SHORT,
  VERSION_SHORT_WITH_DATE,
  VERSION_FULL,
};

// Gets the version.
// If |full_version| is true version string with extra info is extracted,
// otherwise it's in short format x.x.xx.x.
// May block.
COMPONENT_EXPORT(CHROMEOS_DBUS) std::string GetVersion(VersionFormat format);

// Gets the ARC version.
// May block.
COMPONENT_EXPORT(CHROMEOS_DBUS) std::string GetARCVersion();

// Gets the firmware info.
// May block.
COMPONENT_EXPORT(CHROMEOS_DBUS) std::string GetFirmware();

// Extracts the firmware from the file.
COMPONENT_EXPORT(CHROMEOS_DBUS)
std::string ParseFirmware(const std::string& contents);

// Returns true if |new_version| is older than |current_version|.
// Version numbers should be dot separated. The sections are compared as
// numbers if possible, as strings otherwise. Earlier sections have
// precedence. If one version is prefix of another, the shorter one is
// considered older. (See test for examples.)
COMPONENT_EXPORT(CHROMEOS_DBUS)
bool IsRollback(const std::string& current_version,
                const std::string& new_version);

}  // namespace version_loader
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_UTIL_VERSION_LOADER_H_
