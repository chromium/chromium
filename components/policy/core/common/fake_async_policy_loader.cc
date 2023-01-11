// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/fake_async_policy_loader.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"

namespace policy {

FakeAsyncPolicyLoader::FakeAsyncPolicyLoader(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : AsyncPolicyLoader(task_runner, /*periodic_updates=*/true) {}

PolicyBundle FakeAsyncPolicyLoader::Load() {
  return policy_bundle_.Clone();
}

void FakeAsyncPolicyLoader::InitOnBackgroundThread() {
  // Nothing to do.
}

void FakeAsyncPolicyLoader::SetPolicies(const PolicyBundle& policy_bundle) {
  policy_bundle_ = policy_bundle.Clone();
}

void FakeAsyncPolicyLoader::PostReloadOnBackgroundThread(bool force) {
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&AsyncPolicyLoader::Reload,
                                         base::Unretained(this), force));
}

}  // namespace policy
