// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/scheduler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/browser/updater/check_updater_health_task.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

void DoPeriodicTasks(base::OnceClosure callback) {
  base::MakeRefCounted<CheckUpdaterHealthTask>(GetBrowserUpdaterScope())
      ->Run(base::BindOnce(
          [](base::OnceClosure callback) {
            base::ThreadPool::PostTaskAndReply(
                FROM_HERE,
                {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                 base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
                base::BindOnce(&WakeAllUpdaters), std::move(callback));
          },
          std::move(callback)));
}

}  // namespace updater
