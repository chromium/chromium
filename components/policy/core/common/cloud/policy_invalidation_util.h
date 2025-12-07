// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_POLICY_INVALIDATION_UTIL_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_POLICY_INVALIDATION_UTIL_H_

#include "base/time/time.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/policy_export.h"

namespace invalidation {
class DirectInvalidation;
}  // namespace invalidation

namespace policy {
namespace invalidation_timeouts {

// The max tolerated discrepancy between policy or remote commands
// timestamps and invalidation timestamps when determining if an invalidation
// is expired.
constexpr base::TimeDelta kMaxInvalidationTimeDelta =
    base::Seconds(300);

}  // namespace invalidation_timeouts

// Determines if an invalidation is expired.
bool POLICY_EXPORT
IsInvalidationExpired(const invalidation::DirectInvalidation& invalidation,
                      const base::Time& last_fetch_time,
                      const base::Time& current_time);

// Returns a metric type depended on invalidation's state.
PolicyInvalidationType POLICY_EXPORT
GetInvalidationMetric(bool is_missing_payload, bool is_expired);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_POLICY_INVALIDATION_UTIL_H_
