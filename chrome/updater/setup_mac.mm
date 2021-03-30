// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/setup/setup.h"
#include "chrome/updater/setup.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/launchd_util.h"
#include "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

namespace {

void SetupDone(base::OnceCallback<void(int)> callback,
               UpdaterScope scope,
               int result) {
  if (result != setup_exit_codes::kSuccess) {
    std::move(callback).Run(result);
    return;
  }
  PollLaunchctlList(
      scope, kUpdateServiceInternalLaunchdName, LaunchctlPresence::kPresent,
      base::TimeDelta::FromSeconds(kWaitForLaunchctlUpdateSec),
      base::BindOnce(
          [](base::OnceCallback<void(int)> callback, bool service_exists) {
            std::move(callback).Run(
                service_exists
                    ? setup_exit_codes::kSuccess
                    : setup_exit_codes::
                          kFailedAwaitingLaunchdUpdateServiceInternalJob);
          },
          std::move(callback)));
}

}  // namespace

void InstallCandidate(UpdaterScope scope,
                      base::OnceCallback<void(int)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&Setup, scope),
      base::BindOnce(&SetupDone, std::move(callback), scope));
}

}  // namespace updater
