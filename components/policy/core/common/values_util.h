// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_VALUES_UTIL_H_
#define COMPONENTS_POLICY_CORE_COMMON_VALUES_UTIL_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_export.h"

namespace policy {

// A map that represents component policy values as downloaded by the server.
// The key is the component represented as a PolicyNamespace (e.g. a chrome
// extension).
// The value is a JSON value in the format understood by ComponentPolicyStore
// (Chrome OS section of
// https://www.chromium.org/administrators/configuring-policy-for-extensions/)
using ComponentPolicyMap = base::flat_map<PolicyNamespace, base::Value>;

// Converts a list of string value to string flat set. Returns empty
// set if |value| is not set. Non-string items will be ignored.
POLICY_EXPORT base::flat_set<std::string> ValueToStringSet(
    const base::Value* value);

// Returns a copy of provided map.
POLICY_EXPORT ComponentPolicyMap
CopyComponentPolicyMap(const ComponentPolicyMap& map);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_VALUES_UTIL_H_
