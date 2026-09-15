// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/provisioning_domain_refresh_helper.h"

#include <utility>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENTERPRISE_PROXY)
#include "components/enterprise/net/core/mock_enterprise_proxy_service.h"
#endif

namespace enterprise_connectors {

#if BUILDFLAG(ENTERPRISE_PROXY)

namespace {

using ::testing::NiceMock;
using ::testing::Return;

base::DictValue CreatePvdPolicy(const std::string& pvd_id) {
  base::DictValue policy;
  policy.Set("pvd_id", pvd_id);
  return policy;
}

class ProvisioningDomainRefreshHelperTest : public testing::Test {
 public:
  ProvisioningDomainRefreshHelperTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ProvisioningDomainRefreshHelperTest, RefreshConfigs_NullService) {
  ProvisioningDomainRefreshHelper helper;
  base::test::TestFuture<
      connectors_internals::mojom::ProvisioningDomainStatePtr>
      future;
  helper.RefreshConfigs(nullptr, future.GetCallback());
  auto state = future.Take();
  EXPECT_TRUE(state->pvd_configs.empty());
}

TEST_F(ProvisioningDomainRefreshHelperTest, RefreshConfigs_Success) {
  NiceMock<enterprise_net::MockEnterpriseProxyService> mock_service;
  ProvisioningDomainRefreshHelper helper;

  EXPECT_CALL(mock_service, ForceRefreshAllConfigs()).Times(1);
  EXPECT_CALL(mock_service, IsRefreshInProgress())
      .WillOnce(Return(false))
      .WillOnce(Return(true))
      .WillRepeatedly(Return(false));

  base::test::TestFuture<
      connectors_internals::mojom::ProvisioningDomainStatePtr>
      future;
  helper.RefreshConfigs(&mock_service, future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  base::DictValue debug_info;
  base::ListValue domains;
  base::DictValue domain;
  domain.Set("policy", CreatePvdPolicy("refreshed_id"));
  domains.Append(std::move(domain));
  debug_info.Set("domains", std::move(domains));

  EXPECT_CALL(mock_service, GetDebugInfo())
      .WillOnce(Return(std::move(debug_info)));

  mock_service.NotifyObservers();

  auto state = future.Take();
  ASSERT_EQ(state->pvd_configs.size(), 1u);
  EXPECT_EQ(state->pvd_configs[0]->pvd_id, "refreshed_id");
}

TEST_F(ProvisioningDomainRefreshHelperTest, RefreshConfigs_Timeout) {
  NiceMock<enterprise_net::MockEnterpriseProxyService> mock_service;
  ProvisioningDomainRefreshHelper helper;

  EXPECT_CALL(mock_service, ForceRefreshAllConfigs()).Times(1);
  EXPECT_CALL(mock_service, IsRefreshInProgress())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));

  base::DictValue debug_info;
  EXPECT_CALL(mock_service, GetDebugInfo())
      .WillOnce(Return(std::move(debug_info)));

  base::test::TestFuture<
      connectors_internals::mojom::ProvisioningDomainStatePtr>
      future;
  helper.RefreshConfigs(&mock_service, future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  task_environment_.FastForwardBy(base::Seconds(15));

  EXPECT_TRUE(future.IsReady());
  auto state = future.Take();
  EXPECT_TRUE(state->pvd_configs.empty());
}

TEST_F(ProvisioningDomainRefreshHelperTest,
       RefreshConfigs_MultipleConcurrentCallers) {
  NiceMock<enterprise_net::MockEnterpriseProxyService> mock_service;
  ProvisioningDomainRefreshHelper helper;

  EXPECT_CALL(mock_service, ForceRefreshAllConfigs()).Times(1);
  EXPECT_CALL(mock_service, IsRefreshInProgress())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));

  base::test::TestFuture<
      connectors_internals::mojom::ProvisioningDomainStatePtr>
      future1;
  base::test::TestFuture<
      connectors_internals::mojom::ProvisioningDomainStatePtr>
      future2;
  helper.RefreshConfigs(&mock_service, future1.GetCallback());
  helper.RefreshConfigs(&mock_service, future2.GetCallback());

  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  base::DictValue debug_info;
  base::ListValue domains;
  base::DictValue domain;
  domain.Set("policy", CreatePvdPolicy("concurrent_id"));
  domains.Append(std::move(domain));
  debug_info.Set("domains", std::move(domains));

  EXPECT_CALL(mock_service, IsRefreshInProgress())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_service, GetDebugInfo())
      .WillOnce(Return(std::move(debug_info)));

  mock_service.NotifyObservers();

  auto state1 = future1.Take();
  auto state2 = future2.Take();
  ASSERT_EQ(state1->pvd_configs.size(), 1u);
  EXPECT_EQ(state1->pvd_configs[0]->pvd_id, "concurrent_id");
  ASSERT_EQ(state2->pvd_configs.size(), 1u);
  EXPECT_EQ(state2->pvd_configs[0]->pvd_id, "concurrent_id");
}

TEST_F(ProvisioningDomainRefreshHelperTest,
       BackgroundStatusChangeWithoutPendingRefresh_DoesNotFetchDebugInfo) {
  NiceMock<enterprise_net::MockEnterpriseProxyService> mock_service;
  ProvisioningDomainRefreshHelper helper;

  EXPECT_CALL(mock_service, IsRefreshInProgress())
      .WillRepeatedly(Return(false));
  // GetDebugInfo should not be called because there are no pending callbacks.
  EXPECT_CALL(mock_service, GetDebugInfo()).Times(0);

  mock_service.NotifyObservers();
}

}  // namespace

#endif  // BUILDFLAG(ENTERPRISE_PROXY)

}  // namespace enterprise_connectors

