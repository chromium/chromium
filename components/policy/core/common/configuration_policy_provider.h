// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CONFIGURATION_POLICY_PROVIDER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CONFIGURATION_POLICY_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_export.h"

namespace policy {

// A mostly-abstract super class for platform-specific policy providers.
// Platform-specific policy providers (Windows Group Policy, gconf,
// etc.) should implement a subclass of this class.
class POLICY_EXPORT ConfigurationPolicyProvider
    : public SchemaRegistry::Observer {
 public:
  class POLICY_EXPORT Observer {
   public:
    virtual ~Observer();
    virtual void OnUpdatePolicy(ConfigurationPolicyProvider* provider) = 0;
  };

  ConfigurationPolicyProvider();

  // Policy providers can be deleted quite late during shutdown of the browser,
  // and it's not guaranteed that the message loops will still be running when
  // this is invoked. Override Shutdown() instead for cleanup code that needs
  // to post to the FILE thread, for example.
  ~ConfigurationPolicyProvider() override;

  // Invoked as soon as the main message loops are spinning. Policy providers
  // are created early during startup to provide the initial policies; the
  // Init() call allows them to perform initialization tasks that require
  // running message loops.
  // The policy provider will load policy for the components registered in
  // the |schema_registry| whose domain is supported by this provider.
  virtual void Init(SchemaRegistry* registry);

  // Must be invoked before deleting the provider. Implementations can override
  // this method to do appropriate cleanup while threads are still running, and
  // must also invoke ConfigurationPolicyProvider::Shutdown().
  // The provider should keep providing the current policies after Shutdown()
  // is invoked, it only has to stop updating.
  virtual void Shutdown();

  // Returns the current PolicyBundle.
  const PolicyBundle& policies() const { return policy_bundle_; }

  // Check whether this provider has completed initialization for the given
  // policy |domain|. This is used to detect whether initialization is done in
  // case implementations need to do asynchronous operations for initialization.
  virtual bool IsInitializationComplete(PolicyDomain domain) const;

  // Asks the provider to refresh its policies. All the updates caused by this
  // call will be visible on the next call of OnUpdatePolicy on the observers,
  // which are guaranteed to happen even if the refresh fails.
  // It is possible that Shutdown() is called first though, and
  // OnUpdatePolicy won't be called if that happens.
  virtual void RefreshPolicies() = 0;

  // Observers must detach themselves before the provider is deleted.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // SchemaRegistry::Observer:
  void OnSchemaRegistryUpdated(bool has_new_schemas) override;
  void OnSchemaRegistryReady() override;

 protected:
  // Subclasses must invoke this to update the policies currently served by
  // this provider. UpdatePolicy() takes ownership of |policies|.
  // The observers are notified after the policies are updated.
  void UpdatePolicy(std::unique_ptr<PolicyBundle> bundle);

  SchemaRegistry* schema_registry() const;

  const scoped_refptr<SchemaMap>& schema_map() const;

 private:
  // The policies currently configured at this provider.
  PolicyBundle policy_bundle_;

  // Used to validate proper Init() and Shutdown() nesting. This flag is set by
  // Init() and cleared by Shutdown() and needs to be false in the destructor.
  bool initialized_;

  SchemaRegistry* schema_registry_;

  base::ObserverList<Observer, true>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyProvider);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CONFIGURATION_POLICY_PROVIDER_H_
