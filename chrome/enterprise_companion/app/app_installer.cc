// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/enterprise_companion/app/app.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/installer.h"
#include "chrome/enterprise_companion/lock.h"

namespace enterprise_companion {

namespace {

constexpr base::TimeDelta kAcquireLockTimeout = base::Seconds(5);

class AppInstaller : public App {
 public:
  AppInstaller(
      base::OnceCallback<EnterpriseCompanionStatus()> shutdown_remote_task,
      base::OnceCallback<std::unique_ptr<ScopedLock>(base::TimeDelta timeout)>
          lock_provider,
      base::OnceCallback<bool()> install_task)
      : shutdown_remote_task_(std::move(shutdown_remote_task)),
        lock_provider_(std::move(lock_provider)),
        install_task_(std::move(install_task)) {}
  ~AppInstaller() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 private:
  void FirstTaskRun() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    EnterpriseCompanionStatus shutdown_status =
        std::move(shutdown_remote_task_).Run();
    VLOG_IF(1, !shutdown_status.ok())
        << "Could not shutdown the active Chrome Enterprise Companion App. One "
           "may not be running.";

    lock_ = std::move(lock_provider_).Run(kAcquireLockTimeout);
    if (!lock_) {
      Shutdown(EnterpriseCompanionStatus(ApplicationError::kCannotAcquireLock));
      return;
    }

    if (!std::move(install_task_).Run()) {
      Shutdown(
          EnterpriseCompanionStatus(ApplicationError::kInstallationFailed));
      return;
    }
    VLOG(1) << "Installation/Uninstallation completed successfully.";
    Shutdown(EnterpriseCompanionStatus::Success());
  }

  SEQUENCE_CHECKER(sequence_checker_);
  base::OnceCallback<EnterpriseCompanionStatus()> shutdown_remote_task_;
  base::OnceCallback<std::unique_ptr<ScopedLock>(base::TimeDelta timeout)>
      lock_provider_;
  base::OnceCallback<bool()> install_task_;
  std::unique_ptr<ScopedLock> lock_;
};

}  // namespace

std::unique_ptr<App> CreateAppInstaller(
    base::OnceCallback<EnterpriseCompanionStatus()> shutdown_remote_task,
    base::OnceCallback<std::unique_ptr<ScopedLock>(base::TimeDelta timeout)>
        lock_provider,
    base::OnceCallback<bool()> task) {
  return std::make_unique<AppInstaller>(std::move(shutdown_remote_task),
                                        std::move(lock_provider),
                                        std::move(task));
}

std::unique_ptr<App> CreateAppInstall() {
  return std::make_unique<AppInstaller>(
      base::BindOnce([] { return CreateAppShutdown()->Run(); }),
      base::BindOnce(&CreateScopedLock), base::BindOnce(&Install));
}

std::unique_ptr<App> CreateAppUninstall() {
  return std::make_unique<AppInstaller>(
      base::BindOnce([] { return CreateAppShutdown()->Run(); }),
      base::BindOnce(&CreateScopedLock), base::BindOnce(&Uninstall));
}

}  // namespace enterprise_companion
