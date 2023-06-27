// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/cleanup_task.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/util/util.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/updater/util/win_util.h"
#endif

namespace updater {

CleanupTask::CleanupTask(UpdaterScope scope) : scope_(scope) {}

CleanupTask::~CleanupTask() = default;

void CleanupTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CleanupTask::RunCleanup, this), std::move(callback));
}

void CleanupTask::RunCleanup() {
  std::ignore = RunCleanupObsoleteFiles();
}

#if BUILDFLAG(IS_WIN)

bool CleanupTask::RunCleanupObsoleteFiles() {
  // Delete anything other than `GoogleUpdate.exe` under `\Google\Update`.
  return DeleteExcept(GetGoogleUpdateExePath(scope_));
}

#else  // BUILDFLAG(IS_WIN)

bool CleanupTask::RunCleanupObsoleteFiles() {
  return false;
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace updater
