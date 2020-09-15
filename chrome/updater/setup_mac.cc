// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/setup/setup.h"
#include "chrome/updater/setup.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/task_runner.h"

namespace updater {

void InstallCandidate(bool is_machine,
                      scoped_refptr<base::TaskRunner> runner,
                      base::OnceCallback<void(int)> callback) {
  runner->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), InstallCandidate()),
      base::TimeDelta::FromSeconds(3));
}

}  // namespace updater
