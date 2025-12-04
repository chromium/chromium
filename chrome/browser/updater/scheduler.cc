// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/scheduler.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"

namespace updater {

namespace {

void RunAndReschedule() {
  DoPeriodicTasks(base::BindOnce([]() {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RunAndReschedule), base::Hours(5));
  }));
}

}  // namespace

void SchedulePeriodicTasks() {
  // Delay a little bit to get out of the way of browser startup.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&RunAndReschedule), base::Seconds(19));
}

}  // namespace updater
