// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_ASYNC_POLICY_PROVIDER_H_
#define COMPONENTS_POLICY_CORE_COMMON_ASYNC_POLICY_PROVIDER_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/policy_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace policy {

class AsyncPolicyLoader;
class PolicyBundle;
class SchemaRegistry;

// A policy provider that loads its policies asynchronously on a background
// thread. Platform-specific providers are created by passing an implementation
// of AsyncPolicyLoader to a new AsyncPolicyProvider.
class POLICY_EXPORT AsyncPolicyProvider : public ConfigurationPolicyProvider {
 public:
  // The AsyncPolicyProvider does a synchronous load in its constructor, and
  // therefore it needs the |registry| at construction time. The same |registry|
  // should be passed later to Init().
  AsyncPolicyProvider(SchemaRegistry* registry,
                      std::unique_ptr<AsyncPolicyLoader> loader);
  ~AsyncPolicyProvider() override;

  // ConfigurationPolicyProvider implementation.
  void Init(SchemaRegistry* registry) override;
  void Shutdown() override;
  void RefreshPolicies() override;

 private:
  // Helper for RefreshPolicies().
  void ReloadAfterRefreshSync();

  // Invoked with the latest bundle loaded by the |loader_|.
  void OnLoaderReloaded(std::unique_ptr<PolicyBundle> bundle);

  // Callback passed to the loader that it uses to pass back the current policy
  // bundle to the provider. This is invoked on the background thread and
  // forwards to OnLoaderReloaded() on the runner that owns the provider,
  // if |weak_this| is still valid.
  static void LoaderUpdateCallback(
      scoped_refptr<base::SingleThreadTaskRunner> runner,
      base::WeakPtr<AsyncPolicyProvider> weak_this,
      std::unique_ptr<PolicyBundle> bundle);

  // The |loader_| that does the platform-specific policy loading. It lives
  // on the background thread but is owned by |this|.
  std::unique_ptr<AsyncPolicyLoader> loader_;

  // Callback used to synchronize RefreshPolicies() calls with the background
  // thread. See the implementation for the details.
  base::CancelableClosure refresh_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get a WeakPtr to |this| for the update callback given to the
  // loader.
  base::WeakPtrFactory<AsyncPolicyProvider> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AsyncPolicyProvider);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_ASYNC_POLICY_PROVIDER_H_
