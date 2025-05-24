// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/setup.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

namespace {

// If the candidate installation was successful, copy experiment data present in
// the command line to the local prefs.
void OnPlatformCandidateInstalled(base::OnceCallback<void(int)> callback,
                                  int install_result) {
  if (install_result != 0) {
    std::move(callback).Run(install_result);
    return;
  }

  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTaskAndReply(
          FROM_HERE, base::BindOnce([] {
            if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                    kEnableCecaExperimentSwitch)) {
              CreateLocalPrefs(GetUpdaterScope())
                  ->SetCecaExperimentEnabled(true);
            }
          }),
          base::BindOnce(std::move(callback), install_result));
}

}  // namespace

void InstallCandidate(UpdaterScope scope,
                      base::OnceCallback<void(int)> callback) {
  InstallPlatformCandidate(scope, base::BindOnce(&OnPlatformCandidateInstalled,
                                                 std::move(callback)));
}

}  // namespace updater
