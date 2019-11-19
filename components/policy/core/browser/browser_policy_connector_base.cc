// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/browser_policy_connector_base.h"

#include <stddef.h>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "components/policy/core/common/chrome_schema.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "ui/base/resource/resource_bundle.h"

namespace policy {

namespace {

// Used in BrowserPolicyConnectorBase::SetPolicyProviderForTesting.
bool g_created_policy_service = false;
ConfigurationPolicyProvider* g_testing_provider = nullptr;

}  // namespace

BrowserPolicyConnectorBase::BrowserPolicyConnectorBase(
    const HandlerListFactory& handler_list_factory) {
  // GetPolicyService() must be ready after the constructor is done.
  // The connector is created very early during startup, when the browser
  // threads aren't running yet; initialize components that need local_state,
  // the system request context or other threads (e.g. FILE) at
  // SetPolicyProviders().

  // Initialize the SchemaRegistry with the Chrome schema before creating any
  // of the policy providers in subclasses.
  const Schema& chrome_schema = policy::GetChromeSchema();
  handler_list_ = handler_list_factory.Run(chrome_schema);
  schema_registry_.RegisterComponent(PolicyNamespace(POLICY_DOMAIN_CHROME, ""),
                                     chrome_schema);
}

BrowserPolicyConnectorBase::~BrowserPolicyConnectorBase() {
  if (is_initialized()) {
    // Shutdown() wasn't invoked by our owner after having called
    // SetPolicyProviders(). This usually means it's an early shutdown and
    // BrowserProcessImpl::StartTearDown() wasn't invoked.
    // Cleanup properly in those cases and avoid crashing the ToastCrasher test.
    Shutdown();
  }
}

void BrowserPolicyConnectorBase::Shutdown() {
  is_initialized_ = false;
  if (g_testing_provider)
    g_testing_provider->Shutdown();
  for (const auto& provider : policy_providers_)
    provider->Shutdown();
  // Drop g_testing_provider so that tests executed with --single-process-tests
  // can call SetPolicyProviderForTesting() again. It is still owned by the
  // test.
  g_testing_provider = nullptr;
  g_created_policy_service = false;
}

const Schema& BrowserPolicyConnectorBase::GetChromeSchema() const {
  return policy::GetChromeSchema();
}

CombinedSchemaRegistry* BrowserPolicyConnectorBase::GetSchemaRegistry() {
  return &schema_registry_;
}

PolicyService* BrowserPolicyConnectorBase::GetPolicyService() {
  if (policy_service_)
    return policy_service_.get();

  DCHECK(!is_initialized_);
  is_initialized_ = true;

  policy_providers_ = CreatePolicyProviders();

  if (g_testing_provider)
    g_testing_provider->Init(GetSchemaRegistry());

  for (const auto& provider : policy_providers_)
    provider->Init(GetSchemaRegistry());

  g_created_policy_service = true;
  policy_service_ =
      std::make_unique<PolicyServiceImpl>(GetProvidersForPolicyService());
  return policy_service_.get();
}

const ConfigurationPolicyHandlerList*
BrowserPolicyConnectorBase::GetHandlerList() const {
  return handler_list_.get();
}

std::vector<ConfigurationPolicyProvider*>
BrowserPolicyConnectorBase::GetPolicyProviders() const {
  std::vector<ConfigurationPolicyProvider*> providers;
  for (const auto& provider : policy_providers_)
    providers.push_back(provider.get());

  return providers;
}

// static
void BrowserPolicyConnectorBase::SetPolicyProviderForTesting(
    ConfigurationPolicyProvider* provider) {
  // If this function is used by a test then it must be called before the
  // browser is created, and GetPolicyService() gets called.
  CHECK(!g_created_policy_service);
  g_testing_provider = provider;
}

void BrowserPolicyConnectorBase::NotifyWhenResourceBundleReady(
    base::OnceClosure closure) {
  DCHECK(!ui::ResourceBundle::HasSharedInstance());
  resource_bundle_callbacks_.push_back(std::move(closure));
}

// static
ConfigurationPolicyProvider*
BrowserPolicyConnectorBase::GetPolicyProviderForTesting() {
  return g_testing_provider;
}

std::vector<ConfigurationPolicyProvider*>
BrowserPolicyConnectorBase::GetProvidersForPolicyService() {
  std::vector<ConfigurationPolicyProvider*> providers;
  if (g_testing_provider) {
    providers.push_back(g_testing_provider);
    return providers;
  }
  providers.reserve(policy_providers_.size());
  for (const auto& policy : policy_providers_)
    providers.push_back(policy.get());
  return providers;
}

std::vector<std::unique_ptr<ConfigurationPolicyProvider>>
BrowserPolicyConnectorBase::CreatePolicyProviders() {
  return {};
}

void BrowserPolicyConnectorBase::OnResourceBundleCreated() {
  std::vector<base::OnceClosure> resource_bundle_callbacks;
  std::swap(resource_bundle_callbacks, resource_bundle_callbacks_);
  for (auto& closure : resource_bundle_callbacks)
    std::move(closure).Run();
}

}  // namespace policy
