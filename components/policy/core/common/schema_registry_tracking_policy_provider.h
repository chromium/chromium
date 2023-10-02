// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_SCHEMA_REGISTRY_TRACKING_POLICY_PROVIDER_H_
#define COMPONENTS_POLICY_CORE_COMMON_SCHEMA_REGISTRY_TRACKING_POLICY_PROVIDER_H_

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_export.h"

namespace policy {

// A policy provider that relies on a delegate provider to obtain policy
// settings, but uses a different SchemaRegistry to determine which policy
// namespaces to request from the delegate provider.
//
// This provider tracks the SchemaRegistry's state, and becomes ready after
// making sure the delegate provider has refreshed its policies with an updated
// view of the complete schema. It is expected that the delegate's
// SchemaRegistry is a CombinedSchemaRegistry tracking the
// SchemaRegistryTrackingPolicyProvider's registry.
//
// This policy provider implementation is used to wrap the platform policy
// provider for use with individual profiles, which may have different
// SchemaRegistries. The SchemaRegistryTrackingPolicyProvider ensures that
// initialization completion is only signaled for non-Chrome PolicyDomains after
// the SchemaRegistry is fully initialized. This is important to avoid flapping
// on startup due to asynchronous SchemaRegistry initialization while the
// underlying policy provider has already completed initialization.
//
// A concrete example of this is POLICY_DOMAIN_EXTENSIONS, which registers
// the PolicyNamespaces for the different extensions it's interested in based
// on what extensions are installed in a Profile. Before that happens, the
// underlying policy providers will not load the corresponding policy, so at
// startup there would be a window during which the policy appears to be not
// present. This is avoided by only flagging POLICY_DOMAIN_EXTENSIONS ready
// once the corresponding SchemaRegistry has been fully initialized with the
// list of installed extensions.
class POLICY_EXPORT SchemaRegistryTrackingPolicyProvider
    : public ConfigurationPolicyProvider,
      public ConfigurationPolicyProvider::Observer {
 public:
  // The |delegate| must outlive this provider.
  explicit SchemaRegistryTrackingPolicyProvider(
      ConfigurationPolicyProvider* delegate);
  SchemaRegistryTrackingPolicyProvider(
      const SchemaRegistryTrackingPolicyProvider&) = delete;
  SchemaRegistryTrackingPolicyProvider& operator=(
      const SchemaRegistryTrackingPolicyProvider&) = delete;
  ~SchemaRegistryTrackingPolicyProvider() override;

  // ConfigurationPolicyProvider:
  //
  // Note that Init() and Shutdown() are not forwarded to the |delegate_|, since
  // this provider does not own it and its up to the |delegate_|'s owner to
  // initialize it and shut it down.
  //
  // Note also that this provider may have a SchemaRegistry passed in Init()
  // that doesn't match the |delegate_|'s; therefore OnSchemaRegistryUpdated()
  // and OnSchemaRegistryReady() are not forwarded either. It is assumed that
  // the |delegate_|'s SchemaRegistry contains a superset of this provider's
  // SchemaRegistry though (i.e. it's a CombinedSchemaRegistry that contains
  // this provider's SchemaRegistry).
  //
  // This provider manages its own initialization state for all policy domains
  // except POLICY_DOMAIN_CHROME, whose status is always queried from the
  // |delegate_|. RefreshPolicies() calls are also forwarded, since this
  // provider doesn't have a "real" policy source of its own.
  void Init(SchemaRegistry* registry) override;
  bool IsInitializationComplete(PolicyDomain domain) const override;
  bool IsFirstPolicyLoadComplete(PolicyDomain domain) const override;
  void RefreshPolicies(PolicyFetchReason reason) override;
  void OnSchemaRegistryReady() override;
  void OnSchemaRegistryUpdated(bool has_new_schemas) override;

  // ConfigurationPolicyProvider::Observer:
  void OnUpdatePolicy(ConfigurationPolicyProvider* provider) override;

 private:
  enum InitializationState {
    WAITING_FOR_REGISTRY_READY,
    WAITING_FOR_REFRESH,
    READY,
  };

  raw_ptr<ConfigurationPolicyProvider> delegate_;
  InitializationState state_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_SCHEMA_REGISTRY_TRACKING_POLICY_PROVIDER_H_
