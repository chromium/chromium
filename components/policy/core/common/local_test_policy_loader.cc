// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/local_test_policy_loader.h"

#include <string>

#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_loader_common.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace {
const char kLevel[] = "level";
const char kScope[] = "scope";
const char kSource[] = "source";
const char kName[] = "name";
const char kValue[] = "value";
const char kLocalTestId[] = "local_test_id";

policy::PolicyMap GetPolicyMapWithEntry(base::Value::Dict* policy_dict) {
  policy::PolicyLevel level =
      static_cast<policy::PolicyLevel>(*policy_dict->FindInt(kLevel));
  policy::PolicyScope scope =
      static_cast<policy::PolicyScope>(*policy_dict->FindInt(kScope));
  policy::PolicySource source =
      static_cast<policy::PolicySource>(*policy_dict->FindInt(kSource));
  std::string name = *policy_dict->FindString(kName);
  base::Value value = policy_dict->Find(kValue)->Clone();

  policy::PolicyMap::Entry entry(level, scope, source, std::move(value),
                                 nullptr);

  policy::PolicyMap entry_map = policy::PolicyMap();
  entry_map.Set(name, std::move(entry));
  return entry_map;
}

}  // namespace

namespace policy {

LocalTestPolicyLoader::LocalTestPolicyLoader() = default;

LocalTestPolicyLoader::~LocalTestPolicyLoader() = default;

PolicyBundle LocalTestPolicyLoader::Load() {
  return bundle_.Clone();
}

void LocalTestPolicyLoader::SetPolicyListJson(
    const std::string& policy_list_json) {
  PolicyBundle bundle;

  auto policies = base::JSONReader::ReadAndReturnValueWithError(
      policy_list_json, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  CHECK(policies.has_value() && policies->is_list())
      << "List of policies expected";

  PolicyMap& policy_map =
      bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));

  // Add affiliation id if user should be affiliated.
  if (is_user_affiliated_) {
    base::flat_set<std::string> user_ids(policy_map.GetUserAffiliationIds());
    user_ids.insert(kLocalTestId);
    base::flat_set<std::string> device_ids(
        policy_map.GetDeviceAffiliationIds());
    device_ids.insert(kLocalTestId);
    policy_map.SetUserAffiliationIds(user_ids);
    policy_map.SetDeviceAffiliationIds(device_ids);
  }

  for (auto& policy : policies->GetList()) {
    CHECK(policy.is_dict())
        << "A dictionary is expected for each policy definition";
    base::Value::Dict* policy_dict = &policy.GetDict();

    VerifyJsonContents(policy_dict);
    PolicyMap entry_map = GetPolicyMapWithEntry(policy_dict);
    PolicyServiceImpl::IgnoreUserCloudPrecedencePolicies(&entry_map);
    policy_map.MergeFrom(entry_map);
  }

  FilterSensitivePolicies(&policy_map);

  bundle_ = std::move(bundle);
}

void LocalTestPolicyLoader::VerifyJsonContents(base::Value::Dict* policy_dict) {
  auto level_int = policy_dict->FindInt(kLevel);
  CHECK(level_int.has_value() &&
        *level_int <= static_cast<int>(PolicyLevel::POLICY_LEVEL_MAX))
      << "Invalid level found";
  auto scope_int = policy_dict->FindInt(kScope);
  CHECK(scope_int.has_value() &&
        *scope_int <= static_cast<int>(PolicyScope::POLICY_SCOPE_MAX))
      << "Invalid scope found";
  auto source_int = policy_dict->FindInt(kSource);
  CHECK(source_int.has_value() &&
        *source_int < static_cast<int>(PolicySource::POLICY_SOURCE_COUNT))
      << "Invalid source found";
  CHECK(policy_dict->FindString(kName)) << "Invalid name found";
  CHECK(policy_dict->contains(kValue)) << "Invalid value found";
}

void LocalTestPolicyLoader::ClearPolicies() {
  bundle_.Clear();
}

void LocalTestPolicyLoader::SetUserAffiliated(bool affiliated) {
  is_user_affiliated_ = affiliated;
}

}  // namespace policy
