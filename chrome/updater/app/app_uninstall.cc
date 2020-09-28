// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_uninstall.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/constants.h"

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
  void FirstTaskRun() override;
};

void AppUninstall::FirstTaskRun() {
#if defined(OS_MAC)
  // TODO(crbug.com/1114719): Implement --uninstall=self for Win.
  const std::string uninstall_switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kUninstallSwitch);
  if (!uninstall_switch_value.empty()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()}, base::BindOnce(&UninstallCandidate),
        base::BindOnce(&AppUninstall::Shutdown, this));
  } else
#endif
  {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()}, base::BindOnce(&Uninstall, false),
        base::BindOnce(&AppUninstall::Shutdown, this));
  }
}

scoped_refptr<App> MakeAppUninstall() {
  return base::MakeRefCounted<AppUninstall>();
}

}  // namespace updater
