// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/setup.h"

#include <AvailabilityMacros.h>
#import <Foundation/Foundation.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/mac/mac_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/posix/setup.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

namespace {

int MacOSVersion() {
  NSOperatingSystemVersion v = NSProcessInfo.processInfo.operatingSystemVersion;
  return v.majorVersion * 100 * 100 + v.minorVersion * 100 + v.patchVersion;
}

}  // namespace

void InstallCandidate(UpdaterScope scope,
                      base::OnceCallback<void(int)> callback) {
  if (MacOSVersion() < MAC_OS_X_VERSION_MIN_REQUIRED) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), kErrorUnsupportedOperatingSystem));
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(FROM_HERE, {base::MayBlock()},
                                               base::BindOnce(&Setup, scope),
                                               std::move(callback));
}

}  // namespace updater
