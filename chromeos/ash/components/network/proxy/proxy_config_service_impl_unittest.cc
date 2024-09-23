// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/proxy/proxy_config_service_impl.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

const char kFixedPacUrl[] = "http://fixed/";

class TestProxyConfigService : public net::ProxyConfigService {
 public:
  TestProxyConfigService(const net::ProxyConfig& config,
                         ConfigAvailability availability)
      : config_(config), availability_(availability) {}

 private:
  void AddObserver(net::ProxyConfigService::Observer* observer) override {}
  void RemoveObserver(net::ProxyConfigService::Observer* observer) override {}

  net::ProxyConfigService::ConfigAvailability GetLatestProxyConfig(
      net::ProxyConfigWithAnnotation* config) override {
    *config =
        net::ProxyConfigWithAnnotation(config_, TRAFFIC_ANNOTATION_FOR_TESTS);
    return availability_;
  }

  net::ProxyConfig config_;
  ConfigAvailability availability_;
};

}  // namespace

class ProxyConfigServiceImplTest : public testing::Test {
 public:
  void SetUp() override {
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();

    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(profile_prefs_.registry());
    proxy_config_service_ = std::make_unique<ProxyConfigServiceImpl>(
        &profile_prefs_, &local_state_prefs_,
        base::SingleThreadTaskRunner::GetCurrentDefault());

    // Wait for network initialization events to propagate.
    environment_.RunUntilIdle();
  }

  void TearDown() override { proxy_config_service_->DetachFromPrefService(); }

  void CreateTrackingProxyConfigService(
      std::unique_ptr<TestProxyConfigService> nested_service) {
    proxy_resolution_service_ =
        proxy_config_service_->CreateTrackingProxyConfigService(
            std::move(nested_service));
    environment_.RunUntilIdle();
  }

  void DetermineEffectiveConfigFromDefaultNetwork() {
    proxy_config_service_->DetermineEffectiveConfigFromDefaultNetwork();
    environment_.RunUntilIdle();
  }

  net::ProxyConfigService::ConfigAvailability GetLatestProxyConfig(
      net::ProxyConfigWithAnnotation* config) {
    return proxy_resolution_service_->GetLatestProxyConfig(config);
  }

  void SetUseSharedProxies() {
    profile_prefs_.SetUserPref(::proxy_config::prefs::kUseSharedProxies,
                               std::make_unique<base::Value>(true));
    environment_.RunUntilIdle();
  }

  void SetCaptivePortalSignin() {
    profile_prefs_.SetUserPref(chromeos::prefs::kCaptivePortalSignin,
                               std::make_unique<base::Value>(true));
    environment_.RunUntilIdle();
  }

  void SetCaptivePortalAuthenticationIgnoresProxy() {
    profile_prefs_.SetUserPref(
        chromeos::prefs::kCaptivePortalAuthenticationIgnoresProxy,
        std::make_unique<base::Value>(false));
    environment_.RunUntilIdle();
  }

  void SetProxyPref() {
    base::Value::Dict fixed_config;
    fixed_config.Set("mode", "pac_script");
    fixed_config.Set("pac_url", kFixedPacUrl);
    profile_prefs_.SetUserPref(::proxy_config::prefs::kProxy,
                               std::move(fixed_config));
    environment_.RunUntilIdle();
  }

  net::ProxyConfig SetDefaultNetworkProxyConfig() {
    SetUseSharedProxies();
    const NetworkState* default_network =
        NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
    CHECK(default_network);
    proxy_config::SetProxyConfigForNetwork(
        ProxyConfigDictionary(ProxyConfigDictionary::CreateAutoDetect()),
        *default_network);
    environment_.RunUntilIdle();
    return net::ProxyConfig::CreateAutoDetect();
  }

  NetworkHandlerTestHelper* network_handler_test_helper() {
    return network_handler_test_helper_.get();
  }

 protected:
  base::test::TaskEnvironment environment_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple local_state_prefs_;
  std::unique_ptr<ProxyConfigServiceImpl> proxy_config_service_;
  std::unique_ptr<net::ProxyConfigService> proxy_resolution_service_;
};

TEST_F(ProxyConfigServiceImplTest, Default) {
  CreateTrackingProxyConfigService(nullptr);

  net::ProxyConfigWithAnnotation config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            GetLatestProxyConfig(&config));
  EXPECT_TRUE(config.value().Equals(net::ProxyConfig::CreateDirect()));
}

// By default, ProxyConfigServiceImpl should ignore the state of the nested
// ProxyConfigService.
TEST_F(ProxyConfigServiceImplTest, IgnoresNestedProxyConfigServiceByDefault) {
  auto fixed_config =
      net::ProxyConfig::CreateFromCustomPacURL(GURL(kFixedPacUrl));
  std::unique_ptr<TestProxyConfigService> nested_service =
      std::make_unique<TestProxyConfigService>(
          fixed_config, net::ProxyConfigService::CONFIG_VALID);

  CreateTrackingProxyConfigService(std::move(nested_service));

  net::ProxyConfigWithAnnotation config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            GetLatestProxyConfig(&config));
  EXPECT_TRUE(config.value().Equals(net::ProxyConfig::CreateDirect()));
}

