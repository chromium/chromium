// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_uninstall_self.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/lock.h"
#include "chrome/updater/util/util.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/updater/win/setup/uninstall.h"
#elif BUILDFLAG(IS_POSIX)
#include "chrome/updater/posix/setup.h"
#endif

namespace updater {

// AppUninstallSelf uninstalls this instance of the updater.
class AppUninstallSelf : public App {
 public:
  AppUninstallSelf() = default;

 private:
  ~AppUninstallSelf() override = default;
  [[nodiscard]] int Initialize() override;
  void FirstTaskRun() override;

  void UninstallAll();

  // Inter-process lock taken by AppInstall, AppUninstall, and AppUpdate.
  std::unique_ptr<ScopedLock> setup_lock_;
};

int AppUninstallSelf::Initialize() {
  setup_lock_ =
      CreateScopedLock(kSetupMutex, updater_scope(), kWaitForSetupLock);
  return kErrorOk;
}

void AppUninstallSelf::FirstTaskRun() {
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

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&UninstallCandidate, updater_scope()),
      base::BindOnce(&AppUninstallSelf::Shutdown, this));
}

scoped_refptr<App> MakeAppUninstallSelf() {
  return base::MakeRefCounted<AppUninstallSelf>();
}

}  // namespace updater
