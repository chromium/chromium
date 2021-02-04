// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_uninstall.h"

#include <memory>
#include <string>
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

  std::unique_ptr<GlobalPrefs> global_prefs_;
};

void AppUninstall::Initialize() {
  global_prefs_ = CreateGlobalPrefs();
}

void AppUninstall::FirstTaskRun() {
  if (!global_prefs_) {
    return;
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

#if defined(OS_MAC)
  // TODO(crbug.com/1114719): Implement --uninstall-self for Win.
  if (command_line->HasSwitch(kUninstallSelfSwitch)) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()}, base::BindOnce(&UninstallCandidate),
        base::BindOnce(&AppUninstall::Shutdown, this));
    return;
  }
#endif

  const bool has_uninstall_switch = command_line->HasSwitch(kUninstallSwitch);
  const bool has_uninstall_if_unused_switch =
      command_line->HasSwitch(kUninstallIfUnusedSwitch);

  const std::vector<std::string> app_ids =
      base::MakeRefCounted<PersistedData>(global_prefs_->GetPrefService())
          ->GetAppIds();

  const bool can_uninstall =
      has_uninstall_switch ||
      (has_uninstall_if_unused_switch && app_ids.size() == 1 &&
       base::Contains(app_ids, kUpdaterAppId));

  if (can_uninstall) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()}, base::BindOnce(&Uninstall, false),
        base::BindOnce(&AppUninstall::Shutdown, this));
    return;
  }
}

scoped_refptr<App> MakeAppUninstall() {
  return base::MakeRefCounted<AppUninstall>();
}

}  // namespace updater