// Sets proxy_config::prefs::kUseSharedProxies to true, and makes sure the
// nested ProxyConfigService is used.
TEST_F(ProxyConfigServiceImplTest, UsesNestedProxyConfigService) {
  SetUseSharedProxies();

  auto fixed_config =
      net::ProxyConfig::CreateFromCustomPacURL(GURL(kFixedPacUrl));
  std::unique_ptr<TestProxyConfigService> nested_service =
      std::make_unique<TestProxyConfigService>(
          fixed_config, net::ProxyConfigService::CONFIG_VALID);

  CreateTrackingProxyConfigService(std::move(nested_service));

  net::ProxyConfigWithAnnotation config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            GetLatestProxyConfig(&config));
  EXPECT_TRUE(config.value().Equals(fixed_config));
}

TEST_F(ProxyConfigServiceImplTest, DetermineEffectiveConfigFromDefaultNetwork) {
  CreateTrackingProxyConfigService(nullptr);

  // No proxy set
  DetermineEffectiveConfigFromDefaultNetwork();
  net::ProxyConfigWithAnnotation config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            GetLatestProxyConfig(&config));
  EXPECT_EQ(config.value().ToValue(),
            net::ProxyConfig::CreateDirect().ToValue());

  // Per-network proxy set
  net::ProxyConfig network_proxy_config = SetDefaultNetworkProxyConfig();
  DetermineEffectiveConfigFromDefaultNetwork();
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            GetLatestProxyConfig(&config));
  EXPECT_EQ(config.value().ToValue(), network_proxy_config.ToValue());
}

TEST_F(ProxyConfigServiceImplTest,
       DetermineEffectiveConfigFromDefaultNetworkAndPref) {
  CreateTrackingProxyConfigService(nullptr);

  // No proxy set
  DetermineEffectiveConfigFromDefaultNetwork();
  net::ProxyConfigWithAnnotation config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            GetLatestProxyConfig(&config));
  EXPECT_EQ(config.value().ToValue(),
            net::ProxyConfig::CreateDirect().ToValue());

  // Proxy pref set
  SetProxyPref();
  DetermineEffectiveConfigFromDefaultNetwork();
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            GetLatestProxyConfig(&config));
  EXPECT_EQ(
      config.value().ToValue(),
      net::ProxyConfig::CreateFromCustomPacURL(GURL(kFixedPacUrl)).ToValue());
}

class ProxyConfigServiceImplCaptivePortalPopupWindowTest
    : public ProxyConfigServiceImplTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        chromeos::features::kCaptivePortalPopupWindow);
    profile_prefs_.registry()->RegisterBooleanPref(
        chromeos::prefs::kCaptivePortalSignin, false);
    profile_prefs_.registry()->RegisterBooleanPref(
        chromeos::prefs::kCaptivePortalAuthenticationIgnoresProxy, true);
    ProxyConfigServiceImplTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ProxyConfigServiceImplCaptivePortalPopupWindowTest,
       DetermineEffectiveConfigFromDefaultNetworkAndPref) {
  CreateTrackingProxyConfigService(nullptr);

  // No proxy set
  DetermineEffectiveConfigFromDefaultNetwork();
  net::ProxyConfigWithAnnotation config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            GetLatestProxyConfig(&config));
  EXPECT_EQ(config.value().ToValue(),
            net::ProxyConfig::CreateDirect().ToValue());

  // Proxy pref set
  SetProxyPref();
  DetermineEffectiveConfigFromDefaultNetwork();
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            GetLatestProxyConfig(&config));
  EXPECT_EQ(
      config.value().ToValue(),
      net::ProxyConfig::CreateFromCustomPacURL(GURL(kFixedPacUrl)).ToValue());
}

TEST_F(ProxyConfigServiceImplCaptivePortalPopupWindowTest,
       NetworkPortalSignin) {
  SetCaptivePortalSignin();
  CreateTrackingProxyConfigService(nullptr);

  // Proxy pref set but ignored for captive portal signin.
  SetProxyPref();
  DetermineEffectiveConfigFromDefaultNetwork();
  net::ProxyConfigWithAnnotation config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            GetLatestProxyConfig(&config));
  EXPECT_EQ(config.value().ToValue(),
            net::ProxyConfig::CreateDirect().ToValue());
}

TEST_F(ProxyConfigServiceImplCaptivePortalPopupWindowTest,
       CaptivePortalAuthenticationIgnoresProxy) {
  SetCaptivePortalSignin();
  SetCaptivePortalAuthenticationIgnoresProxy();
  CreateTrackingProxyConfigService(nullptr);

  // Proxy pref set and not ignored for captive portal signin.
  SetProxyPref();
  DetermineEffectiveConfigFromDefaultNetwork();
  net::ProxyConfigWithAnnotation config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            GetLatestProxyConfig(&config));
  EXPECT_EQ(
      config.value().ToValue(),
      net::ProxyConfig::CreateFromCustomPacURL(GURL(kFixedPacUrl)).ToValue());
}

}  // namespace ash
