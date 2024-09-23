// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_CRASH_CLIENT_H_
#define CHROME_ENTERPRISE_COMPANION_CRASH_CLIENT_H_

#include <optional>

#include "base/files/file_path.h"

namespace enterprise_companion {

// Returns the default crashpad database file path, or nullopt if it could not
// be resolved. The database is placed within the install directory.
std::optional<base::FilePath> GetDefaultCrashDatabasePath();

// Initializes collection and upload of crash reports.
bool InitializeCrashReporting(
    std::optional<base::FilePath> crash_database_path =
        GetDefaultCrashDatabasePath());

// Runs the crash reporter message loop within the current process. On return,
// the current process should exit.
int CrashReporterMain();

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_CRASH_CLIENT_H_
