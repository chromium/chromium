// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_FAKE_ASYNC_POLICY_LOADER_H_
#define COMPONENTS_POLICY_CORE_COMMON_FAKE_ASYNC_POLICY_LOADER_H_

#include "base/memory/ref_counted.h"
#include "components/policy/core/common/async_policy_loader.h"
#include "components/policy/core/common/policy_bundle.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace policy {

// Fake AsyncPolicyLoader for testing with test-controlled policies.
//
// Typical test code would populate the policy contents via calls to
// ClearPolicies and AddPolicies and then notify the rest of the policy
// subsystem of the changes by calling PostReloadOnBackgroundThread.
class FakeAsyncPolicyLoader : public AsyncPolicyLoader {
 public:
  explicit FakeAsyncPolicyLoader(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  FakeAsyncPolicyLoader(const FakeAsyncPolicyLoader&) = delete;
  FakeAsyncPolicyLoader& operator=(const FakeAsyncPolicyLoader&) = delete;

  // Implementation of virtual methods from AsyncPolicyLoader base class.
  PolicyBundle Load() override;
  void InitOnBackgroundThread() override;

  // Provides content for the simulated / faked policies.
  void SetPolicies(const PolicyBundle& policy_bundle);

  // Notifies the rest of the policy subsystem that policy contents have
  // changed.  This simulates / fakes a notification that normally would be
  // triggered by a FilePathWatcher or (registry)ObjectWatcher in a real loader.
  //
  // See AsyncPolicyLoader::Reload method for description of the |force|
  // parameter.
  void PostReloadOnBackgroundThread(bool force);

 private:
  PolicyBundle policy_bundle_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_FAKE_ASYNC_POLICY_LOADER_H_
