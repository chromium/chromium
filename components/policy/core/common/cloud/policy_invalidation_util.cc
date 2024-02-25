// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/policy_invalidation_util.h"

#include "base/strings/string_util.h"
#include "components/invalidation/public/invalidation.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

namespace {

constexpr char kFcmPolicyPublicTopicPrefix[] = "cs-";

}  // namespace

bool IsPublicInvalidationTopic(const invalidation::Topic& topic) {
  return base::StartsWith(topic, kFcmPolicyPublicTopicPrefix,
                          base::CompareCase::SENSITIVE);
}

bool GetCloudPolicyTopicFromPolicy(
    const enterprise_management::PolicyData& policy,
    invalidation::Topic* topic) {
  if (!policy.has_policy_invalidation_topic() ||
      policy.policy_invalidation_topic().empty()) {
    return false;
  }
  *topic = policy.policy_invalidation_topic();
  return true;
}

bool GetRemoteCommandTopicFromPolicy(
    const enterprise_management::PolicyData& policy,
    invalidation::Topic* topic) {
  if (!policy.has_command_invalidation_topic() ||
      policy.command_invalidation_topic().empty()) {
    return false;
  }
  *topic = policy.command_invalidation_topic();
  return true;
}

bool IsInvalidationExpired(const invalidation::Invalidation& invalidation,
                           const base::Time& last_fetch_time,
                           const base::Time& current_time) {
  // The invalidation version is the timestamp in microseconds. If the
  // invalidation occurred before the last fetch, then the invalidation
  // is expired.
  base::Time invalidation_time =
      base::Time::UnixEpoch() + base::Microseconds(invalidation.version()) +
      invalidation_timeouts::kMaxInvalidationTimeDelta;
  return invalidation_time < last_fetch_time;
}

PolicyInvalidationType GetInvalidationMetric(bool is_missing_payload,
                                             bool is_expired) {
  if (is_expired) {
    if (is_missing_payload) {
      return POLICY_INVALIDATION_TYPE_NO_PAYLOAD_EXPIRED;
    }
    return POLICY_INVALIDATION_TYPE_EXPIRED;
  }
  if (is_missing_payload) {
    return POLICY_INVALIDATION_TYPE_NO_PAYLOAD;
  }
  return POLICY_INVALIDATION_TYPE_NORMAL;
}

}  // namespace policy
