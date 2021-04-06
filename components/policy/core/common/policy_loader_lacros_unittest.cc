// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_lacros.h"

#include <stdint.h>
#include <vector>

#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_delegate.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/configuration_policy_provider_test.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

std::vector<uint8_t> GetValidPolicyFetchResponse() {
  em::CloudPolicySettings policy_proto;
  em::PolicyData policy_data;
  policy_proto.mutable_homepagelocation()->set_value("http://chromium.org");
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

}  // namespace

// Test cases for lacros policy provider specific functionality.
class PolicyLoaderLacrosTest : public PolicyTestBase {
 protected:
  PolicyLoaderLacrosTest() = default;
  ~PolicyLoaderLacrosTest() override {}

  SchemaRegistry schema_registry_;
};

TEST_F(PolicyLoaderLacrosTest, BasicTest) {
  std::vector<uint8_t> data = GetValidPolicyFetchResponse();

  chromeos::LacrosChromeServiceImpl::DisableCrosapiForTests();
  chromeos::LacrosChromeServiceImpl lacros_chrome_service(/*delegate=*/nullptr);
  auto init_params = crosapi::mojom::BrowserInitParams::New();
  init_params->device_account_policy = data;
  lacros_chrome_service.SetInitParamsForTests(std::move(init_params));

  PolicyLoaderLacros loader(task_environment_.GetMainThreadTaskRunner());
  base::RunLoop().RunUntilIdle();
  EXPECT_GT(GetChromePolicyMap(loader.Load().get())->size(), (unsigned int)0);
}

TEST_F(PolicyLoaderLacrosTest, UpdateTest) {
  auto init_params = crosapi::mojom::BrowserInitParams::New();

  chromeos::LacrosChromeServiceImpl::DisableCrosapiForTests();
  chromeos::LacrosChromeServiceImpl lacros_chrome_service(/*delegate=*/nullptr);
  lacros_chrome_service.SetInitParamsForTests(std::move(init_params));

  PolicyLoaderLacros* loader =
      new PolicyLoaderLacros(task_environment_.GetMainThreadTaskRunner());
  AsyncPolicyProvider provider(&schema_registry_,
                               std::unique_ptr<AsyncPolicyLoader>(loader));
  provider.Init(&schema_registry_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetChromePolicyMap(loader->Load().get())->size(), (unsigned int)0);

  std::vector<uint8_t> data = GetValidPolicyFetchResponse();
  loader->NotifyPolicyUpdate(data);
  base::RunLoop().RunUntilIdle();
  EXPECT_GT(GetChromePolicyMap(loader->Load().get())->size(), (unsigned int)0);
  provider.Shutdown();
}

}  // namespace policy
