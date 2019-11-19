// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/schema_map.h"

#include "base/logging.h"
#include "base/values.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"

namespace policy {

SchemaMap::SchemaMap() {}

SchemaMap::SchemaMap(DomainMap& map) {
  map_.swap(map);
}

SchemaMap::~SchemaMap() {}

const DomainMap& SchemaMap::GetDomains() const {
  return map_;
}

const ComponentMap* SchemaMap::GetComponents(PolicyDomain domain) const {
  const auto it = map_.find(domain);
  return it == map_.end() ? nullptr : &it->second;
}

const Schema* SchemaMap::GetSchema(const PolicyNamespace& ns) const {
  const ComponentMap* map = GetComponents(ns.domain);
  if (!map)
    return nullptr;
  const auto it = map->find(ns.component_id);
  return it == map->end() ? nullptr : &it->second;
}

void SchemaMap::FilterBundle(PolicyBundle* bundle) const {
  for (const auto& bundle_item : *bundle) {
    const PolicyNamespace& ns = bundle_item.first;
    const std::unique_ptr<PolicyMap>& policy_map = bundle_item.second;

    // Chrome policies are not filtered, so that typos appear in about:policy.
    // Everything else gets filtered, so that components only see valid policy.
    if (ns.domain == POLICY_DOMAIN_CHROME)
      continue;

    const Schema* schema = GetSchema(ns);

    if (!schema) {
      policy_map->Clear();
      continue;
    }

    if (!schema->valid()) {
      // Don't serve unknown policies.
      policy_map->Clear();
      continue;
    }

    for (auto it_map = policy_map->begin(); it_map != policy_map->end();) {
      const std::string& policy_name = it_map->first;
      base::Value* policy_value = it_map->second.value.get();
      Schema policy_schema = schema->GetProperty(policy_name);
      ++it_map;
      std::string error_path;
      std::string error;
      if (!policy_value ||
          !policy_schema.Normalize(policy_value, SCHEMA_ALLOW_UNKNOWN,
                                   &error_path, &error, nullptr)) {
        LOG(ERROR) << "Dropping policy " << policy_name << " of component "
                   << ns.component_id << " due to error at "
                   << (error_path.empty() ? "root" : error_path) << ": "
                   << error;
        policy_map->Erase(policy_name);
      }
    }
  }
}

bool SchemaMap::HasComponents() const {
  for (const auto& item : map_) {
    const PolicyDomain& domain = item.first;
    const ComponentMap& component_map = item.second;
    if (domain == POLICY_DOMAIN_CHROME)
      continue;
    if (!component_map.empty())
      return true;
  }
  return false;
}

void SchemaMap::GetChanges(const scoped_refptr<SchemaMap>& older,
                           PolicyNamespaceList* removed,
                           PolicyNamespaceList* added) const {
  GetNamespacesNotInOther(older.get(), added);
  older->GetNamespacesNotInOther(this, removed);
}

void SchemaMap::GetNamespacesNotInOther(const SchemaMap* other,
                                        PolicyNamespaceList* list) const {
  list->clear();
  for (const auto& item : map_) {
    const PolicyDomain& domain = item.first;
    const ComponentMap& component_map = item.second;
    for (const auto& comp : component_map) {
      const std::string& component_id = comp.first;
      const PolicyNamespace ns(domain, component_id);
      if (!other->GetSchema(ns))
        list->push_back(ns);
    }
  }
}

}  // namespace policy
