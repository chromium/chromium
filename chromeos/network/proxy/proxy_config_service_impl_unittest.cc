// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/proxy/proxy_config_service_impl.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/network/network_handler.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
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
  void SetUp() override {
    shill_clients::InitializeFakes();
    chromeos::NetworkHandler::Initialize();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    chromeos::NetworkHandler::Shutdown();
    shill_clients::Shutdown();
  }

 protected:
  base::test::TaskEnvironment environment_;
};

// By default, ProxyConfigServiceImpl should ignore the state of the nested
// ProxyConfigService.
TEST_F(ProxyConfigServiceImplTest, IgnoresNestedProxyConfigServiceByDefault) {
  TestingPrefServiceSimple profile_prefs;
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(profile_prefs.registry());
  TestingPrefServiceSimple local_state_prefs;

  net::ProxyConfig fixed_config;
  fixed_config.set_pac_url(GURL(kFixedPacUrl));
  std::unique_ptr<TestProxyConfigService> nested_service =
      std::make_unique<TestProxyConfigService>(
          fixed_config, net::ProxyConfigService::CONFIG_VALID);

  ProxyConfigServiceImpl proxy_tracker(&profile_prefs, &local_state_prefs,
                                       base::ThreadTaskRunnerHandle::Get());

  std::unique_ptr<net::ProxyConfigService> proxy_resolution_service =
      proxy_tracker.CreateTrackingProxyConfigService(std::move(nested_service));

  net::ProxyConfigWithAnnotation config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_resolution_service->GetLatestProxyConfig(&config));
  EXPECT_TRUE(config.value().Equals(net::ProxyConfig::CreateDirect()));

  environment_.RunUntilIdle();
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_resolution_service->GetLatestProxyConfig(&config));
  EXPECT_TRUE(config.value().Equals(net::ProxyConfig::CreateDirect()));

  proxy_tracker.DetachFromPrefService();
}

// Sets proxy_config::prefs::kUseSharedProxies to true, and makes sure the
// nested ProxyConfigService is used.
TEST_F(ProxyConfigServiceImplTest, UsesNestedProxyConfigService) {
  TestingPrefServiceSimple profile_prefs;
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(profile_prefs.registry());
  TestingPrefServiceSimple local_state_prefs;
  profile_prefs.SetUserPref(proxy_config::prefs::kUseSharedProxies,
                            std::make_unique<base::Value>(true));

  net::ProxyConfig fixed_config;
  fixed_config.set_pac_url(GURL(kFixedPacUrl));
  std::unique_ptr<TestProxyConfigService> nested_service =
      std::make_unique<TestProxyConfigService>(
          fixed_config, net::ProxyConfigService::CONFIG_VALID);

  ProxyConfigServiceImpl proxy_tracker(&profile_prefs, &local_state_prefs,
                                       base::ThreadTaskRunnerHandle::Get());

  std::unique_ptr<net::ProxyConfigService> proxy_resolution_service =
      proxy_tracker.CreateTrackingProxyConfigService(std::move(nested_service));

  net::ProxyConfigWithAnnotation config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_resolution_service->GetLatestProxyConfig(&config));
  EXPECT_TRUE(config.value().Equals(fixed_config));

  environment_.RunUntilIdle();
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_resolution_service->GetLatestProxyConfig(&config));
  EXPECT_TRUE(config.value().Equals(fixed_config));

  proxy_tracker.DetachFromPrefService();
}

}  // namespace chromeos
