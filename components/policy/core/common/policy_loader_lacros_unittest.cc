// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_lacros.h"

#include <stdint.h>
#include <vector>

#include "base/values.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "chromeos/startup/browser_init_params.h"
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

const PolicyMap& GetChromePolicyMap(const PolicyBundle& bundle) {
  PolicyNamespace ns = PolicyNamespace(POLICY_DOMAIN_CHROME, std::string());
  return bundle.Get(ns);
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
  ~PolicyLoaderLacrosTest() override = default;

  void SetPolicy() {
    std::vector<uint8_t> data = GetValidPolicyFetchResponseWithAllPolicy();
    auto init_params = crosapi::mojom::BrowserInitParams::New();
    init_params->device_account_policy = data;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
  }

  void CheckProfilePolicies(const PolicyMap& policy_map) const {
    if (per_profile_ == PolicyPerProfileFilter::kFalse) {
      EXPECT_EQ(nullptr, policy_map.GetValue(key::kHomepageLocation,
                                             base::Value::Type::STRING));
      EXPECT_EQ(nullptr, policy_map.GetValue(key::kAllowDinosaurEasterEgg,
                                             base::Value::Type::BOOLEAN));
    } else {
      EXPECT_EQ("http://chromium.org",
                policy_map
                    .GetValue(key::kHomepageLocation, base::Value::Type::STRING)
                    ->GetString());
      // Enterprise default.
      EXPECT_EQ(false, policy_map
                           .GetValue(key::kAllowDinosaurEasterEgg,
                                     base::Value::Type::BOOLEAN)
                           ->GetBool());
    }
  }

  void CheckSystemWidePolicies(const PolicyMap& policy_map) const {
    if (per_profile_ == PolicyPerProfileFilter::kTrue) {
      EXPECT_EQ(nullptr, policy_map.GetValue(key::kTaskManagerEndProcessEnabled,
                                             base::Value::Type::BOOLEAN));
      EXPECT_EQ(nullptr, policy_map.GetValue(key::kPinUnlockAutosubmitEnabled,
                                             base::Value::Type::BOOLEAN));
    } else {
      EXPECT_FALSE(policy_map
                       .GetValue(key::kTaskManagerEndProcessEnabled,
                                 base::Value::Type::BOOLEAN)
                       ->GetBool());
      // Enterprise default.
      EXPECT_FALSE(policy_map
                       .GetValue(key::kPinUnlockAutosubmitEnabled,
                                 base::Value::Type::BOOLEAN)
                       ->GetBool());
    }
  }

  void CheckCorrectPoliciesAreSet(const PolicyBundle& bundle) const {
    const PolicyMap& policy_map = GetChromePolicyMap(bundle);
    CheckProfilePolicies(policy_map);
    CheckSystemWidePolicies(policy_map);
  }

  void SwitchAndCheckLocalDeviceAccountUser(
      crosapi::mojom::SessionType session_type) {
    auto init_params = crosapi::mojom::BrowserInitParams::New();
    init_params->session_type = session_type;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
    EXPECT_TRUE(PolicyLoaderLacros::IsDeviceLocalAccountUser());
    EXPECT_TRUE(PolicyLoaderLacros::IsMainUserAffiliated());
  }

  SchemaRegistry schema_registry_;
  PolicyPerProfileFilter per_profile_ = PolicyPerProfileFilter::kFalse;
  chromeos::ScopedLacrosServiceTestHelper test_helper_;
};

TEST_F(PolicyLoaderLacrosTest, BasicTestSystemWidePolicies) {
  per_profile_ = PolicyPerProfileFilter::kFalse;
  SetPolicy();

  PolicyLoaderLacros loader(task_environment_.GetMainThreadTaskRunner(),
                            per_profile_);
  base::RunLoop().RunUntilIdle();
  CheckCorrectPoliciesAreSet(loader.Load());
}

TEST_F(PolicyLoaderLacrosTest, BasicTestProfilePolicies) {
  per_profile_ = PolicyPerProfileFilter::kTrue;
  SetPolicy();

  PolicyLoaderLacros loader(task_environment_.GetMainThreadTaskRunner(),
                            per_profile_);
  base::RunLoop().RunUntilIdle();
  CheckCorrectPoliciesAreSet(loader.Load());
}

TEST_F(PolicyLoaderLacrosTest, UpdateTestProfilePolicies) {
  per_profile_ = PolicyPerProfileFilter::kTrue;
  auto init_params = crosapi::mojom::BrowserInitParams::New();

  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));

  PolicyLoaderLacros* loader = new PolicyLoaderLacros(
      task_environment_.GetMainThreadTaskRunner(), per_profile_);
  AsyncPolicyProvider provider(&schema_registry_,
                               std::unique_ptr<AsyncPolicyLoader>(loader));
  provider.Init(&schema_registry_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetChromePolicyMap(loader->Load()).empty());

  std::vector<uint8_t> data = GetValidPolicyFetchResponseWithAllPolicy();
  loader->OnPolicyUpdated(data);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetChromePolicyMap(loader->Load()).empty());
  provider.Shutdown();
}

