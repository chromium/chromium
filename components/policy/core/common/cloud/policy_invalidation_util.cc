// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/policy_invalidation_util.h"

#include "components/invalidation/invalidation_listener.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

bool IsInvalidationExpired(const invalidation::DirectInvalidation& invalidation,
                           const base::Time& last_fetch_time,
                           const base::Time& current_time) {
  // If the invalidation occurred before the last fetch, then the invalidation
  // is expired.
  return invalidation.issue_timestamp() +
             invalidation_timeouts::kMaxInvalidationTimeDelta <
         last_fetch_time;
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
