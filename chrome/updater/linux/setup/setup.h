// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_LINUX_SETUP_SETUP_H_
#define CHROME_UPDATER_LINUX_SETUP_SETUP_H_

#include "base/notreached.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

int Uninstall(UpdaterScope scope) {
  NOTIMPLEMENTED();
  return -1;
}

// Uninstalls this version of the updater.
int UninstallCandidate(UpdaterScope scope) {
  NOTIMPLEMENTED();
  return -1;
}

}  // namespace updater

#endif  // CHROME_UPDATER_LINUX_SETUP_SETUP_H_
