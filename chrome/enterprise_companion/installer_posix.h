// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_INSTALLER_POSIX_H_
#define CHROME_ENTERPRISE_COMPANION_INSTALLER_POSIX_H_

#include "base/files/file_path.h"

namespace enterprise_companion {

// Install the application to `dir`, configuring its permissions and contents.
// Returns false on error.
bool InstallToDir(const base::FilePath& install_directory);

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_INSTALLER_POSIX_H_
