// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_namespace.h"

#include <tuple>

namespace policy {

PolicyNamespace::PolicyNamespace() = default;

PolicyNamespace::PolicyNamespace(PolicyDomain domain,
                                 const std::string& component_id)
    : domain(domain),
      component_id(component_id) {}

PolicyNamespace::PolicyNamespace(const PolicyNamespace& other)
    : domain(other.domain),
      component_id(other.component_id) {}

PolicyNamespace::~PolicyNamespace() = default;

PolicyNamespace& PolicyNamespace::operator=(const PolicyNamespace& other) {
  domain = other.domain;
  component_id = other.component_id;
  return *this;
}

bool PolicyNamespace::operator<(const PolicyNamespace& other) const {
  return std::tie(domain, component_id) <
         std::tie(other.domain, other.component_id);
}

bool PolicyNamespace::operator==(const PolicyNamespace& other) const {
  return domain == other.domain && component_id == other.component_id;
}

bool PolicyNamespace::operator!=(const PolicyNamespace& other) const {
  return !(*this == other);
}

}  // namespace policy
