// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/version.h"
#include "chrome/browser/updater/check_updater_health_task.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

CheckUpdaterHealthTask::CheckUpdaterHealthTask(UpdaterScope scope)
    : scope_(scope) {}
CheckUpdaterHealthTask::~CheckUpdaterHealthTask() = default;

void CheckUpdaterHealthTask::CheckAndRecordUpdaterHealth(
    const base::Version& version) {}

void CheckUpdaterHealthTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramBoolean("GoogleUpdate.UpdaterHealth.UpdaterValid", false);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback)));
}

}  // namespace updater
