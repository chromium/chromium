// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_LACROS_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_LACROS_H_

#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/policy/core/common/async_policy_loader.h"

namespace policy {

// A policy loader for Lacros. The data is taken from Ash and the validatity of
// data is trusted, since they have been validated by Ash.
class POLICY_EXPORT PolicyLoaderLacros : public AsyncPolicyLoader {
 public:
  // Creates the policy loader, saving the task_runner internally. Later
  // task_runner is used to have in sequence the process of policy parsing and
  // validation.
  explicit PolicyLoaderLacros(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  // Not copyable or movable
  PolicyLoaderLacros(const PolicyLoaderLacros&) = delete;
  PolicyLoaderLacros& operator=(const PolicyLoaderLacros&) = delete;
  ~PolicyLoaderLacros() override;

  // AsyncPolicyLoader implementation.
  // Verifies that it runs on correct thread.
  void InitOnBackgroundThread() override;
  // Loads the policy data from LacrosInitParams and populates it in the bundle
  // that is returned.
  std::unique_ptr<PolicyBundle> Load() override;
  // Returns the last time the policy successfully loaded.
  base::Time LastModificationTime() override;

 private:
  // Task runner for running background jobs.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The time of last modification.
  base::Time last_modification_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_LACROS_H_
