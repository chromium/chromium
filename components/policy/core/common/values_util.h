// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_VALUES_UTIL_H_
#define COMPONENTS_POLICY_CORE_COMMON_VALUES_UTIL_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "components/policy/policy_export.h"

namespace policy {

// Converts a list of string value to string flat set. Returns empty
// set if |value| is not set. Non-string items will be ignored.
POLICY_EXPORT base::flat_set<std::string> ValueToStringSet(
    const base::Value* value);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_VALUES_UTIL_H_
