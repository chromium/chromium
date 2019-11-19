// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/async_policy_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/policy/core/common/async_policy_loader.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/schema_registry.h"

namespace policy {

AsyncPolicyProvider::AsyncPolicyProvider(
    SchemaRegistry* registry,
    std::unique_ptr<AsyncPolicyLoader> loader)
    : loader_(std::move(loader)) {
  // Make an immediate synchronous load on startup.
  OnLoaderReloaded(loader_->InitialLoad(registry->schema_map()));
}

AsyncPolicyProvider::~AsyncPolicyProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AsyncPolicyProvider::Init(SchemaRegistry* registry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ConfigurationPolicyProvider::Init(registry);

  if (!loader_)
    return;

  AsyncPolicyLoader::UpdateCallback callback =
      base::Bind(&AsyncPolicyProvider::LoaderUpdateCallback,
                 base::ThreadTaskRunnerHandle::Get(),
                 weak_factory_.GetWeakPtr());
  bool post = loader_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&AsyncPolicyLoader::Init,
                                base::Unretained(loader_.get()), callback));
  DCHECK(post) << "AsyncPolicyProvider::Init() called with threads not running";
}

void AsyncPolicyProvider::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Note on the lifetime of |loader_|:
  // The |loader_| lives on the background thread, and is deleted from here.
  // This means that posting tasks on the |loader_| to the background thread
  // from the AsyncPolicyProvider is always safe, since a potential DeleteSoon()
  // is only posted from here. The |loader_| posts back to the
  // AsyncPolicyProvider through the |update_callback_|, which has a WeakPtr to
  // |this|.
  // If threads are spinning, delete the loader on the thread it lives on. If
  // there are no threads, kill it immediately.
  AsyncPolicyLoader* loader_to_delete = loader_.release();
  if (!loader_to_delete->task_runner()->DeleteSoon(FROM_HERE, loader_to_delete))
    delete loader_to_delete;
  ConfigurationPolicyProvider::Shutdown();
}

void AsyncPolicyProvider::RefreshPolicies() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Subtle: RefreshPolicies() has a contract that requires the next policy
  // update notification (triggered from UpdatePolicy()) to reflect any changes
  // made before this call. So if a caller has modified the policy settings and
  // invoked RefreshPolicies(), then by the next notification these policies
  // should already be provided.
  // However, it's also possible that an asynchronous Reload() is in progress
  // and just posted OnLoaderReloaded(). Therefore a task is posted to the
  // background thread before posting the next Reload, to prevent a potential
  // concurrent Reload() from triggering a notification too early. If another
  // refresh task has been posted, it is invalidated now.
  if (!loader_)
    return;
  refresh_callback_.Reset(
      base::Bind(&AsyncPolicyProvider::ReloadAfterRefreshSync,
                 weak_factory_.GetWeakPtr()));
  loader_->task_runner()->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                           refresh_callback_.callback());
}

void AsyncPolicyProvider::ReloadAfterRefreshSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This task can only enter if it was posted from RefreshPolicies(), and it
  // hasn't been cancelled meanwhile by another call to RefreshPolicies().
  DCHECK(!refresh_callback_.IsCancelled());
  // There can't be another refresh callback pending now, since its creation
  // in RefreshPolicies() would have cancelled the current execution. So it's
  // safe to cancel the |refresh_callback_| now, so that OnLoaderReloaded()
  // sees that there is no refresh pending.
  refresh_callback_.Cancel();

  if (!loader_)
    return;

  loader_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&AsyncPolicyLoader::RefreshPolicies,
                                base::Unretained(loader_.get()), schema_map()));
}

void AsyncPolicyProvider::OnLoaderReloaded(
    std::unique_ptr<PolicyBundle> bundle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only propagate policy updates if there are no pending refreshes, and if
  // Shutdown() hasn't been called yet.
  if (refresh_callback_.IsCancelled() && loader_)
    UpdatePolicy(std::move(bundle));
}

// static
void AsyncPolicyProvider::LoaderUpdateCallback(
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    base::WeakPtr<AsyncPolicyProvider> weak_this,
    std::unique_ptr<PolicyBundle> bundle) {
  runner->PostTask(FROM_HERE,
                   base::BindOnce(&AsyncPolicyProvider::OnLoaderReloaded,
                                  weak_this, std::move(bundle)));
}

}  // namespace policy
