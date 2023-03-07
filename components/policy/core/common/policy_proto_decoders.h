// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_PROTO_DECODERS_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_PROTO_DECODERS_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_export.h"

namespace enterprise_management {
class CloudPolicySettings;
}  // namespace enterprise_management

namespace policy {

class CloudExternalDataManager;
class PolicyMap;

enum class PolicyPerProfileFilter {
  // Applies to the browser profile.
  kTrue,
  // Applies to all browser instances.
  kFalse,
  // Any user policy.
  kAny
};

// Decode all the fields in `policy` that match the needed `per_profile` flag
// which are recognized (see the metadata in policy_constants.cc) and store them
// in the given `map`, with the given `source` and `scope`. The value of
// `per_profile` parameter specifies which fields have to be included based on
// per_profile flag.
POLICY_EXPORT void DecodeProtoFields(
    const enterprise_management::CloudPolicySettings& policy,
    base::WeakPtr<CloudExternalDataManager> external_data_manager,
    PolicySource source,
    PolicyScope scope,
    PolicyMap* map,
    PolicyPerProfileFilter per_profile);

// Parses the JSON policy in `json_dict` into `policy`, and returns true if the
// parse was successful. The `scope` and `source` are set as scope and source of
// the policy in the result. In case of failure, the `error` is populated with
// error message and false is returned.
POLICY_EXPORT bool ParseComponentPolicy(base::Value::Dict json_dict,
                                        PolicyScope scope,
                                        PolicySource source,
                                        PolicyMap* policy,
                                        std::string* error);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_PROTO_DECODERS_H_
