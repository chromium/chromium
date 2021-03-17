// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/pref_proxy_config_tracker_impl.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Mock;

namespace {

const char kFixedPacUrl[] = "http://chromium.org/fixed_pac_url";

// Testing proxy config service that allows us to fire notifications at will.
class TestProxyConfigService : public net::ProxyConfigService {
 public:
  TestProxyConfigService(const net::ProxyConfigWithAnnotation& config,
                         ConfigAvailability availability)
      : config_(config), availability_(availability) {}

  void SetProxyConfig(const net::ProxyConfig config,
                      ConfigAvailability availability) {
    config_ =
        net::ProxyConfigWithAnnotation(config, TRAFFIC_ANNOTATION_FOR_TESTS);
    availability_ = availability;
    for (net::ProxyConfigService::Observer& observer : observers_)
      observer.OnProxyConfigChanged(config_, availability);
  }

 private:
  void AddObserver(net::ProxyConfigService::Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(net::ProxyConfigService::Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  net::ProxyConfigService::ConfigAvailability GetLatestProxyConfig(
      net::ProxyConfigWithAnnotation* config) override {
    *config = config_;
    return availability_;
  }

  net::ProxyConfigWithAnnotation config_;
  ConfigAvailability availability_;
  base::ObserverList<net::ProxyConfigService::Observer, true>::Unchecked
      observers_;
};

// A mock observer for capturing callbacks.
class MockObserver : public net::ProxyConfigService::Observer {
 public:
  MOCK_METHOD2(OnProxyConfigChanged,
               void(const net::ProxyConfigWithAnnotation&,
                    net::ProxyConfigService::ConfigAvailability));
};

class PrefProxyConfigTrackerImplTest : public testing::Test {
 protected:
  PrefProxyConfigTrackerImplTest() {}

  // Initializes the proxy config service. The delegate config service has the
  // specified initial config availability.
  void InitConfigService(net::ProxyConfigService::ConfigAvailability
                             delegate_config_availability) {
    pref_service_.reset(new TestingPrefServiceSimple());
    PrefProxyConfigTrackerImpl::RegisterPrefs(pref_service_->registry());
    net::ProxyConfig proxy_config;
    proxy_config.set_pac_url(GURL(kFixedPacUrl));
    fixed_config_ = net::ProxyConfigWithAnnotation(
        proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
    delegate_service_ =
        new TestProxyConfigService(fixed_config_, delegate_config_availability);
    proxy_config_tracker_.reset(new PrefProxyConfigTrackerImpl(
        pref_service_.get(), base::ThreadTaskRunnerHandle::Get()));
    proxy_config_service_ =
        proxy_config_tracker_->CreateTrackingProxyConfigService(
            std::unique_ptr<net::ProxyConfigService>(delegate_service_));
  }

  ~PrefProxyConfigTrackerImplTest() override {
    proxy_config_tracker_->DetachFromPrefService();
    base::RunLoop().RunUntilIdle();
    proxy_config_tracker_.reset();
    proxy_config_service_.reset();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  TestProxyConfigService* delegate_service_; // weak
  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  net::ProxyConfigWithAnnotation fixed_config_;
  std::unique_ptr<PrefProxyConfigTrackerImpl> proxy_config_tracker_;
};

TEST_F(PrefProxyConfigTrackerImplTest, BaseConfiguration) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_EQ(GURL(kFixedPacUrl), actual_config.value().pac_url());
}

TEST_F(PrefProxyConfigTrackerImplTest, DynamicPrefOverrides) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateFixedServers(
          "http://example.com:3128", std::string())));
  base::RunLoop().RunUntilIdle();

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_FALSE(actual_config.value().auto_detect());
  EXPECT_EQ(net::ProxyConfig::ProxyRules::Type::PROXY_LIST,
            actual_config.value().proxy_rules().type);
  EXPECT_EQ(actual_config.value().proxy_rules().single_proxies.Get(),
            net::ProxyServer::FromURI("http://example.com:3128",
                                      net::ProxyServer::SCHEME_HTTP));

  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateAutoDetect()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.value().auto_detect());
}

// Compares proxy configurations, but allows different sources.
MATCHER_P(ProxyConfigMatches, config, "") {
  net::ProxyConfig reference(config);
  return reference.Equals(arg.value());
}

