// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_utils.h"

#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/schema.h"
#include "components/prefs/pref_service.h"

namespace policy::utils {

bool IsPolicyTestingEnabled(PrefService* pref_service,
                            version_info::Channel channel) {
  if (pref_service &&
      !pref_service->GetBoolean(policy_prefs::kPolicyTestPageEnabled)) {
    return false;
  }

  if (channel == version_info::Channel::CANARY ||
      channel == version_info::Channel::DEFAULT) {
    return true;
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_IOS)
  if (channel == version_info::Channel::DEV) {
    return true;
  }
#endif

#if BUILDFLAG(IS_IOS)
  if (channel == version_info::Channel::BETA) {
    return true;
  }
#endif

#if !defined(NDEBUG)
  // The page should be available in debug builds.
  return true;
#else
  return false;
#endif
}

base::Value::Dict GetPolicyNameToTypeMapping(
    const base::Value::List& policy_names,
    const policy::Schema& schema) {
  base::Value::Dict result;
  for (auto& policy_name : policy_names) {
    base::Value::Type policy_type =
        schema.GetKnownProperty(policy_name.GetString()).type();
    switch (policy_type) {
      case base::Value::Type::BOOLEAN:
        result.Set(policy_name.GetString(), "boolean");
        break;
      case base::Value::Type::DICT:
        result.Set(policy_name.GetString(), "dictionary");
        break;
      case base::Value::Type::INTEGER:
        result.Set(policy_name.GetString(), "integer");
        break;
      case base::Value::Type::DOUBLE:
        result.Set(policy_name.GetString(), "number");
        break;
      case base::Value::Type::LIST:
        result.Set(policy_name.GetString(), "list");
        break;
      case base::Value::Type::STRING:
        result.Set(policy_name.GetString(), "string");
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Unrecognized policy type " << (int)policy_type << " ("
            << policy_name << ")";
        break;
    }
  }
  return result;
}

}  // namespace policy::utils
