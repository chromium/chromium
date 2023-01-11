// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_DETAILS_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_DETAILS_H_

#include <stddef.h>

#include <string>

#include "base/functional/callback_forward.h"
#include "components/policy/policy_export.h"
#include "components/policy/risk_tag.h"

namespace policy {

// Contains read-only metadata about a Chrome policy.
struct POLICY_EXPORT PolicyDetails {
  // True if this policy has been deprecated.
  bool is_deprecated : 1;

  // True if the policy hasn't been released yet.
  bool is_future : 1;

  // True if this policy is a Chrome OS device policy.
  bool is_device_policy : 1;

  // The id of the protobuf field that contains this policy,
  // in the cloud policy protobuf.
  short id;

  // If this policy references external data then this is the maximum size
  // allowed for that data.
  // Otherwise this field is 0 and doesn't have any meaning.
  uint32_t max_external_data_size;

  // Contains tags that describe impact on a user's privacy or security.
  RiskTag risk_tags[kMaxRiskTagCount];
};

// A typedef for functions that match the signature of
// GetChromePolicyDetails(). This can be used to inject that
// function into objects, so that it can be easily mocked for
// tests.
using GetChromePolicyDetailsCallback =
    base::RepeatingCallback<const PolicyDetails*(const std::string&)>;

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_DETAILS_H_
