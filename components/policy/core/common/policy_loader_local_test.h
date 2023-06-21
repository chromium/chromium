// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_LOCAL_TEST_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_LOCAL_TEST_H_

#include <string>

#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/policy_export.h"

namespace policy {

class PolicyBundle;

// Loads policies from policy testing page
class POLICY_EXPORT PolicyLoaderLocalTest {
 public:
  explicit PolicyLoaderLocalTest();

  ~PolicyLoaderLocalTest();

  PolicyBundle Load();

  // Sets policies from |policy_list_json| to |bundle_|,
  // |policy_list_json| should be a json string containing a list of
  // dictionaries. Each dictionary represents a policy and should
  // contain a key-value pair for the level(int), scope(int), source(int),
  // name and value of the policy.
  void SetPolicyListJson(const std::string& policy_list_json);

  void VerifyJsonContents(base::Value::Dict* policy_dict);

 private:
  PolicyBundle bundle_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_LOCAL_TEST_H_
