// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATER_SCHEDULER_H_
#define CHROME_BROWSER_UPDATER_SCHEDULER_H_

#include "base/functional/callback_forward.h"

namespace updater {

// Schedule updater periodic tasks to run five minutes later and every five
// hours thereafter. This is a backup scheduler so that even if the updater's
// scheduler is broken or disabled, it will run tasks while Chrome is running.
void SchedulePeriodicTasks();

// Do the periodic tasks right away, invoking `callback` when done.
void DoPeriodicTasks(base::OnceClosure callback);

}  // namespace updater

#endif  // CHROME_BROWSER_UPDATER_SCHEDULER_H_
