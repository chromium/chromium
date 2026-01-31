// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client_util.h"

#include "chrome/updater/updater_scope.h"

namespace updater {

UpdaterScope GetBrowserUpdaterScope() {
  // There does not exist a mechanism to communicate across the root/user
  // boundary for the Chromium Updater on Linux.
  return UpdaterScope::kUser;
}

// Does nothing.
void SetActive() {}

}  // namespace updater
