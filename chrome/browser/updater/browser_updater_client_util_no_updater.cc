// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/updater/updater_scope.h"
#include "components/version_info/version_info.h"

std::string CurrentlyInstalledVersion() {
  return std::string(version_info::GetVersionNumber());
}

updater::UpdaterScope GetUpdaterScope() {
  return updater::UpdaterScope::kUser;
}

void EnsureUpdater(base::OnceClosure prompt, base::OnceClosure complete) {
  std::move(complete).Run();
}

void SetupSystemUpdater() {}
