// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/configuration_policy_provider.h"

#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"

namespace policy {

ConfigurationPolicyProvider::Observer::~Observer() = default;

ConfigurationPolicyProvider::ConfigurationPolicyProvider()
    : initialized_(false), schema_registry_(nullptr) {}

ConfigurationPolicyProvider::~ConfigurationPolicyProvider() {
  DCHECK(!initialized_);
}

void ConfigurationPolicyProvider::Init(SchemaRegistry* registry) {
  schema_registry_ = registry;
  schema_registry_->AddObserver(this);
  initialized_ = true;
}

void ConfigurationPolicyProvider::Shutdown() {
  initialized_ = false;
  if (schema_registry_) {
    // Unit tests don't initialize the BrowserPolicyConnector but call
    // shutdown; handle that.
    schema_registry_->RemoveObserver(this);
    schema_registry_ = nullptr;
  }
}

bool ConfigurationPolicyProvider::IsInitializationComplete(
    PolicyDomain domain) const {
  return true;
}

bool ConfigurationPolicyProvider::IsFirstPolicyLoadComplete(
    PolicyDomain domain) const {
  return true;
}

void ConfigurationPolicyProvider::UpdatePolicy(PolicyBundle bundle) {
  policy_bundle_ = std::move(bundle);
  for (auto& observer : observer_list_)
    observer.OnUpdatePolicy(this);
}

SchemaRegistry* ConfigurationPolicyProvider::schema_registry() const {
  return schema_registry_;
}

const scoped_refptr<SchemaMap>&
ConfigurationPolicyProvider::schema_map() const {
  return schema_registry_->schema_map();
}

void ConfigurationPolicyProvider::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ConfigurationPolicyProvider::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ConfigurationPolicyProvider::OnSchemaRegistryUpdated(
    bool has_new_schemas) {}

void ConfigurationPolicyProvider::OnSchemaRegistryReady() {}

#if BUILDFLAG(IS_ANDROID)
void ConfigurationPolicyProvider::ShutdownForTesting() {
  observer_list_.Clear();
  Shutdown();
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace policy
