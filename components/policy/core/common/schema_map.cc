// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/schema_map.h"

#include <utility>

#include "base/values.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"

namespace policy {

SchemaMap::SchemaMap() = default;

SchemaMap::SchemaMap(DomainMap map) : map_(std::move(map)) {}

SchemaMap::~SchemaMap() = default;

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

void SchemaMap::FilterBundle(PolicyBundle& bundle,
                             bool drop_invalid_component_policies) const {
  for (auto& bundle_item : bundle) {
    const PolicyNamespace& ns = bundle_item.first;
    PolicyMap& policy_map = bundle_item.second;

    // Chrome policies are not filtered, so that typos appear in about:policy.
    if (ns.domain == POLICY_DOMAIN_CHROME)
      continue;

    const Schema* schema = GetSchema(ns);

    if (!schema) {
      policy_map.Clear();
      continue;
    }

    if (!schema->valid()) {
      // Don't serve unknown policies.
      if (drop_invalid_component_policies) {
        policy_map.Clear();
      } else {
        policy_map.SetAllInvalid();
      }
      continue;
    }

    for (auto it_map = policy_map.begin(); it_map != policy_map.end();) {
      const std::string& policy_name = it_map->first;
      PolicyMap::Entry& entry = it_map->second;
      const Schema policy_schema = schema->GetProperty(policy_name);

      const bool has_value = entry.value_unsafe();
      bool is_valid = false;
      if (has_value) {
        if (policy_schema.Validate(*entry.value_unsafe(), SCHEMA_STRICT,
                                   /*out_error_path=*/nullptr,
                                   /*out_error=*/nullptr)) {
          is_valid = true;
        } else {
          base::Value normalized = entry.value_unsafe()->Clone();
          is_valid = policy_schema.Normalize(&normalized, SCHEMA_ALLOW_UNKNOWN,
                                             /*out_error_path=*/nullptr,
                                             /*out_error=*/nullptr,
                                             /*out_changed=*/nullptr);
          if (is_valid) {
            entry.set_value(std::move(normalized));
          }
        }
      }
      if (drop_invalid_component_policies && (!has_value || !is_valid)) {
        it_map = policy_map.EraseIt(it_map);
        continue;
      }

      ++it_map;

      if (!has_value) {
        entry.SetIgnored();
        continue;
      }

      if (!is_valid)
        entry.SetInvalid();
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
