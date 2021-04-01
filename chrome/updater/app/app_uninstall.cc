// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_uninstall.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"

#if defined(OS_WIN)
#include "chrome/updater/win/setup/uninstall.h"
#endif

#if defined(OS_MAC)
#include "chrome/updater/mac/setup/setup.h"
#endif

namespace updater {

// AppUninstall uninstalls the updater.
class AppUninstall : public App {
 public:
  AppUninstall() = default;

 private:
  ~AppUninstall() override = default;
  void Initialize() override;
  void FirstTaskRun() override;

  // Conditionally set, if prefs must be acquired for some uninstall scenarios.
  // Creating the prefs instance may result in deadlocks. Therefore, the prefs
  // lock can't be taken in all cases.
  std::unique_ptr<GlobalPrefs> global_prefs_;
};

void AppUninstall::Initialize() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kUninstallIfUnusedSwitch))
    global_prefs_ = CreateGlobalPrefs();
}

void AppUninstall::FirstTaskRun() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(kUninstallSwitch)) {
    CHECK(!global_prefs_);
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&Uninstall, updater_scope()),
        base::BindOnce(&AppUninstall::Shutdown, this));
    return;
  }

#if defined(OS_MAC)
  // TODO(crbug.com/1114719): Implement --uninstall-self for Win.
  if (command_line->HasSwitch(kUninstallSelfSwitch)) {
    CHECK(!global_prefs_);
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&UninstallCandidate, updater_scope()),
        base::BindOnce(&AppUninstall::Shutdown, this));
    return;
  }
#endif

  if (command_line->HasSwitch(kUninstallIfUnusedSwitch)) {
    CHECK(global_prefs_);
    const std::vector<std::string> registered_apps =
        base::MakeRefCounted<PersistedData>(global_prefs_->GetPrefService())
            ->GetAppIds();
    if (registered_apps.size() == 1 &&
        base::Contains(registered_apps, kUpdaterAppId)) {
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock()},
          base::BindOnce(&Uninstall, updater_scope()),
          base::BindOnce(
              [](base::OnceCallback<void(int)> shutdown, int exit_code) {
                // global_prefs is captured so that this process holds the prefs
                // lock through uninstallation.
                std::move(shutdown).Run(exit_code);
              },
              base::BindOnce(&AppUninstall::Shutdown, this)));
    }
  }
}

scoped_refptr<App> MakeAppUninstall() {
  return base::MakeRefCounted<AppUninstall>();
}

}  // namespace updater
