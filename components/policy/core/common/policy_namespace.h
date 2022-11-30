// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_NAMESPACE_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_NAMESPACE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "components/policy/policy_export.h"

namespace policy {

// Policies are namespaced by a (PolicyDomain, ID) pair. The meaning of the ID
// string depends on the domain; for example, if the PolicyDomain is
// "extensions" then the ID identifies the extension that the policies control.
enum PolicyDomain {
  // The component ID for chrome policies is always the empty string.
  POLICY_DOMAIN_CHROME,

  // The component ID for the extension policies is equal to the extension ID.
  POLICY_DOMAIN_EXTENSIONS,

  // The namespace that corresponds to the policies for extensions running
  // under Chrome OS signin profile. The component ID is equal to the extension
  // ID.
  POLICY_DOMAIN_SIGNIN_EXTENSIONS,

  // Must be the last entry.
  POLICY_DOMAIN_SIZE,
};

// Groups a policy domain and a component ID in a single object representing
// a policy namespace. Objects of this class can be used as keys in std::maps.
struct POLICY_EXPORT PolicyNamespace {
  PolicyNamespace();
  PolicyNamespace(PolicyDomain domain, const std::string& component_id);
  PolicyNamespace(const PolicyNamespace& other);
  ~PolicyNamespace();

  PolicyNamespace& operator=(const PolicyNamespace& other);
  bool operator<(const PolicyNamespace& other) const;
  bool operator==(const PolicyNamespace& other) const;
  bool operator!=(const PolicyNamespace& other) const;

  PolicyDomain domain;
  std::string component_id;
};

typedef std::vector<PolicyNamespace> PolicyNamespaceList;

struct PolicyNamespaceHash {
  size_t operator()(const policy::PolicyNamespace& ns) const {
    return std::hash<std::string>()(ns.component_id) ^
           (UINT64_C(1) << ns.domain);
  }
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_NAMESPACE_H_
