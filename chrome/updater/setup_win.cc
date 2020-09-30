// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/setup.h"
#include "chrome/updater/win/setup/setup.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace updater {

void InstallCandidate(bool is_machine,
                      base::OnceCallback<void(int)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&Setup, is_machine),
      std::move(callback));
}

}  // namespace updater
