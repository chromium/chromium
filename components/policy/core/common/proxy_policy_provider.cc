// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/proxy_policy_provider.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "components/policy/core/common/policy_bundle.h"

namespace policy {

ProxyPolicyProvider::ProxyPolicyProvider() : delegate_(nullptr) {}

ProxyPolicyProvider::~ProxyPolicyProvider() {
  DCHECK(!delegate_);
}

void ProxyPolicyProvider::SetDelegate(ConfigurationPolicyProvider* delegate) {
  if (delegate_)
    delegate_->RemoveObserver(this);
  delegate_ = delegate;
  if (delegate_) {
    delegate_->AddObserver(this);
    OnUpdatePolicy(delegate_);
  } else {
    UpdatePolicy(PolicyBundle());
  }
}

void ProxyPolicyProvider::Shutdown() {
  // Note: the delegate is not owned by the proxy provider, so this call is not
  // forwarded. The same applies for the Init() call.
  // Just drop the delegate without propagating updates here.
  if (delegate_) {
    delegate_->RemoveObserver(this);
    delegate_ = nullptr;
  }
  ConfigurationPolicyProvider::Shutdown();
}

void ProxyPolicyProvider::RefreshPolicies() {
  if (delegate_) {
    delegate_->RefreshPolicies();
  } else {
    // Subtle: if a RefreshPolicies() call comes after Shutdown() then the
    // current bundle should be served instead. This also does the right thing
    // if SetDelegate() was never called before.
    UpdatePolicy(policies().Clone());
  }
}

bool ProxyPolicyProvider::IsFirstPolicyLoadComplete(PolicyDomain domain) const {
  return delegate_ && delegate_->IsInitializationComplete(domain);
}

void ProxyPolicyProvider::OnUpdatePolicy(
    ConfigurationPolicyProvider* provider) {
  if (block_policy_updates_for_testing_)
    return;

  DCHECK_EQ(delegate_, provider);
  UpdatePolicy(delegate_->policies().Clone());
}

}  // namespace policy