TEST_F(PolicyLoaderLacrosTest, UpdateTestSystemWidePolicies) {
  per_profile_ = PolicyPerProfileFilter::kFalse;
  auto init_params = crosapi::mojom::BrowserInitParams::New();

  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));

  PolicyLoaderLacros* loader = new PolicyLoaderLacros(
      task_environment_.GetMainThreadTaskRunner(), per_profile_);
  AsyncPolicyProvider provider(&schema_registry_,
                               std::unique_ptr<AsyncPolicyLoader>(loader));
  provider.Init(&schema_registry_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetChromePolicyMap(loader->Load()).empty());

  std::vector<uint8_t> data = GetValidPolicyFetchResponseWithAllPolicy();
  loader->OnPolicyUpdated(data);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetChromePolicyMap(loader->Load()).empty());
  provider.Shutdown();
}

TEST_F(PolicyLoaderLacrosTest, TwoLoaders) {
  auto init_params = crosapi::mojom::BrowserInitParams::New();

  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));

  PolicyLoaderLacros* system_wide_loader =
      new PolicyLoaderLacros(task_environment_.GetMainThreadTaskRunner(),
                             PolicyPerProfileFilter::kFalse);
  AsyncPolicyProvider system_wide_provider(
      &schema_registry_,
      std::unique_ptr<AsyncPolicyLoader>(system_wide_loader));
  system_wide_provider.Init(&schema_registry_);

  PolicyLoaderLacros* per_profile_loader =
      new PolicyLoaderLacros(task_environment_.GetMainThreadTaskRunner(),
                             PolicyPerProfileFilter::kTrue);
  AsyncPolicyProvider per_profile_provider(
      &schema_registry_,
      std::unique_ptr<AsyncPolicyLoader>(per_profile_loader));
  per_profile_provider.Init(&schema_registry_);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(GetChromePolicyMap(system_wide_loader->Load()).empty());
  EXPECT_TRUE(GetChromePolicyMap(per_profile_loader->Load()).empty());

  std::vector<uint8_t> data = GetValidPolicyFetchResponseWithAllPolicy();
  system_wide_loader->OnPolicyUpdated(data);
  per_profile_loader->OnPolicyUpdated(data);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetChromePolicyMap(system_wide_loader->Load()).empty());
  EXPECT_FALSE(GetChromePolicyMap(per_profile_loader->Load()).empty());
  system_wide_provider.Shutdown();
  per_profile_provider.Shutdown();
}

TEST_F(PolicyLoaderLacrosTest, ChildUsersNoEnterpriseDefaults) {
  // Prepare child policy (per_profile:False).
  per_profile_ = PolicyPerProfileFilter::kFalse;

  em::CloudPolicySettings policy_proto;
  policy_proto.mutable_lacrossecondaryprofilesallowed()->set_value(false);
  const std::vector<uint8_t> data = GetValidPolicyFetchResponse(policy_proto);

  // Setup child user session with the policy.
  auto init_params = crosapi::mojom::BrowserInitParams::New();
  init_params->session_type = crosapi::mojom::SessionType::kChildSession;
  init_params->device_account_policy = std::move(data);
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));

  // Load the policy.
  PolicyLoaderLacros loader(task_environment_.GetMainThreadTaskRunner(),
                            per_profile_);
  base::RunLoop().RunUntilIdle();
  PolicyBundle bundle = loader.Load();

  EXPECT_TRUE(PolicyLoaderLacros::IsMainUserManaged());
  EXPECT_FALSE(PolicyLoaderLacros::IsDeviceLocalAccountUser());
  EXPECT_FALSE(PolicyLoaderLacros::IsMainUserAffiliated());

  // Check that desired policy is set and enterprise defaults are not applied.
  const PolicyMap& policy_map = GetChromePolicyMap(bundle);
  EXPECT_EQ(1u, policy_map.size());

  const PolicyMap::Entry* entry =
      policy_map.Get(key::kLacrosSecondaryProfilesAllowed);
  ASSERT_TRUE(entry);
  EXPECT_FALSE(entry->value(base::Value::Type::BOOLEAN)->GetBool());
  EXPECT_EQ(policy::POLICY_SOURCE_CLOUD_FROM_ASH, entry->source);
}

TEST_F(PolicyLoaderLacrosTest, DeviceLocalAccountUsers) {
  SwitchAndCheckLocalDeviceAccountUser(
      crosapi::mojom::SessionType::kPublicSession);
  SwitchAndCheckLocalDeviceAccountUser(
      crosapi::mojom::SessionType::kWebKioskSession);
  SwitchAndCheckLocalDeviceAccountUser(
      crosapi::mojom::SessionType::kAppKioskSession);
}

TEST_F(PolicyLoaderLacrosTest, DeviceAffiliatedId) {
  const char kAffiliationId[] = "affiliation-id";
  auto init_params = crosapi::mojom::BrowserInitParams::New();
  init_params->device_properties = crosapi::mojom::DeviceProperties::New();
  init_params->device_properties->device_affiliation_ids = {kAffiliationId};
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
  EXPECT_EQ(1u, PolicyLoaderLacros::device_affiliation_ids().size());
  EXPECT_EQ(kAffiliationId, PolicyLoaderLacros::device_affiliation_ids()[0]);
}

TEST_F(PolicyLoaderLacrosTest, DeviceDMToken) {
  const char kDMToken[] = "dm-token";
  auto init_params = crosapi::mojom::BrowserInitParams::New();
  init_params->device_properties = crosapi::mojom::DeviceProperties::New();
  init_params->device_properties->device_dm_token = kDMToken;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
  EXPECT_EQ(kDMToken, PolicyLoaderLacros::device_dm_token());
}

}  // namespace policy
