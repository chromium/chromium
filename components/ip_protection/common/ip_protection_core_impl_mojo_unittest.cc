// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_core_impl_mojo.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager.h"
#include "components/ip_protection/common/ip_protection_token_manager.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ip_protection {

namespace {

class FakeIpProtectionProxyConfigManager
    : public IpProtectionProxyConfigManager {
 public:
  bool IsProxyListAvailable() override { return false; }

  const std::vector<net::ProxyChain>& ProxyList() override { NOTREACHED(); }

  const std::string& CurrentGeo() override { return geo_id_; }

  void RequestRefreshProxyList() override {
    if (on_force_refresh_proxy_list_) {
      if (!geo_id_to_change_on_refresh_.empty()) {
        geo_id_ = geo_id_to_change_on_refresh_;
      }
      std::move(on_force_refresh_proxy_list_).Run();
    }
  }

  void SetOnRequestRefreshProxyList(
      base::OnceClosure on_force_refresh_proxy_list,
      std::string geo_id = "") {
    geo_id_to_change_on_refresh_ = geo_id;
    on_force_refresh_proxy_list_ = std::move(on_force_refresh_proxy_list);
  }

  void SetCurrentGeo(const std::string& geo_id) { geo_id_ = geo_id; }

 private:
  std::string geo_id_;
  std::string geo_id_to_change_on_refresh_;
  base::OnceClosure on_force_refresh_proxy_list_;
};

class IpProtectionCoreImplMojoTest : public testing::Test {
 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// When `kIpPrivacyIncludeOAuthTokenInGetProxyConfig` feature is enabled, the
// proxy list should be refreshed on `AuthTokensMayBeAvailable`.
TEST_F(IpProtectionCoreImplMojoTest,
       RefreshProxyListOnInvalidateTryAgainAfterTimeOnly) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy,
      {
          {net::features::kIpPrivacyIncludeOAuthTokenInGetProxyConfig.name,
           "true"},
      });

  auto ipp_proxy_config_manager =
      std::make_unique<FakeIpProtectionProxyConfigManager>();
  bool refresh_requested = false;
  ipp_proxy_config_manager->SetOnRequestRefreshProxyList(
      base::BindLambdaForTesting([&]() { refresh_requested = true; }));
  auto ip_protection_core = IpProtectionCoreImplMojo::CreateForTesting(
      /*masked_domain_list_manager=*/nullptr,
      std::move(ipp_proxy_config_manager),
      std::map<ProxyLayer, std::unique_ptr<IpProtectionTokenManager>>(),
      /*probabilistic_reveal_token_registry=*/nullptr,
      /*ipp_prt_manager=*/nullptr,
      /*is_ip_protection_enabled=*/true, /*ip_protection_incognito=*/true);

  ip_protection_core.AuthTokensMayBeAvailable();

  EXPECT_TRUE(refresh_requested);
}

TEST_F(IpProtectionCoreImplMojoTest, ChangeEnabledStatus) {
  auto ip_protection_core = IpProtectionCoreImplMojo::CreateForTesting(
      /*masked_domain_list_manager=*/nullptr,
      /*ip_protection_proxy_config_manager=*/nullptr,
      std::map<ProxyLayer, std::unique_ptr<IpProtectionTokenManager>>(),
      /*probabilistic_reveal_token_registry=*/nullptr,
      /*ipp_prt_manager=*/nullptr,
      /*is_ip_protection_enabled=*/false, /*ip_protection_incognito=*/true);
  EXPECT_FALSE(ip_protection_core.IsIpProtectionEnabled());

  ip_protection_core.SetIpProtectionEnabled(true);
  EXPECT_TRUE(ip_protection_core.IsIpProtectionEnabled());

  ip_protection_core.SetIpProtectionEnabled(false);
  EXPECT_FALSE(ip_protection_core.IsIpProtectionEnabled());
}

}  // namespace
}  // namespace ip_protection
