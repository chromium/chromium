// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/scheduler.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ui/cocoa/keystone_infobar_delegate.h"
#include "chrome/browser/updater/browser_updater_client.h"
#include "chrome/browser/updater/browser_updater_client_util.h"

namespace updater {

void DoPeriodicTasks(base::OnceClosure callback) {
  EnsureUpdater(
      base::TaskPriority::BEST_EFFORT,
      base::BindOnce(&ShowUpdaterPromotionInfoBar),
      base::BindOnce(
          [](base::OnceClosure callback) {
            // Run updater periodic tasks in case the launchd scheduled task is
            // blocked.
            base::ThreadPool::PostTaskAndReply(
                FROM_HERE,
                {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                 base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
                base::BindOnce(&WakeAllUpdaters), std::move(callback));
          },
          std::move(callback)));
}

}  // namespace updater
