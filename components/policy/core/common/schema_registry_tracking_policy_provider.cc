// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/schema_registry_tracking_policy_provider.h"

#include <utility>

#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/core/common/schema_registry.h"
#include "policy_types.h"

namespace policy {

SchemaRegistryTrackingPolicyProvider::SchemaRegistryTrackingPolicyProvider(
    ConfigurationPolicyProvider* delegate)
    : delegate_(delegate), state_(WAITING_FOR_REGISTRY_READY) {
  delegate_->AddObserver(this);
  // Serve the initial |delegate_| policies.
  OnUpdatePolicy(delegate_);
}

SchemaRegistryTrackingPolicyProvider::~SchemaRegistryTrackingPolicyProvider() {
  delegate_->RemoveObserver(this);
}

void SchemaRegistryTrackingPolicyProvider::Init(SchemaRegistry* registry) {
  ConfigurationPolicyProvider::Init(registry);
  if (registry->IsReady())
    OnSchemaRegistryReady();
}

bool SchemaRegistryTrackingPolicyProvider::IsInitializationComplete(
    PolicyDomain domain) const {
  if (domain == POLICY_DOMAIN_CHROME)
    return delegate_->IsInitializationComplete(domain);
  // This provider keeps its own state for all the other domains.
  return state_ == READY;
}

bool SchemaRegistryTrackingPolicyProvider::IsFirstPolicyLoadComplete(
    PolicyDomain domain) const {
  if (domain == POLICY_DOMAIN_CHROME)
    return delegate_->IsFirstPolicyLoadComplete(domain);
  // This provider keeps its own state for all the other domains.
  return state_ == READY;
}

void SchemaRegistryTrackingPolicyProvider::RefreshPolicies(
    PolicyFetchReason reason) {
  delegate_->RefreshPolicies(reason);
}

void SchemaRegistryTrackingPolicyProvider::OnSchemaRegistryReady() {
  DCHECK_EQ(WAITING_FOR_REGISTRY_READY, state_);
  // This provider's registry is ready, meaning that it has all the initial
  // components schemas; the delegate's registry should also see them now,
  // since it's tracking the former.
  // Asking the delegate to RefreshPolicies now means that the next
  // OnUpdatePolicy from the delegate will have the initial policy for
  // components.
  if (!schema_map()->HasComponents()) {
    // If there are no component registered for this provider then there's no
    // need to reload.
    state_ = READY;
    OnUpdatePolicy(delegate_);
    return;
  }

  state_ = WAITING_FOR_REFRESH;
  RefreshPolicies(PolicyFetchReason::kSchemaUpdated);
}

void SchemaRegistryTrackingPolicyProvider::OnSchemaRegistryUpdated(
    bool has_new_schemas) {
  if (state_ != READY)
    return;
  if (has_new_schemas) {
    RefreshPolicies(PolicyFetchReason::kSchemaUpdated);
  } else {
    // Remove the policies that were being served for the component that have
    // been removed. This is important so that update notifications are also
    // sent in case those component are reinstalled during the current session.
    OnUpdatePolicy(delegate_);
  }
}

void SchemaRegistryTrackingPolicyProvider::OnUpdatePolicy(
    ConfigurationPolicyProvider* provider) {
  DCHECK_EQ(delegate_, provider);

  if (state_ == WAITING_FOR_REFRESH)
    state_ = READY;

  PolicyBundle bundle;
  if (state_ == READY) {
    bundle = delegate_->policies().Clone();
    schema_map()->FilterBundle(bundle,
                               /*drop_invalid_component_policies=*/true);
  } else {
    // Always pass on the Chrome policy, even if the components are not ready
    // yet.
    const PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, "");
    bundle.Get(chrome_ns) = delegate_->policies().Get(chrome_ns).Clone();
  }

  UpdatePolicy(std::move(bundle));
}

}  // namespace policy
