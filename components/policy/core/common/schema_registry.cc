// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/schema_registry.h"

#include "base/logging.h"
#include "extensions/buildflags/buildflags.h"

namespace policy {

SchemaRegistry::Observer::~Observer() {}

SchemaRegistry::InternalObserver::~InternalObserver() {}

SchemaRegistry::SchemaRegistry() : schema_map_(new SchemaMap) {
  for (int i = 0; i < POLICY_DOMAIN_SIZE; ++i)
    domains_ready_[i] = false;
#if !BUILDFLAG(ENABLE_EXTENSIONS)
  SetExtensionsDomainsReady();
#endif
}

SchemaRegistry::~SchemaRegistry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : internal_observers_)
    observer.OnSchemaRegistryShuttingDown(this);
}

void SchemaRegistry::RegisterComponent(const PolicyNamespace& ns,
                                       const Schema& schema) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ComponentMap map;
  map[ns.component_id] = schema;
  RegisterComponents(ns.domain, map);
}

void SchemaRegistry::RegisterComponents(PolicyDomain domain,
                                        const ComponentMap& components) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Don't issue notifications if nothing is being registered.
  if (components.empty())
    return;
  // Assume that a schema was updated if the namespace was already registered
  // before.
  DomainMap map(schema_map_->GetDomains());
  for (auto it = components.begin(); it != components.end(); ++it)
    map[domain][it->first] = it->second;
  schema_map_ = new SchemaMap(map);
  Notify(true);
}

void SchemaRegistry::UnregisterComponent(const PolicyNamespace& ns) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DomainMap map(schema_map_->GetDomains());
  if (map[ns.domain].erase(ns.component_id) != 0) {
    schema_map_ = new SchemaMap(map);
    Notify(false);
  } else {
    // Extension might be uninstalled before install so the associated policies
    // are unregistered before registered. For example, a policy forced
    // extension is removed from forced list during launch due to policy update.
    DCHECK(ns.domain != POLICY_DOMAIN_CHROME);
  }
}

bool SchemaRegistry::IsReady() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (int i = 0; i < POLICY_DOMAIN_SIZE; ++i) {
    if (!domains_ready_[i])
      return false;
  }
  return true;
}

void SchemaRegistry::SetDomainReady(PolicyDomain domain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (domains_ready_[domain])
    return;
  domains_ready_[domain] = true;
  if (IsReady()) {
    for (auto& observer : observers_)
      observer.OnSchemaRegistryReady();
  }
}

void SchemaRegistry::SetAllDomainsReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (int i = 0; i < POLICY_DOMAIN_SIZE; ++i)
    SetDomainReady(static_cast<PolicyDomain>(i));
}

void SchemaRegistry::SetExtensionsDomainsReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetDomainReady(POLICY_DOMAIN_EXTENSIONS);
  SetDomainReady(POLICY_DOMAIN_SIGNIN_EXTENSIONS);
}

void SchemaRegistry::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void SchemaRegistry::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void SchemaRegistry::AddInternalObserver(InternalObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  internal_observers_.AddObserver(observer);
}

void SchemaRegistry::RemoveInternalObserver(InternalObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  internal_observers_.RemoveObserver(observer);
}

void SchemaRegistry::Notify(bool has_new_schemas) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_)
    observer.OnSchemaRegistryUpdated(has_new_schemas);
}

CombinedSchemaRegistry::CombinedSchemaRegistry()
    : own_schema_map_(new SchemaMap) {
  // The combined registry is always ready, since it can always start tracking
  // another registry that is not ready yet and going from "ready" to "not
  // ready" is not allowed.
  SetAllDomainsReady();
}

CombinedSchemaRegistry::~CombinedSchemaRegistry() {}

void CombinedSchemaRegistry::Track(SchemaRegistry* registry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  registries_.insert(registry);
  registry->AddObserver(this);
  registry->AddInternalObserver(this);
  // Recombine the maps only if the |registry| has any components other than
  // POLICY_DOMAIN_CHROME.
  if (registry->schema_map()->HasComponents())
    Combine(true);
}

void CombinedSchemaRegistry::RegisterComponents(
    PolicyDomain domain,
    const ComponentMap& components) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DomainMap map(own_schema_map_->GetDomains());
  for (auto it = components.begin(); it != components.end(); ++it)
    map[domain][it->first] = it->second;
  own_schema_map_ = new SchemaMap(map);
  Combine(true);
}

