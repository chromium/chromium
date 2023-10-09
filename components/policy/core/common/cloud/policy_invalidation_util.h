// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_POLICY_INVALIDATION_UTIL_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_POLICY_INVALIDATION_UTIL_H_

#include "base/time/time.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/policy_export.h"

namespace enterprise_management {

class PolicyData;

}  // namespace enterprise_management

namespace invalidation {

class Invalidation;

}  // namespace invalidation

namespace policy {

namespace invalidation_timeouts {

// The max tolerated discrepancy between policy or remote commands
// timestamps and invalidation timestamps when determining if an invalidation
// is expired.
constexpr base::TimeDelta kMaxInvalidationTimeDelta =
    base::Seconds(300);

}  // namespace invalidation_timeouts

// Returns true if |topic| is a public topic. Topic can be either public or
// private. Private topic is keyed by GAIA ID, while public isn't, so many
// users can be subscribed to the same public topic.
// For example:
// If client subscribes to "DeviceGuestModeEnabled" public topic all the
// clients subscribed to this topic will receive all the outgoing messages
// addressed to topic "DeviceGuestModeEnabled". But if 2 clients with different
// users subscribe to private topic "BOOKMARK", they will receive different set
// of messages addressed to pair ("BOOKMARK", GAIA ID) respectievely.
bool POLICY_EXPORT IsPublicInvalidationTopic(const invalidation::Topic& topic);

// Returns true if |policy| has data about policy to invalidate, and saves
// that data in |topic|, and false otherwise.
bool POLICY_EXPORT
GetCloudPolicyTopicFromPolicy(const enterprise_management::PolicyData& policy,
                              invalidation::Topic* topic);

// The same as GetCloudPolicyTopicFromPolicy but gets the |topic| for
// remote command.
bool POLICY_EXPORT
GetRemoteCommandTopicFromPolicy(const enterprise_management::PolicyData& policy,
                                invalidation::Topic* topic);

// Determines if an invalidation is expired.
bool POLICY_EXPORT
IsInvalidationExpired(const invalidation::Invalidation& invalidation,
                      const base::Time& last_fetch_time,
                      const base::Time& current_time);

// Returns a metric type depended on invalidation's state.
PolicyInvalidationType POLICY_EXPORT
GetInvalidationMetric(bool is_missing_payload, bool is_expired);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_POLICY_INVALIDATION_UTIL_H_
