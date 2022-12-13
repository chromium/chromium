// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/posix/app_server_posix.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/updater/app/server/posix/update_service_internal_stub.h"

namespace updater {

AppServerPosix::AppServerPosix() = default;
AppServerPosix::~AppServerPosix() = default;

void AppServerPosix::TaskStarted() {
  main_task_runner_->PostTask(FROM_HERE,
                              BindOnce(&AppServerPosix::MarkTaskStarted, this));
}

void AppServerPosix::MarkTaskStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++tasks_running_;
  VLOG(2) << "Starting task, " << tasks_running_ << " tasks running";
}

void AppServerPosix::TaskCompleted() {
  main_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AppServerPosix::AcknowledgeTaskCompletion, this),
      external_constants()->ServerKeepAliveTime());
}

void AppServerPosix::AcknowledgeTaskCompletion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (--tasks_running_ < 1) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AppServerPosix::Shutdown, this, 0));
  }
  VLOG(2) << "Completing task, " << tasks_running_ << " tasks running";
}

}  // namespace updater
