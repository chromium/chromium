// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_LOCAL_TEST_POLICY_LOADER_H_
#define COMPONENTS_POLICY_CORE_COMMON_LOCAL_TEST_POLICY_LOADER_H_

#include <string>

#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/policy_export.h"

namespace policy {

class PolicyBundle;

// Loads policies from policy testing page
class POLICY_EXPORT LocalTestPolicyLoader {
 public:
  explicit LocalTestPolicyLoader();

  ~LocalTestPolicyLoader();

  PolicyBundle Load();

  // Sets policies from |policy_list_json| to |bundle_|,
  // |policy_list_json| should be a json string containing a list of
  // dictionaries. Each dictionary represents a policy and should
  // contain a key-value pair for the level(int), scope(int), source(int),
  // name and value of the policy.
  void SetPolicyListJson(const std::string& policy_list_json);

  void VerifyJsonContents(base::Value::Dict* policy_dict);

  void ClearPolicies();

  void SetUserAffiliated(bool affiliated);

  const std::string& policies() const { return policies_; }

 private:
  bool is_user_affiliated_ = false;
  std::string policies_;

  PolicyBundle bundle_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_LOCAL_TEST_POLICY_LOADER_H_
