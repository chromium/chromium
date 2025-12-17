// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/scheduler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/updater/updater.h"

namespace updater {

void DoPeriodicTasks(base::RepeatingClosure prompt,
                     base::OnceClosure callback) {
  EnsureUpdater(
      base::TaskPriority::BEST_EFFORT, prompt,
      base::BindOnce(
          [](base::OnceClosure callback) {
            // Some users disable the background launchd task that runs the
            // updater periodic tasks. To continue to deliver updates to those
            // users, run the periodic tasks now.
            base::ThreadPool::PostTask(
                FROM_HERE,
                {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                 base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
                base::BindOnce(&WakeAllUpdaters, std::move(callback)));
          },
          base::BindPostTaskToCurrentDefault(std::move(callback))));
}

}  // namespace updater