TEST_F(PrefProxyConfigTrackerImplTest, Observers) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  const net::ProxyConfigService::ConfigAvailability CONFIG_VALID =
      net::ProxyConfigService::CONFIG_VALID;
  MockObserver observer;
  proxy_config_service_->AddObserver(&observer);

  // Firing the observers in the delegate should trigger a notification.
  net::ProxyConfig config2;
  config2.set_auto_detect(true);
  EXPECT_CALL(observer, OnProxyConfigChanged(ProxyConfigMatches(config2),
                                             CONFIG_VALID)).Times(1);
  delegate_service_->SetProxyConfig(config2, CONFIG_VALID);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Override configuration, this should trigger a notification.
  net::ProxyConfig pref_config;
  pref_config.set_pac_url(GURL(kFixedPacUrl));

  EXPECT_CALL(observer, OnProxyConfigChanged(ProxyConfigMatches(pref_config),
                                             CONFIG_VALID)).Times(1);
  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(
          ProxyConfigDictionary::CreatePacScript(kFixedPacUrl, false)));
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Since there are pref overrides, delegate changes should be ignored.
  net::ProxyConfig config3;
  config3.proxy_rules().ParseFromString("http=config3:80");
  EXPECT_CALL(observer, OnProxyConfigChanged(_, _)).Times(0);
  net::ProxyConfig fixed_proxy_config = fixed_config_.value();
  fixed_proxy_config.set_auto_detect(true);
  fixed_config_ = net::ProxyConfigWithAnnotation(fixed_proxy_config,
                                                 TRAFFIC_ANNOTATION_FOR_TESTS);
  delegate_service_->SetProxyConfig(config3, CONFIG_VALID);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Clear the override should switch back to the fixed configuration.
  EXPECT_CALL(observer, OnProxyConfigChanged(ProxyConfigMatches(config3),
                                             CONFIG_VALID)).Times(1);
  pref_service_->RemoveManagedPref(proxy_config::prefs::kProxy);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Delegate service notifications should show up again.
  net::ProxyConfig config4;
  config4.proxy_rules().ParseFromString("socks:config4");
  EXPECT_CALL(observer, OnProxyConfigChanged(ProxyConfigMatches(config4),
                                             CONFIG_VALID)).Times(1);
  delegate_service_->SetProxyConfig(config4, CONFIG_VALID);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  proxy_config_service_->RemoveObserver(&observer);
}

TEST_F(PrefProxyConfigTrackerImplTest, Fallback) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  const net::ProxyConfigService::ConfigAvailability CONFIG_VALID =
      net::ProxyConfigService::CONFIG_VALID;
  MockObserver observer;
  net::ProxyConfigWithAnnotation actual_config;
  delegate_service_->SetProxyConfig(net::ProxyConfig::CreateDirect(),
                                    net::ProxyConfigService::CONFIG_UNSET);
  proxy_config_service_->AddObserver(&observer);

  // Prepare test data.
  net::ProxyConfig recommended_config = net::ProxyConfig::CreateAutoDetect();
  net::ProxyConfig user_config =
      net::ProxyConfig::CreateFromCustomPacURL(GURL(kFixedPacUrl));

  // Set a recommended pref.
  EXPECT_CALL(observer,
              OnProxyConfigChanged(ProxyConfigMatches(recommended_config),
                                   CONFIG_VALID)).Times(1);
  pref_service_->SetRecommendedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateAutoDetect()));
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.value().Equals(recommended_config));

  // Override in user prefs.
  EXPECT_CALL(observer,
              OnProxyConfigChanged(ProxyConfigMatches(user_config),
                                   CONFIG_VALID)).Times(1);
  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(
          ProxyConfigDictionary::CreatePacScript(kFixedPacUrl, false)));
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.value().Equals(user_config));

  // Go back to recommended pref.
  EXPECT_CALL(observer,
              OnProxyConfigChanged(ProxyConfigMatches(recommended_config),
                                   CONFIG_VALID)).Times(1);
  pref_service_->RemoveManagedPref(proxy_config::prefs::kProxy);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.value().Equals(recommended_config));

  proxy_config_service_->RemoveObserver(&observer);
}

TEST_F(PrefProxyConfigTrackerImplTest, ExplicitSystemSettings) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  pref_service_->SetRecommendedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateAutoDetect()));
  pref_service_->SetUserPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateSystem()));
  base::RunLoop().RunUntilIdle();

  // Test if we actually use the system setting, which is |kFixedPacUrl|.
  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_EQ(GURL(kFixedPacUrl), actual_config.value().pac_url());
}

// Test the case where the delegate service gets a config only after the service
// is created.
TEST_F(PrefProxyConfigTrackerImplTest, DelegateConfigServiceGetsConfigLate) {
  InitConfigService(net::ProxyConfigService::CONFIG_PENDING);

  testing::StrictMock<MockObserver> observer;
  proxy_config_service_->AddObserver(&observer);

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_PENDING,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));

  // When the delegate service gets the config, the other service should update
  // its observers.
  EXPECT_CALL(observer,
              OnProxyConfigChanged(ProxyConfigMatches(fixed_config_.value()),
                                   net::ProxyConfigService::CONFIG_VALID))
      .Times(1);
  delegate_service_->SetProxyConfig(fixed_config_.value(),
                                    net::ProxyConfigService::CONFIG_VALID);

  // Since no prefs were set, should just use the delegated config service's
  // settings.
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_EQ(GURL(kFixedPacUrl), actual_config.value().pac_url());

  proxy_config_service_->RemoveObserver(&observer);
}

}  // namespace
