// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/proxy_policy_provider.h"

#include <memory>
#include <utility>
#include <variant>

#include "base/check_op.h"
#include "components/policy/core/common/policy_bundle.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace policy {

ProxyPolicyProvider::ProxyPolicyProvider() = default;

ProxyPolicyProvider::~ProxyPolicyProvider() {
  DCHECK(!delegate());
}

void ProxyPolicyProvider::SetOwnedDelegate(OwnedDelegate delegate) {
  ResetDelegate();
  if (delegate) {
    delegate_ = std::move(delegate);
  }
  OnDelegateChanged();
}

void ProxyPolicyProvider::SetUnownedDelegate(UnownedDelegate delegate) {
  ResetDelegate();
  delegate_ = delegate;
  OnDelegateChanged();
}

void ProxyPolicyProvider::Shutdown() {
  // Note: the delegate is not owned by the proxy provider, so this call is not
  // forwarded. The same applies for the Init() call.
  // Just drop the delegate without propagating updates here.
  ResetDelegate();
  ConfigurationPolicyProvider::Shutdown();
}

void ProxyPolicyProvider::RefreshPolicies(PolicyFetchReason reason) {
  if (delegate()) {
    delegate()->RefreshPolicies(reason);
  } else {
    // Subtle: if a RefreshPolicies() call comes after Shutdown() then the
    // current bundle should be served instead. This also does the right thing
    // if SetDelegate() was never called before.
    UpdatePolicy(policies().Clone());
  }
}

bool ProxyPolicyProvider::IsFirstPolicyLoadComplete(PolicyDomain domain) const {
  // - Uninitialized delegate always returns false.
  // - An initialized but nullptr delegate returns true.
  // - An initialized and non-null delegate calls the delegate.
  return !std::holds_alternative<Unspecified>(delegate_) &&
         (!delegate() || delegate()->IsInitializationComplete(domain));
}

void ProxyPolicyProvider::OnUpdatePolicy(
    ConfigurationPolicyProvider* provider) {
  if (block_policy_updates_for_testing_)
    return;

  DCHECK_EQ(delegate(), provider);
  UpdatePolicy(delegate()->policies().Clone());
}

ConfigurationPolicyProvider* ProxyPolicyProvider::delegate() {
  return std::visit(
      absl::Overload(
          [](Unspecified) -> ConfigurationPolicyProvider* { return nullptr; },
          [](const auto& delegate) { return delegate.get(); }),
      delegate_);
}

const ConfigurationPolicyProvider* ProxyPolicyProvider::delegate() const {
  return std::visit(
      absl::Overload(
          [](Unspecified) -> ConfigurationPolicyProvider* { return nullptr; },
          [](const auto& delegate) { return delegate.get(); }),
      delegate_);
}

void ProxyPolicyProvider::ResetDelegate() {
  if (std::holds_alternative<OwnedDelegate>(delegate_)) {
    std::get<OwnedDelegate>(delegate_)->Shutdown();
  }

  if (delegate()) {
    delegate()->RemoveObserver(this);
  }

  // If Unspecified, stay Unspecified.
  if (!std::holds_alternative<Unspecified>(delegate_)) {
    delegate_ = UnownedDelegate(nullptr);
  }
}

void ProxyPolicyProvider::OnDelegateChanged() {
  if (delegate()) {
    delegate()->AddObserver(this);
    OnUpdatePolicy(delegate());
  } else {
    UpdatePolicy(PolicyBundle());
  }
}

}  // namespace policy
