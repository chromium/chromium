// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_APP_UNINSTALL_H_
#define CHROME_UPDATER_APP_APP_UNINSTALL_H_

#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

class App;

// Returns a vector of updater executable paths from all versions underneath the
// official install directory for `scope`, excluding the current version.
std::vector<base::FilePath> GetVersionExecutablePaths(UpdaterScope scope);

// Returns a command line to uninstall the updater at `executable_path`.
base::CommandLine GetUninstallSelfCommandLine(
    UpdaterScope scope,
    const base::FilePath& executable_path);

scoped_refptr<App> MakeAppUninstall();

}  // namespace updater

#endif  // CHROME_UPDATER_APP_APP_UNINSTALL_H_
