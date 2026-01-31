// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client_util.h"

#include "chrome/browser/updater/updater.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/update_did_run_state.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

UpdaterScope GetBrowserUpdaterScope() {
  return InstallUtil::IsPerUserInstall() ? UpdaterScope::kUser
                                         : UpdaterScope::kSystem;
}

// Marks the browser as active.
void SetActive() {
  installer::UpdateDidRunState();
}

}  // namespace updater
