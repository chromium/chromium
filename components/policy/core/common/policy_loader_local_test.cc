// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_local_test.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_loader_common.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace policy {

PolicyLoaderLocalTest::PolicyLoaderLocalTest() = default;

PolicyLoaderLocalTest::~PolicyLoaderLocalTest() = default;

PolicyBundle PolicyLoaderLocalTest::Load() {
  return bundle_.Clone();
}

void PolicyLoaderLocalTest::SetPolicyListJson(
    const std::string& policy_list_json) {
  PolicyBundle bundle;

  auto policies = base::JSONReader::ReadAndReturnValueWithError(
      policy_list_json, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  CHECK(policies.has_value() && policies->is_list())
      << "List of policies expected";

  PolicyMap& policy_map =
      bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));

  for (auto& policy : policies->GetList()) {
    CHECK(policy.is_dict())
        << "A dictionary is expected for each policy definition";
    base::Value::Dict* policy_dict = &policy.GetDict();

    VerifyJsonContents(policy_dict);

    PolicyLevel level =
        static_cast<PolicyLevel>(*policy_dict->FindInt("level"));
    PolicyScope scope =
        static_cast<PolicyScope>(*policy_dict->FindInt("scope"));
    PolicySource source =
        static_cast<PolicySource>(*policy_dict->FindInt("source"));
    std::string name = *policy_dict->FindString("name");
    base::Value value = policy_dict->Find("value")->Clone();

    PolicyMap::Entry entry(level, scope, source, std::move(value), nullptr);
    // Policies are set to the |policy_map| this way so that combinations
    // of the same policy are merged properly
    PolicyMap entry_map = PolicyMap();
    entry_map.Set(name, std::move(entry));

    policy_map.MergePolicy(name, std::move(entry_map), false);
  }

  FilterSensitivePolicies(&policy_map);

  bundle_ = std::move(bundle);
}

void PolicyLoaderLocalTest::VerifyJsonContents(base::Value::Dict* policy_dict) {
  auto level_int = policy_dict->FindInt("level");
  CHECK(level_int.has_value() &&
        *level_int <= static_cast<int>(PolicyLevel::POLICY_LEVEL_MAX))
      << "Invalid level found";
  auto scope_int = policy_dict->FindInt("scope");
  CHECK(scope_int.has_value() &&
        *scope_int <= static_cast<int>(PolicyScope::POLICY_SCOPE_MAX))
      << "Invalid scope found";
  auto source_int = policy_dict->FindInt("source");
  CHECK(source_int.has_value() &&
        *source_int < static_cast<int>(PolicySource::POLICY_SOURCE_COUNT))
      << "Invalid source found";
  CHECK(policy_dict->FindString("name")) << "Invalid name found";
  CHECK(policy_dict->contains("value")) << "Invalid value found";
}

void PolicyLoaderLocalTest::ClearPolicies() {
  bundle_.Clear();
}

}  // namespace policy
