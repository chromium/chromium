// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_BROWSER_POLICY_CONNECTOR_BASE_H_
#define COMPONENTS_POLICY_CORE_BROWSER_BROWSER_POLICY_CONNECTOR_BASE_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_export.h"

namespace policy {

class ConfigurationPolicyProvider;
class PolicyService;
class PolicyServiceImpl;

// The BrowserPolicyConnectorBase keeps and initializes some core elements of
// the policy component, mainly the PolicyProviders and the PolicyService.
class POLICY_EXPORT BrowserPolicyConnectorBase {
 public:
  BrowserPolicyConnectorBase(const BrowserPolicyConnectorBase&) = delete;
  BrowserPolicyConnectorBase& operator=(const BrowserPolicyConnectorBase&) =
      delete;

  // Invoke Shutdown() before deleting, see below.
  virtual ~BrowserPolicyConnectorBase();

  // Stops the policy providers and cleans up the connector before it can be
  // safely deleted. This must be invoked before the destructor and while the
  // threads are still running. The policy providers are still valid but won't
  // update anymore after this call. Subclasses can override this for cleanup
  // and should call the parent method.
  virtual void Shutdown();

  // Returns true if SetPolicyProviders() has been called but Shutdown() hasn't
  // been yet.
  bool is_initialized() const { return is_initialized_; }

  // Returns a handle to the Chrome schema.
  const Schema& GetChromeSchema() const;

  // Returns the global CombinedSchemaRegistry. SchemaRegistries from Profiles
  // should be tracked by the global registry, so that the global policy
  // providers also load policies for the components of each Profile.
  CombinedSchemaRegistry* GetSchemaRegistry();

  // Returns the browser-global PolicyService, that contains policies for the
  // whole browser.
  PolicyService* GetPolicyService();

  // Returns true if the PolicyService object has already been created.
  bool HasPolicyService();

  const ConfigurationPolicyHandlerList* GetHandlerList() const;

  std::vector<ConfigurationPolicyProvider*> GetPolicyProviders() const;

  // Sets a |provider| that will be included in PolicyServices returned by
  // GetPolicyService. This is a static method because local state is
  // created immediately after the connector, and tests don't have a chance to
  // inject the provider otherwise. |provider| must outlive the connector, and
  // its ownership is not taken though the connector will initialize and shut it
  // down.
  static void SetPolicyProviderForTesting(
      ConfigurationPolicyProvider* provider);
  ConfigurationPolicyProvider* GetPolicyProviderForTesting();

  // Sets the policy service to be returned by |GetPolicyService| during tests.
  static void SetPolicyServiceForTesting(PolicyService* policy_service);

  // Adds a callback that is notified the the ResourceBundle is loaded.
  void NotifyWhenResourceBundleReady(base::OnceClosure closure);

 protected:
  // Builds an uninitialized BrowserPolicyConnectorBase. SetPolicyProviders()
  // should be called to create and start the policy components.
  explicit BrowserPolicyConnectorBase(
      const HandlerListFactory& handler_list_factory);

  // Called from GetPolicyService() to create the set of
  // ConfigurationPolicyProviders that are used, in decreasing order of
  // priority.
  virtual std::vector<std::unique_ptr<ConfigurationPolicyProvider>>
  CreatePolicyProviders();

  // Must be called when ui::ResourceBundle has been loaded, results in running
  // any callbacks scheduled in NotifyWhenResourceBundleReady().
  void OnResourceBundleCreated();

 private:
  // Returns the providers to pass to the PolicyService. Generally this is the
  // same as |policy_providers_|, unless SetPolicyProviderForTesting() has been
  // called.
  std::vector<raw_ptr<ConfigurationPolicyProvider, VectorExperimental>>
  GetProvidersForPolicyService();

  // Set to true when the PolicyService has been created, and false in
  // Shutdown(). Once created the PolicyService is destroyed in the destructor,
  // not Shutdown().
  bool is_initialized_ = false;

  // Used to convert policies to preferences. The providers declared below
  // may trigger policy updates during shutdown, which will result in
  // |handler_list_| being consulted for policy translation.
  // Therefore, it's important to destroy |handler_list_| after the providers.
  std::unique_ptr<ConfigurationPolicyHandlerList> handler_list_;

  // The global SchemaRegistry, which will track all the other registries.
  CombinedSchemaRegistry schema_registry_;

  // The browser-global policy providers, in decreasing order of priority.
  std::vector<std::unique_ptr<ConfigurationPolicyProvider>> policy_providers_;

  // Must be deleted before all the policy providers.
  std::unique_ptr<PolicyServiceImpl> policy_service_;

  // Callbacks scheduled via NotifyWhenResourceBundleReady().
  std::vector<base::OnceClosure> resource_bundle_callbacks_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_BROWSER_POLICY_CONNECTOR_BASE_H_
