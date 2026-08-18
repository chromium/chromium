// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/connectors_internals/connectors_internals_page_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "base/json/json_reader.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

base::DictValue CreateFetchedConfig(const std::string& identifier) {
  base::DictValue config;
  config.Set("identifier", identifier);
  return config;
}

class ConnectorsInternalsPageHandlerTest : public testing::Test {
 public:
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

TEST_F(ConnectorsInternalsPageHandlerTest,
       GetProvisioningDomainState_NoDomains) {
#if BUILDFLAG(ENTERPRISE_PROXY)
  auto* mock_service = static_cast<enterprise_net::MockEnterpriseProxyService*>(
      EnterpriseProxyServiceFactory::GetForProfile(&profile_));
  base::DictValue debug_info;

  EXPECT_CALL(*mock_service, GetDebugInfo())
      .WillOnce(Return(std::move(debug_info)));
#endif

  base::test::TestFuture<
      connectors_internals::mojom::ProvisioningDomainStatePtr>
      future;
  page_handler_->GetProvisioningDomainState(future.GetCallback());
  auto state = future.Take();

  EXPECT_TRUE(state->pvd_configs.empty());
}

TEST_F(ConnectorsInternalsPageHandlerTest,
       GetProvisioningDomainState_EdgeCases) {
#if BUILDFLAG(ENTERPRISE_PROXY)
  auto* mock_service = static_cast<enterprise_net::MockEnterpriseProxyService*>(
      EnterpriseProxyServiceFactory::GetForProfile(&profile_));
  base::DictValue debug_info;
  base::ListValue domains;

  // Non-dict entry (should be skipped)
  domains.Append("not a dict");

  // No policy.pvd_id, fallback to fetched_config.identifier
  base::DictValue domain2;
  domain2.Set("fetched_config", CreateFetchedConfig("fallback_id_2"));
  domains.Append(std::move(domain2));

  // Empty policy.pvd_id, fallback to fetched_config.identifier
  base::DictValue domain3;
  domain3.Set("policy", CreatePvdPolicy(""));
  domain3.Set("fetched_config", CreateFetchedConfig("fallback_id_3"));
  domains.Append(std::move(domain3));

  // Neither available
  base::DictValue domain4;
  domains.Append(std::move(domain4));

  // Both available, prefers policy.pvd_id
  base::DictValue domain5;
  domain5.Set("policy", CreatePvdPolicy("preferred_id"));
  domain5.Set("fetched_config", CreateFetchedConfig("ignored_id"));
  domains.Append(std::move(domain5));

  debug_info.Set("domains", std::move(domains));

  EXPECT_CALL(*mock_service, GetDebugInfo())
      .WillOnce(Return(std::move(debug_info)));
#endif

  base::test::TestFuture<
      connectors_internals::mojom::ProvisioningDomainStatePtr>
      future;
  page_handler_->GetProvisioningDomainState(future.GetCallback());
  auto state = future.Take();

#if BUILDFLAG(ENTERPRISE_PROXY)
  ASSERT_EQ(state->pvd_configs.size(), 4u);
  EXPECT_EQ(state->pvd_configs[0]->pvd_id, "fallback_id_2");
  EXPECT_EQ(state->pvd_configs[1]->pvd_id, "fallback_id_3");
  EXPECT_EQ(state->pvd_configs[2]->pvd_id, "");
  EXPECT_EQ(state->pvd_configs[3]->pvd_id, "preferred_id");
#else
  EXPECT_TRUE(state->pvd_configs.empty());
#endif
}

}  // namespace

}  // namespace enterprise_connectors
