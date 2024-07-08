// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_INSTALLER_H_
#define CHROME_ENTERPRISE_COMPANION_INSTALLER_H_

#include <optional>

#include "base/files/file_path.h"

namespace enterprise_companion {

// The name of the program image. E.g. "enterprise_companion.exe".
extern const char kExecutableName[];

// Returns the base install directory.
std::optional<base::FilePath> GetInstallDirectory();

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_INSTALLER_H_
