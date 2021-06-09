// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_lacros.h"

#include <stdint.h>
#include <vector>

#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/configuration_policy_provider_test.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

std::vector<uint8_t> GetValidPolicyFetchResponse(
    const em::CloudPolicySettings& policy_proto) {
  em::PolicyData policy_data;
  policy_proto.SerializeToString(policy_data.mutable_policy_value());
  policy_data.set_policy_type(dm_protocol::kChromeUserPolicyType);
  em::PolicyFetchResponse policy_response;
  policy_data.SerializeToString(policy_response.mutable_policy_data());
  std::vector<uint8_t> data;
  size_t size = policy_response.ByteSizeLong();
  data.resize(size);
  policy_response.SerializeToArray(data.data(), size);
  return data;
}

const PolicyMap* GetChromePolicyMap(PolicyBundle* bundle) {
  PolicyNamespace ns = PolicyNamespace(POLICY_DOMAIN_CHROME, std::string());
  return &(bundle->Get(ns));
}

std::vector<uint8_t> GetValidPolicyFetchResponseWithPerProfilePolicy() {
  em::CloudPolicySettings policy_proto;
  // HomepageLocation is a per_profile:True policy. See policy_templates.json
  // for details.
  policy_proto.mutable_homepagelocation()->set_value("http://chromium.org");
  return GetValidPolicyFetchResponse(policy_proto);
}

std::vector<uint8_t> GetValidPolicyFetchResponseWithSystemWidePolicy() {
  em::CloudPolicySettings policy_proto;
  // TaskManagerEndProcessEnabled is a per_profile:True policy. See
  // policy_templates.json for details.
  policy_proto.mutable_taskmanagerendprocessenabled()->set_value(false);
  return GetValidPolicyFetchResponse(policy_proto);
}

std::vector<uint8_t> GetValidPolicyFetchResponseWithAllPolicy() {
  em::CloudPolicySettings policy_proto;
  // TaskManagerEndProcessEnabled is a per_profile:True policy. See
  // policy_templates.json for details.
  policy_proto.mutable_taskmanagerendprocessenabled()->set_value(false);
  // HomepageLocation is a per_profile:True policy. See policy_templates.json
  // for details.
  policy_proto.mutable_homepagelocation()->set_value("http://chromium.org");
  return GetValidPolicyFetchResponse(policy_proto);
}

}  // namespace

// Test cases for lacros policy provider specific functionality.
class PolicyLoaderLacrosTest : public PolicyTestBase {
 protected:
  PolicyLoaderLacrosTest() = default;
  ~PolicyLoaderLacrosTest() override {}

  void SetPolicy(std::vector<uint8_t> data) {
    auto init_params = crosapi::mojom::BrowserInitParams::New();
    init_params->device_account_policy = data;
    chromeos::LacrosService::Get()->SetInitParamsForTests(
        std::move(init_params));
  }

  void SetSystemWidePolicy() {
    std::vector<uint8_t> data =
        GetValidPolicyFetchResponseWithSystemWidePolicy();
    system_wide_policies_set_ = true;
    SetPolicy(data);
  }

  void SetProfilePolicy() {
    std::vector<uint8_t> data =
        GetValidPolicyFetchResponseWithPerProfilePolicy();
    SetPolicy(data);
  }

  void SetAllPolicy() {
    std::vector<uint8_t> data = GetValidPolicyFetchResponseWithAllPolicy();
    system_wide_policies_set_ = true;
    SetPolicy(data);
  }

  void CheckOnlySystemWidePoliciesAreSet(PolicyBundle* bundle) {
    const PolicyMap* policy_map = GetChromePolicyMap(bundle);
    // Profile policies.
    EXPECT_EQ(nullptr, policy_map->GetValue(key::kHomepageLocation));
    EXPECT_EQ(nullptr, policy_map->GetValue(key::kAllowDinosaurEasterEgg));

    // System-wide policies.
    if (system_wide_policies_set_) {
      EXPECT_FALSE(
          policy_map->GetValue(key::kTaskManagerEndProcessEnabled)->GetBool());
    }
    // Enterprise default.
    EXPECT_FALSE(
        policy_map->GetValue(key::kPinUnlockAutosubmitEnabled)->GetBool());
  }

  SchemaRegistry schema_registry_;
  bool system_wide_policies_set_ = false;
  chromeos::ScopedLacrosServiceTestHelper test_helper_;
};

TEST_F(PolicyLoaderLacrosTest, BasicTest) {
  SetSystemWidePolicy();

  PolicyLoaderLacros loader(task_environment_.GetMainThreadTaskRunner());
  base::RunLoop().RunUntilIdle();
  CheckOnlySystemWidePoliciesAreSet(loader.Load().get());
}

TEST_F(PolicyLoaderLacrosTest, BasicTestPerProfile) {
  SetProfilePolicy();

  PolicyLoaderLacros loader(task_environment_.GetMainThreadTaskRunner());
  base::RunLoop().RunUntilIdle();
  CheckOnlySystemWidePoliciesAreSet(loader.Load().get());
}

TEST_F(PolicyLoaderLacrosTest, UpdateTest) {
  auto init_params = crosapi::mojom::BrowserInitParams::New();

  // chromeos::ScopedLacrosServiceTestHelper test_helper;
  chromeos::LacrosService::Get()->SetInitParamsForTests(std::move(init_params));

  PolicyLoaderLacros* loader =
      new PolicyLoaderLacros(task_environment_.GetMainThreadTaskRunner());
  AsyncPolicyProvider provider(&schema_registry_,
                               std::unique_ptr<AsyncPolicyLoader>(loader));
  provider.Init(&schema_registry_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetChromePolicyMap(loader->Load().get())->size(), (unsigned int)0);

  std::vector<uint8_t> data = GetValidPolicyFetchResponseWithSystemWidePolicy();
  system_wide_policies_set_ = true;
  loader->NotifyPolicyUpdate(data);
  base::RunLoop().RunUntilIdle();
  EXPECT_GT(GetChromePolicyMap(loader->Load().get())->size(),
            static_cast<unsigned int>(0));
  provider.Shutdown();
}

TEST_F(PolicyLoaderLacrosTest, EnterpriseDefaultsTest) {
  SetAllPolicy();

  PolicyLoaderLacros loader(task_environment_.GetMainThreadTaskRunner());
  base::RunLoop().RunUntilIdle();

  CheckOnlySystemWidePoliciesAreSet(loader.Load().get());
}

}  // namespace policy
