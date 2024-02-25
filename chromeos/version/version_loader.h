// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_VERSION_VERSION_LOADER_H_
#define CHROMEOS_VERSION_VERSION_LOADER_H_

#include <optional>
#include <string>

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
// If not found returns std::nullopt.
// May block.
COMPONENT_EXPORT(CHROMEOS_VERSION)
std::optional<std::string> GetVersion(VersionFormat format);

// Gets the ARC version.
// May block.
COMPONENT_EXPORT(CHROMEOS_VERSION) std::string GetArcVersion();

// Gets the ARC Android SDK version.
// If not found returns std::nullopt.
// May block.
COMPONENT_EXPORT(CHROMEOS_VERSION)
std::optional<std::string> GetArcAndroidSdkVersion();

// Gets the firmware info.
// May block.
COMPONENT_EXPORT(CHROMEOS_VERSION) std::string GetFirmware();

// Extracts the firmware from the file.
COMPONENT_EXPORT(CHROMEOS_VERSION)
std::string ParseFirmware(const std::string& contents);

// Returns true if |new_version| is older than |current_version|.
// Version numbers should be dot separated. The sections are compared as
// numbers if possible, as strings otherwise. Earlier sections have
// precedence. If one version is prefix of another, the shorter one is
// considered older. (See test for examples.)
COMPONENT_EXPORT(CHROMEOS_VERSION)
bool IsRollback(const std::string& current_version,
                const std::string& new_version);

}  // namespace version_loader
}  // namespace chromeos

#endif  // CHROMEOS_VERSION_VERSION_LOADER_H_
