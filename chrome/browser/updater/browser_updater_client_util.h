// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_UTIL_H_
#define CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_UTIL_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/updater/updater_scope.h"

extern const char kUpdaterName[];
extern const char kPrivilegedHelperName[];

// Get the current installed version of the browser.
std::string CurrentlyInstalledVersion();

// System level updater should only be used if the browser is owned by root.
// During promotion, the browser will be changed to be owned by root and wheel.
// A browser must go through promotion before it can utilize the system-level
// updater.
updater::UpdaterScope GetUpdaterScope();

// If this build should integrate with an updater, makes sure that an updater
// is installed and that the browser is registered with it for updates. Must be
// called on a sequenced task runner. In cases where user intervention is
// necessary, calls `prompt` (on the same sequence).  After the updater is made
// present (or cannot be made present), calls `complete` on the same sequence.
void EnsureUpdater(base::OnceClosure prompt, base::OnceClosure complete);

// Prompts the user for credentials and sets up a system-level updater.
void SetupSystemUpdater();

#endif  // CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_UTIL_H_
