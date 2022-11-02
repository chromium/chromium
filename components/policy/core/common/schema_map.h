// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_SCHEMA_MAP_H_
#define COMPONENTS_POLICY_CORE_COMMON_SCHEMA_MAP_H_

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_export.h"

namespace policy {

class PolicyBundle;

// Maps component id (e.g. extension id) to schema.
typedef std::map<std::string, Schema> ComponentMap;
typedef std::map<PolicyDomain, ComponentMap> DomainMap;

// Contains a mapping of policy namespaces (domain + component ID) to its
// corresponding Schema.
// This class is thread-safe.
class POLICY_EXPORT SchemaMap : public base::RefCountedThreadSafe<SchemaMap> {
 public:
  SchemaMap();
  explicit SchemaMap(DomainMap map);
  SchemaMap(const SchemaMap&) = delete;
  SchemaMap& operator=(const SchemaMap&) = delete;

  const DomainMap& GetDomains() const;

  const ComponentMap* GetComponents(PolicyDomain domain) const;

  const Schema* GetSchema(const PolicyNamespace& ns) const;

  // Removes all the policies in `bundle` that don't match the known schemas.
  // Unknown components are also dropped. Unknown fields in component policies
  // are removed.
  // If `drop_invalid_component_policies` is true, invalid policies are removed.
  // If `drop_invalid_component_policies` is false, they will merely be marked
  // invalid. They will still be filtered when accessing them via
  // PolicyMap::Get() or PolicyMap::GetValue(), but will be surfaced in
  // about:policy with an attached error.
  void FilterBundle(PolicyBundle& bundle,
                    bool drop_invalid_component_policies) const;

  // Returns true if this map contains at least one component of a domain other
  // than POLICY_DOMAIN_CHROME.
  bool HasComponents() const;

  void GetChanges(const scoped_refptr<SchemaMap>& older,
                  PolicyNamespaceList* removed,
                  PolicyNamespaceList* added) const;

 private:
  friend class base::RefCountedThreadSafe<SchemaMap>;

  void GetNamespacesNotInOther(const SchemaMap* other,
                               PolicyNamespaceList* list) const;

  ~SchemaMap();

  DomainMap map_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_SCHEMA_MAP_H_
