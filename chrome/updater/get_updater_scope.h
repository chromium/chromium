// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_GET_UPDATER_SCOPE_H_
#define CHROME_UPDATER_GET_UPDATER_SCOPE_H_

#include "base/command_line.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

// Returns `true` if the tag has a "needsadmin=prefers" argument.
bool IsPrefersForCommandLine(const base::CommandLine& command_line);

// Returns the scope of the updater, which is either per-system or per-user.
// The updater scope is determined from the `command_line` argument.
UpdaterScope GetUpdaterScopeForCommandLine(
    const base::CommandLine& command_line);

// Returns the scope of the updater, which is either per-system or per-user.
// The updater scope is determined from command line arguments of the process,
// the presence and content of the tag, and the integrity level of the process,
// where applicable.
UpdaterScope GetUpdaterScope();

bool IsSystemInstall();

}  // namespace updater

#endif  // CHROME_UPDATER_GET_UPDATER_SCOPE_H_
