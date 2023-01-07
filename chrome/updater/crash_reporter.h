// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CRASH_REPORTER_H_
#define CHROME_UPDATER_CRASH_REPORTER_H_

#include <string>

namespace updater {

enum class UpdaterScope;

// Starts a new instance of this executable running as the crash reporter
// process.
void StartCrashReporter(UpdaterScope updater_scope, const std::string& version);

// Runs the crash reporter message loop within the current process. On return,
// the current process should exit.
int CrashReporterMain();

}  // namespace updater

#endif  // CHROME_UPDATER_CRASH_REPORTER_H_
