// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_AFFILIATION_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_AFFILIATION_H_

#include <string>

#include "base/containers/flat_set.h"
#include "components/policy/policy_export.h"

namespace policy {

// Returns true if the user and browser are managed by the same customer
// (affiliated). This is determined by comparing affiliation IDs obtained in the
// policy fetching response. If either policies has no affiliation IDs, this
// function returns false.
POLICY_EXPORT bool IsAffiliated(const base::flat_set<std::string>& user_ids,
                                const base::flat_set<std::string>& device_ids);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_AFFILIATION_H_
