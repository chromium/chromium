// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_SCHEMA_MAP_H_
#define COMPONENTS_POLICY_CORE_COMMON_SCHEMA_MAP_H_

#include <map>
#include <string>

#include "base/macros.h"
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
  // Takes ownership of |map| (its contents will be swapped).
  // TODO(emaxx): Change to use move semantics.
  explicit SchemaMap(DomainMap& map);

  const DomainMap& GetDomains() const;

  const ComponentMap* GetComponents(PolicyDomain domain) const;

  const Schema* GetSchema(const PolicyNamespace& ns) const;

  // Removes all the policies in |bundle| that don't match the known schemas.
  // Unknown components are also dropped. Unknown fields in component policies
  // are removed.
  void FilterBundle(PolicyBundle* bundle) const;

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

  DISALLOW_COPY_AND_ASSIGN(SchemaMap);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_SCHEMA_MAP_H_
