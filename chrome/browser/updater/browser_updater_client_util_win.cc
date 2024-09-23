// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client_util.h"

#include "chrome/installer/util/install_util.h"
#include "chrome/updater/updater_scope.h"

updater::UpdaterScope GetUpdaterScope() {
  return InstallUtil::IsPerUserInstall() ? updater::UpdaterScope::kUser
                                         : updater::UpdaterScope::kSystem;
}
