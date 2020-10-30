// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/proxy_policy_provider.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "components/policy/core/common/policy_bundle.h"

namespace policy {

ProxyPolicyProvider::ProxyPolicyProvider() : delegate_(NULL) {}

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
    UpdatePolicy(std::unique_ptr<PolicyBundle>(new PolicyBundle()));
  }
}

void ProxyPolicyProvider::Shutdown() {
  // Note: the delegate is not owned by the proxy provider, so this call is not
  // forwarded. The same applies for the Init() call.
  // Just drop the delegate without propagating updates here.
  if (delegate_) {
    delegate_->RemoveObserver(this);
    delegate_ = NULL;
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
    std::unique_ptr<PolicyBundle> bundle(new PolicyBundle());
    bundle->CopyFrom(policies());
    UpdatePolicy(std::move(bundle));
  }
}

void ProxyPolicyProvider::OnUpdatePolicy(
    ConfigurationPolicyProvider* provider) {
  if (block_policy_updates_for_testing_)
    return;

  DCHECK_EQ(delegate_, provider);
  std::unique_ptr<PolicyBundle> bundle(new PolicyBundle());
  bundle->CopyFrom(delegate_->policies());
  UpdatePolicy(std::move(bundle));
}

}  // namespace policy