void CombinedSchemaRegistry::UnregisterComponent(const PolicyNamespace& ns) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DomainMap map(own_schema_map_->GetDomains());
  if (map[ns.domain].erase(ns.component_id) != 0) {
    own_schema_map_ = new SchemaMap(map);
    Combine(false);
  } else {
    NOTREACHED();
  }
}

void CombinedSchemaRegistry::OnSchemaRegistryUpdated(bool has_new_schemas) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Combine(has_new_schemas);
}

void CombinedSchemaRegistry::OnSchemaRegistryShuttingDown(
    SchemaRegistry* registry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  registry->RemoveObserver(this);
  registry->RemoveInternalObserver(this);
  if (registries_.erase(registry) != 0) {
    if (registry->schema_map()->HasComponents())
      Combine(false);
  } else {
    NOTREACHED();
  }
}

void CombinedSchemaRegistry::Combine(bool has_new_schemas) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If two registries publish a Schema for the same component then it's
  // undefined which version gets in the combined registry.
  //
  // The common case is that both registries want policy for the same component,
  // and the Schemas should be the same; in that case this makes no difference.
  //
  // But if the Schemas are different then one of the components is out of date.
  // In that case the policy loaded will be valid only for one of them, until
  // the outdated components are updated. This is a known limitation of the
  // way policies are loaded currently, but isn't a problem worth fixing for
  // the time being.
  DomainMap map(own_schema_map_->GetDomains());
  for (auto reg_it = registries_.begin(); reg_it != registries_.end();
       ++reg_it) {
    const DomainMap& reg_domain_map = (*reg_it)->schema_map()->GetDomains();
    for (auto domain_it = reg_domain_map.begin();
         domain_it != reg_domain_map.end(); ++domain_it) {
      const ComponentMap& reg_component_map = domain_it->second;
      for (auto comp_it = reg_component_map.begin();
           comp_it != reg_component_map.end(); ++comp_it) {
        map[domain_it->first][comp_it->first] = comp_it->second;
      }
    }
  }
  schema_map_ = new SchemaMap(map);
  Notify(has_new_schemas);
}

ForwardingSchemaRegistry::ForwardingSchemaRegistry(SchemaRegistry* wrapped)
    : wrapped_(wrapped) {
  schema_map_ = wrapped_->schema_map();
  wrapped_->AddObserver(this);
  wrapped_->AddInternalObserver(this);
  UpdateReadiness();
}

ForwardingSchemaRegistry::~ForwardingSchemaRegistry() {
  if (wrapped_) {
    wrapped_->RemoveObserver(this);
    wrapped_->RemoveInternalObserver(this);
  }
}

void ForwardingSchemaRegistry::RegisterComponents(
    PolicyDomain domain,
    const ComponentMap& components) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // POLICY_DOMAIN_CHROME is skipped to avoid spurious updates when a new
  // Profile is created. If the ForwardingSchemaRegistry is used outside
  // device-level accounts then this should become configurable.
  if (wrapped_ && domain != POLICY_DOMAIN_CHROME)
    wrapped_->RegisterComponents(domain, components);
  // Ignore otherwise.
}

void ForwardingSchemaRegistry::UnregisterComponent(const PolicyNamespace& ns) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (wrapped_)
    wrapped_->UnregisterComponent(ns);
  // Ignore otherwise.
}

void ForwardingSchemaRegistry::OnSchemaRegistryUpdated(bool has_new_schemas) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  schema_map_ = wrapped_->schema_map();
  Notify(has_new_schemas);
}

void ForwardingSchemaRegistry::OnSchemaRegistryReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateReadiness();
}

void ForwardingSchemaRegistry::OnSchemaRegistryShuttingDown(
    SchemaRegistry* registry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(wrapped_, registry);
  wrapped_->RemoveObserver(this);
  wrapped_->RemoveInternalObserver(this);
  wrapped_ = nullptr;
  // Keep serving the same |schema_map_|.
}

void ForwardingSchemaRegistry::UpdateReadiness() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (wrapped_->IsReady())
    SetAllDomainsReady();
}

}  // namespace policy
