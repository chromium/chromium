// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/scheduler.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace updater {

namespace {

void RunAndReschedule(base::RepeatingClosure prompt) {
  DoPeriodicTasks(
      prompt,
      base::BindOnce(
          [](base::RepeatingClosure prompt) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
                FROM_HERE, base::BindOnce(&RunAndReschedule, prompt),
                base::Hours(5));
          },
          prompt));
}

}  // namespace

void SchedulePeriodicTasks(base::RepeatingClosure prompt) {
  // Delay a little bit to get out of the way of browser startup.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&RunAndReschedule, prompt), base::Seconds(19));
}

}  // namespace updater
