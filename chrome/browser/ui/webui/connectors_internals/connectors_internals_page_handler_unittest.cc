// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/connectors_internals/connectors_internals_page_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENTERPRISE_PROXY)
#include "chrome/browser/enterprise/net/enterprise_proxy_service_factory.h"
#include "components/enterprise/net/core/mock_enterprise_proxy_service.h"
#endif

namespace enterprise_connectors {

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

base::DictValue CreatePvdPolicy(const std::string& pvd_id) {
  base::DictValue policy;
  policy.Set("pvd_id", pvd_id);
  return policy;
}

class ConnectorsInternalsPageHandlerTest : public testing::Test {
 public:
  ConnectorsInternalsPageHandlerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
#if BUILDFLAG(ENTERPRISE_PROXY)
    EnterpriseProxyServiceFactory::GetInstance()->SetTestingFactory(
        &profile_,
        base::BindLambdaForTesting([](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              NiceMock<enterprise_net::MockEnterpriseProxyService>>();
        }));
#endif

    handler_ = std::make_unique<ConnectorsInternalsPageHandler>(
        page_handler_.BindNewPipeAndPassReceiver(), &profile_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  mojo::Remote<connectors_internals::mojom::PageHandler> page_handler_;
  std::unique_ptr<ConnectorsInternalsPageHandler> handler_;
};

TEST_F(ConnectorsInternalsPageHandlerTest, GetProvisioningDomainState) {
#if BUILDFLAG(ENTERPRISE_PROXY)
  auto* mock_service = static_cast<enterprise_net::MockEnterpriseProxyService*>(
      EnterpriseProxyServiceFactory::GetForProfile(&profile_));

  const char kDebugInfo[] = R"({
    "domains": [
      {
        "policy": {
          "pvd_id": "domain1.example.com",
          "endpoints": [
            {"url": "https://proxy1.example.com", "weight": 100}
          ]
        },
        "fetched_config": {
          "identifier": "domain1.example.com",
          "expires": "Wed, 21 Oct 2026 07:28:00 GMT",
          "routes": ["route1", "route2"]
        }
      }
    ]
  })";

  std::optional<base::DictValue> debug_info =
      base::JSONReader::ReadDict(kDebugInfo, base::JSON_PARSE_RFC);
  ASSERT_TRUE(debug_info.has_value());

  EXPECT_CALL(*mock_service, GetDebugInfo())
      .WillOnce(Return(std::move(debug_info).value()));
#endif

  base::test::TestFuture<
      connectors_internals::mojom::ProvisioningDomainStatePtr>
      future;
  page_handler_->GetProvisioningDomainState(future.GetCallback());
  auto state = future.Take();

#if BUILDFLAG(ENTERPRISE_PROXY)
  ASSERT_EQ(state->pvd_configs.size(), 1u);
  EXPECT_EQ(state->pvd_configs[0]->pvd_id, "domain1.example.com");
  ASSERT_TRUE(state->pvd_configs[0]->expiration_time.has_value());
  base::Time expected_time;
  ASSERT_TRUE(
      base::Time::FromString("Wed, 21 Oct 2026 07:28:00 GMT", &expected_time));
  EXPECT_EQ(state->pvd_configs[0]->expiration_time.value(), expected_time);
#else
  EXPECT_TRUE(state->pvd_configs.empty());
#endif
}

TEST_F(ConnectorsInternalsPageHandlerTest, RefreshProvisioningDomainConfigs) {
#if BUILDFLAG(ENTERPRISE_PROXY)
  auto* mock_service = static_cast<enterprise_net::MockEnterpriseProxyService*>(
      EnterpriseProxyServiceFactory::GetForProfile(&profile_));

  // Initially not in progress, then becomes in progress after
  // ForceRefreshAllConfigs.
  EXPECT_CALL(*mock_service, ForceRefreshAllConfigs()).Times(1);
  EXPECT_CALL(*mock_service, IsRefreshInProgress())
      .WillOnce(Return(false))
      .WillOnce(Return(true))
      .WillRepeatedly(Return(false));

  base::test::TestFuture<
      connectors_internals::mojom::ProvisioningDomainStatePtr>
      future;
  page_handler_->RefreshProvisioningDomainConfigs(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Simulate refresh completion.
  base::DictValue debug_info;
  base::ListValue domains;
  base::DictValue domain;
  domain.Set("policy", CreatePvdPolicy("refreshed_id"));
  domains.Append(std::move(domain));
  debug_info.Set("domains", std::move(domains));

  EXPECT_CALL(*mock_service, GetDebugInfo())
      .WillOnce(Return(std::move(debug_info)));

  mock_service->NotifyObservers();

  auto state = future.Take();
  ASSERT_EQ(state->pvd_configs.size(), 1u);
  EXPECT_EQ(state->pvd_configs[0]->pvd_id, "refreshed_id");
#else
  base::test::TestFuture<
      connectors_internals::mojom::ProvisioningDomainStatePtr>
      future;
  page_handler_->RefreshProvisioningDomainConfigs(future.GetCallback());
  auto state = future.Take();
  EXPECT_TRUE(state->pvd_configs.empty());
#endif
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(ConnectorsInternalsPageHandlerTest,
       GetDeviceTrustState_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndDisableFeature(kDeviceTrustConnectorAndroid);

  base::test::TestFuture<connectors_internals::mojom::DeviceTrustStatePtr>
      future;
  page_handler_->GetDeviceTrustState(future.GetCallback());
  auto state = future.Take();

  EXPECT_FALSE(state->is_enabled);
  EXPECT_TRUE(state->policy_enabled_levels.empty());
  EXPECT_EQ(
      state->key_info->is_key_manager_initialized,
      connectors_internals::mojom::KeyManagerInitializedValue::UNSUPPORTED);
}

TEST_F(ConnectorsInternalsPageHandlerTest, GetDeviceTrustState_NoService) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(kDeviceTrustConnectorAndroid);

  // No DeviceTrustService is created for testing profiles, so the handler is
  // expected to gracefully return an unsupported state.
  base::test::TestFuture<connectors_internals::mojom::DeviceTrustStatePtr>
      future;
  page_handler_->GetDeviceTrustState(future.GetCallback());
  auto state = future.Take();

  EXPECT_FALSE(state->is_enabled);
  EXPECT_TRUE(state->policy_enabled_levels.empty());
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

}  // namespace enterprise_connectors
