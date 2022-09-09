// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_SETUP_UNINSTALL_H_
#define CHROME_UPDATER_WIN_SETUP_UNINSTALL_H_

#include "chrome/updater/updater_scope.h"

namespace updater {

int Uninstall(UpdaterScope scope);

int UninstallCandidate(UpdaterScope scope);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_SETUP_UNINSTALL_H_
