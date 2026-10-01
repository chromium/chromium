// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/scheduler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "chrome/browser/updater/updater.h"

namespace updater {

void DoPeriodicTasks(base::RepeatingClosure prompt,
                     base::OnceClosure callback) {
  // Ensure the updater is present, then check its health and wake it, in case
  // users disabled the background launchd task that runs its periodic tasks.
  EnsureUpdater(base::TaskPriority::BEST_EFFORT, prompt,
                base::BindOnce(&CheckUpdaterHealthAndWakeAllUpdaters,
                               std::move(callback)));
}

}  // namespace updater
