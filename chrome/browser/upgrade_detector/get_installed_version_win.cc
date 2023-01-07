// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/get_installed_version.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/installer/util/install_util.h"

namespace {

InstalledAndCriticalVersion GetInstalledVersionSynchronous() {
  base::Version installed_version =
      InstallUtil::GetChromeVersion(!InstallUtil::IsPerUserInstall());
  if (installed_version.IsValid()) {
    base::Version critical_version = InstallUtil::GetCriticalUpdateVersion();
    if (critical_version.IsValid()) {
      return InstalledAndCriticalVersion(std::move(installed_version),
                                         std::move(critical_version));
    }
  }
  return InstalledAndCriticalVersion(std::move(installed_version));
}

}  // namespace

void GetInstalledVersion(InstalledVersionCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::BindOnce(&GetInstalledVersionSynchronous), std::move(callback));
}
