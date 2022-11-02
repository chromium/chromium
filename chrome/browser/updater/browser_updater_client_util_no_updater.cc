// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client_util.h"

#include <string>

#include "base/callback.h"
#include "chrome/updater/updater_scope.h"

std::string CurrentlyInstalledVersion() {
  return {};
}

updater::UpdaterScope GetUpdaterScope() {
  return updater::UpdaterScope::kUser;
}

void EnsureUpdater(base::OnceClosure prompt, base::OnceClosure complete) {
  std::move(complete).Run();
}

void SetupSystemUpdater() {}
