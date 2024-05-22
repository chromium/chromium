// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_update.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/lock.h"
#include "chrome/updater/setup.h"
#include "chrome/updater/util/util.h"

namespace updater {

class AppUpdate : public App {
 private:
  ~AppUpdate() override = default;
  [[nodiscard]] int Initialize() override;
  void FirstTaskRun() override;

  void SetupDone(int result);

  // Inter-process lock taken by AppInstall, AppUninstall, and AppUpdate.
  std::unique_ptr<ScopedLock> setup_lock_;
};

int AppUpdate::Initialize() {
  setup_lock_ =
      CreateScopedLock(kSetupMutex, updater_scope(), kWaitForSetupLock);
  return kErrorOk;
}

void AppUpdate::FirstTaskRun() {
  if (WrongUser(updater_scope())) {
    VLOG(0) << "The current user is not compatible with the current scope.";
    Shutdown(kErrorWrongUser);
    return;
  }

  if (!setup_lock_) {
    VLOG(0) << "Failed to acquire setup mutex; shutting down.";
    Shutdown(kErrorFailedToLockSetupMutex);
    return;
  }

  InstallCandidate(updater_scope(),
                   base::BindOnce(&AppUpdate::SetupDone, this));
}

void AppUpdate::SetupDone(int result) {
  Shutdown(result);
}

scoped_refptr<App> MakeAppUpdate() {
  return base::MakeRefCounted<AppUpdate>();
}

}  // namespace updater
