// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/setup.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/posix/setup.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/launchd_util.h"

namespace updater {

void InstallCandidate(UpdaterScope scope,
                      base::OnceCallback<void(int)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(FROM_HERE, {base::MayBlock()},
                                               base::BindOnce(&Setup, scope),
                                               std::move(callback));
}

}  // namespace updater
