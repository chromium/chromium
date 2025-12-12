// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/pref_proxy_config_tracker_impl.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/buildflag.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/net_buildflags.h"
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
    for (net::ProxyConfigService::Observer& observer : observers_) {
      observer.OnProxyConfigChanged(config_, availability);
    }
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
  PrefProxyConfigTrackerImplTest() = default;

  // Initializes the proxy config service. The delegate config service has the
  // specified initial config availability.
  void InitConfigService(net::ProxyConfigService::ConfigAvailability
                             delegate_config_availability) {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    PrefProxyConfigTrackerImpl::RegisterPrefs(pref_service_->registry());
    net::ProxyConfig proxy_config;
    proxy_config.set_pac_url(GURL(kFixedPacUrl));
    fixed_config_ = net::ProxyConfigWithAnnotation(
        proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
    delegate_service_ =
        new TestProxyConfigService(fixed_config_, delegate_config_availability);
    proxy_config_tracker_ = std::make_unique<PrefProxyConfigTrackerImpl>(
        pref_service_.get(), base::SingleThreadTaskRunner::GetCurrentDefault());
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
  raw_ptr<TestProxyConfigService, DanglingUntriaged> delegate_service_;  // weak
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
  EXPECT_EQ(actual_config.value().proxy_rules().single_proxies.First(),
            net::ProxyUriToProxyChain("http://example.com:3128",
                                      net::ProxyServer::SCHEME_HTTP));

  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateAutoDetect()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.value().auto_detect());
}

#if BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
TEST_F(PrefProxyConfigTrackerImplTest, DynamicPrefOverridesSingleBracketedUri) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateFixedServers(
          "[http://example.com:3128]", std::string())));
  base::RunLoop().RunUntilIdle();

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_FALSE(actual_config.value().auto_detect());
  EXPECT_EQ(net::ProxyConfig::ProxyRules::Type::PROXY_LIST,
            actual_config.value().proxy_rules().type);
  EXPECT_EQ(actual_config.value().proxy_rules().single_proxies.First(),
            net::ProxyUriToProxyChain("http://example.com:3128",
                                      net::ProxyServer::SCHEME_HTTP));

  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateAutoDetect()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.value().auto_detect());
}

TEST_F(PrefProxyConfigTrackerImplTest, DynamicPrefOverridesMultiBracketedUris) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateFixedServers(
          "[https://foopy:443 https://hoopy:443]", std::string())));
  base::RunLoop().RunUntilIdle();

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_FALSE(actual_config.value().auto_detect());
  EXPECT_EQ(net::ProxyConfig::ProxyRules::Type::PROXY_LIST,
            actual_config.value().proxy_rules().type);

  // Build expected proxy chain for multi-proxy chain.
  net::ProxyChain expected_proxy_chain(
      {net::ProxyUriToProxyServer("https://foopy:443",
                                  net::ProxyServer::SCHEME_HTTPS),
       net::ProxyUriToProxyServer("https://hoopy:443",
                                  net::ProxyServer::SCHEME_HTTPS)});
  EXPECT_EQ(actual_config.value().proxy_rules().single_proxies.First(),
            expected_proxy_chain);

  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateAutoDetect()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.value().auto_detect());
}
#else
// Ensure that bracketed URIs are not parsed in release builds.
TEST_F(PrefProxyConfigTrackerImplTest,
       DynamicPrefOverridesBracketedUriNotValidInReleaseBuilds) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateFixedServers(
          "[http://example.com:3128]", std::string())));
  base::RunLoop().RunUntilIdle();

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_FALSE(actual_config.value().auto_detect());
  EXPECT_EQ(net::ProxyConfig::ProxyRules::Type::PROXY_LIST,
            actual_config.value().proxy_rules().type);
  // ProxyList should be empty b/c brackets in URI are invalid format.
  EXPECT_TRUE(actual_config.value().proxy_rules().single_proxies.IsEmpty());

  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateAutoDetect()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.value().auto_detect());
}
#endif

#if BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)
// Ensure that QUIC proxies are correctly parsed when the build flag for QUIC
// proxy support, `ENABLE_QUIC_PROXY_SUPPORT`, is enabled.
TEST_F(PrefProxyConfigTrackerImplTest,
       DynamicPrefOverridesQuicProxySupportValidWhenBuildFlagEnabled) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateFixedServers(
          "quic://foopy:443", std::string())));
  base::RunLoop().RunUntilIdle();

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_FALSE(actual_config.value().auto_detect());
  EXPECT_EQ(net::ProxyConfig::ProxyRules::Type::PROXY_LIST,
            actual_config.value().proxy_rules().type);
  EXPECT_EQ(actual_config.value().proxy_rules().single_proxies.First(),
            net::ProxyUriToProxyChain("quic://foopy:443",
                                      net::ProxyServer::SCHEME_HTTP,
                                      /*is_quic_allowed=*/true));

  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateAutoDetect()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.value().auto_detect());
}
#else
// Ensure that QUIC proxy support is not valid/parsed when the build flag for
// QUIC support, `ENABLE_QUIC_PROXY_SUPPORT`, is disabled.
TEST_F(PrefProxyConfigTrackerImplTest,
       DynamicPrefOverridesQuicProxySupportNotValidWhenBuildFlagDisabled) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateFixedServers(
          "quic://foopy:443", std::string())));
  base::RunLoop().RunUntilIdle();

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_FALSE(actual_config.value().auto_detect());
  EXPECT_EQ(net::ProxyConfig::ProxyRules::Type::PROXY_LIST,
            actual_config.value().proxy_rules().type);
  // ProxyList should be empty b/c brackets in URI are invalid format.
  EXPECT_TRUE(actual_config.value().proxy_rules().single_proxies.IsEmpty());

  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateAutoDetect()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.value().auto_detect());
}
#endif  // BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)

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
  EXPECT_CALL(observer,
              OnProxyConfigChanged(ProxyConfigMatches(config2), CONFIG_VALID))
      .Times(1);
  delegate_service_->SetProxyConfig(config2, CONFIG_VALID);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Override configuration, this should trigger a notification.
  net::ProxyConfig pref_config;
  pref_config.set_pac_url(GURL(kFixedPacUrl));

  EXPECT_CALL(observer, OnProxyConfigChanged(ProxyConfigMatches(pref_config),
                                             CONFIG_VALID))
      .Times(1);
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
  EXPECT_CALL(observer,
              OnProxyConfigChanged(ProxyConfigMatches(config3), CONFIG_VALID))
      .Times(1);
  pref_service_->RemoveManagedPref(proxy_config::prefs::kProxy);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Delegate service notifications should show up again.
  net::ProxyConfig config4;
  config4.proxy_rules().ParseFromString("socks:config4");
  EXPECT_CALL(observer,
              OnProxyConfigChanged(ProxyConfigMatches(config4), CONFIG_VALID))
      .Times(1);
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
                                   CONFIG_VALID))
      .Times(1);
  pref_service_->SetRecommendedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateAutoDetect()));
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.value().Equals(recommended_config));

  // Override in user prefs.
  EXPECT_CALL(observer, OnProxyConfigChanged(ProxyConfigMatches(user_config),
                                             CONFIG_VALID))
      .Times(1);
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
                                   CONFIG_VALID))
      .Times(1);
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

class PrefProxyConfigOverrideRulesTest : public PrefProxyConfigTrackerImplTest {
 public:
  void SetOverrideRules(const std::string& pref) {
    pref_service_->SetManagedPref(
        proxy_config::prefs::kProxyOverrideRules,
        std::make_unique<base::Value>(
            *base::JSONReader::Read(pref, base::JSON_ALLOW_TRAILING_COMMAS)));
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::ScopedFeatureList scoped_features_{kEnableProxyOverrideRules};
};

TEST_F(PrefProxyConfigOverrideRulesTest, DynamicPolicy) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  SetOverrideRules(
      R"([
             {
                 "DestinationMatchers": [
                     "https://some.app.com",
                     "https://other.app.com",
                 ],
                 "ExcludeDestinationMatchers": [
                     "https://exception.some.app.com",
                 ],
                 "ProxyList": [
                     "HTTPS proxy.app:443",
                     "DIRECT",
                 ],
                 "Conditions": [
                     {
                         "DnsProbe": {
                             "Host": "corp.ads",
                             "Result": "resolved",
                         },
                     }
                 ]
             }
         ])");

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));

  EXPECT_EQ(actual_config.value().proxy_override_rules().size(), 1u);

  const auto& rule = actual_config.value().proxy_override_rules().at(0);
  EXPECT_EQ(rule.destination_matchers.rules().size(), 3u);
  EXPECT_EQ(rule.destination_matchers.rules().at(0)->ToString(),
            "https://some.app.com");
  EXPECT_EQ(rule.destination_matchers.rules().at(1)->ToString(),
            "https://other.app.com");
  EXPECT_EQ(rule.destination_matchers.rules().at(2)->ToString(), "<-loopback>");

  EXPECT_EQ(rule.exclude_destination_matchers.rules().size(), 2u);
  EXPECT_EQ(rule.exclude_destination_matchers.rules().at(0)->ToString(),
            "https://exception.some.app.com");
  EXPECT_EQ(rule.exclude_destination_matchers.rules().at(1)->ToString(),
            "<-loopback>");

  EXPECT_EQ(rule.proxy_list.size(), 2u);
  EXPECT_EQ(rule.proxy_list.AllChains().at(0),
            net::PacResultElementToProxyChain("HTTPS proxy.app:443"));
  EXPECT_EQ(rule.proxy_list.AllChains().at(1),
            net::PacResultElementToProxyChain("DIRECT"));

  EXPECT_EQ(rule.dns_conditions.size(), 1u);
  EXPECT_EQ(rule.dns_conditions.at(0).host,
            url::SchemeHostPort(GURL("http://corp.ads")));
  EXPECT_TRUE(rule.dns_conditions.at(0).host.IsValid());
  EXPECT_EQ(rule.dns_conditions.at(0).result,
            net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::kResolved);

  // Setting the pref again in the same test scope validates the policy is
  // dynamic and that the retrieved config changes appropriately.
  SetOverrideRules(
      R"([
             {
                 "DestinationMatchers": [
                     "https://some.other.app.com",
                 ],
                 "ProxyList": [
                     "DIRECT",
                     "PROXY some.host:123",
                     "HTTPS proxy.app:443",
                     "DIRECT",
                 ],
                 "Conditions": [
                     {
                         "DnsProbe": {
                             "Host": "corp.ads",
                             "Result": "resolved",
                         },
                     },
                     {
                         "DnsProbe": {
                             "Host": "ads.corp",
                             "Result": "not_found",
                         },
                     }
                 ]
             },
             {
                 "DestinationMatchers": [
                     "https://some.special.app.com",
                 ],
                 "ExcludeDestinationMatchers": [
                     "https://exception.some.special.app.com",
                 ],
                 "ProxyList": [
                     "DIRECT",
                 ],
             }
        ])");

  net::ProxyConfigWithAnnotation updated_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&updated_config));

  EXPECT_EQ(updated_config.value().proxy_override_rules().size(), 2u);

  const auto& rule_0 = updated_config.value().proxy_override_rules().at(0);
  EXPECT_EQ(rule_0.destination_matchers.rules().size(), 2u);
  EXPECT_EQ(rule_0.destination_matchers.rules().at(0)->ToString(),
            "https://some.other.app.com");
  EXPECT_EQ(rule_0.destination_matchers.rules().at(1)->ToString(),
            "<-loopback>");

  EXPECT_EQ(rule_0.exclude_destination_matchers.rules().size(), 1u);
  EXPECT_EQ(rule_0.exclude_destination_matchers.rules().at(0)->ToString(),
            "<-loopback>");

  EXPECT_EQ(rule_0.proxy_list.size(), 4u);
  EXPECT_EQ(rule_0.proxy_list.AllChains().at(0),
            net::PacResultElementToProxyChain("DIRECT"));
  EXPECT_EQ(rule_0.proxy_list.AllChains().at(1),
            net::PacResultElementToProxyChain("PROXY some.host:123"));
  EXPECT_EQ(rule_0.proxy_list.AllChains().at(2),
            net::PacResultElementToProxyChain("HTTPS proxy.app:443"));
  EXPECT_EQ(rule_0.proxy_list.AllChains().at(3),
            net::PacResultElementToProxyChain("DIRECT"));

  EXPECT_EQ(rule_0.dns_conditions.size(), 2u);
  EXPECT_TRUE(rule_0.dns_conditions.at(0).host.IsValid());
  EXPECT_EQ(rule_0.dns_conditions.at(0).host,
            url::SchemeHostPort(GURL("http://corp.ads")));
  EXPECT_EQ(rule_0.dns_conditions.at(0).result,
            net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::kResolved);
  EXPECT_TRUE(rule_0.dns_conditions.at(1).host.IsValid());
  EXPECT_EQ(rule_0.dns_conditions.at(1).host,
            url::SchemeHostPort(GURL("http://ads.corp")));
  EXPECT_EQ(rule_0.dns_conditions.at(1).result,
            net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::kNotFound);

  const auto& rule_1 = updated_config.value().proxy_override_rules().at(1);
  EXPECT_EQ(rule_1.destination_matchers.rules().size(), 2u);
  EXPECT_EQ(rule_1.destination_matchers.rules().at(0)->ToString(),
            "https://some.special.app.com");
  EXPECT_EQ(rule_1.destination_matchers.rules().at(1)->ToString(),
            "<-loopback>");

  EXPECT_EQ(rule_1.exclude_destination_matchers.rules().size(), 2u);
  EXPECT_EQ(rule_1.exclude_destination_matchers.rules().at(0)->ToString(),
            "https://exception.some.special.app.com");
  EXPECT_EQ(rule_1.exclude_destination_matchers.rules().at(1)->ToString(),
            "<-loopback>");

  EXPECT_EQ(rule_1.proxy_list.size(), 1u);
  EXPECT_EQ(rule_1.proxy_list.AllChains().at(0),
            net::PacResultElementToProxyChain("DIRECT"));

  EXPECT_TRUE(rule_1.dns_conditions.empty());

  // Changing the `kProxy` pref should not change `kProxyOverrideRules`, and
  // `kProxyOverrideRules` being set shouldn't prevent a `kProxy` value from
  // being in the resulting config.
  auto previous_override_rules = updated_config.value().proxy_override_rules();
  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateFixedServers(
          "http://example.com:3128", std::string())));
  base::RunLoop().RunUntilIdle();

  net::ProxyConfigWithAnnotation fixed_servers_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&fixed_servers_config));

  EXPECT_EQ(fixed_servers_config.value().proxy_override_rules(),
            previous_override_rules);
  EXPECT_FALSE(fixed_servers_config.value().auto_detect());
  EXPECT_EQ(net::ProxyConfig::ProxyRules::Type::PROXY_LIST,
            fixed_servers_config.value().proxy_rules().type);
  EXPECT_EQ(fixed_servers_config.value().proxy_rules().single_proxies.First(),
            net::ProxyUriToProxyChain("http://example.com:3128",
                                      net::ProxyServer::SCHEME_HTTP));
}

TEST_F(PrefProxyConfigOverrideRulesTest, URLAndPacProxyList) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);

  // The first two entries of the "ProxyList" are ignored due to not being valid
  // PAC strings or URLs.
  SetOverrideRules(
      R"([
             {
                 "DestinationMatchers": [
                     "https://some.app.com",
                     "https://other.app.com",
                 ],
                 "ProxyList": [
                     "proxy://bad.value",
                     "some_random_bad_value",
                     "HTTPS proxy.app:443",
                     "https://other.app:344",
                     "DIRECT",
                 ],
                 "Conditions": [
                     {
                         "DnsProbe": {
                             "Host": "corp.ads",
                             "Result": "resolved",
                         },
                     }
                 ]
             }
         ])");

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));

  EXPECT_EQ(actual_config.value().proxy_override_rules().size(), 1u);

  const auto& rule = actual_config.value().proxy_override_rules().at(0);
  EXPECT_EQ(rule.destination_matchers.rules().size(), 3u);
  EXPECT_EQ(rule.destination_matchers.rules().at(0)->ToString(),
            "https://some.app.com");
  EXPECT_EQ(rule.destination_matchers.rules().at(1)->ToString(),
            "https://other.app.com");
  EXPECT_EQ(rule.destination_matchers.rules().at(2)->ToString(), "<-loopback>");

  EXPECT_EQ(rule.exclude_destination_matchers.rules().size(), 1u);
  EXPECT_EQ(rule.exclude_destination_matchers.rules().at(0)->ToString(),
            "<-loopback>");

  EXPECT_EQ(rule.proxy_list.size(), 3u);
  EXPECT_EQ(rule.proxy_list.AllChains().at(0),
            net::PacResultElementToProxyChain("HTTPS proxy.app:443"));
  EXPECT_EQ(rule.proxy_list.AllChains().at(1),
            net::PacResultElementToProxyChain("HTTPS other.app:344"));
  EXPECT_EQ(rule.proxy_list.AllChains().at(2),
            net::PacResultElementToProxyChain("DIRECT"));

  EXPECT_EQ(rule.dns_conditions.size(), 1u);
  EXPECT_TRUE(rule.dns_conditions.at(0).host.IsValid());
  EXPECT_EQ(rule.dns_conditions.at(0).host,
            url::SchemeHostPort(GURL("http://corp.ads")));
  EXPECT_EQ(rule.dns_conditions.at(0).result,
            net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::kResolved);
}

TEST_F(PrefProxyConfigOverrideRulesTest, DNSProbeHostValues) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);

  SetOverrideRules(
      R"([
             {
                 "DestinationMatchers": [
                     "https://some.app.com",
                 ],
                 "ProxyList": [
                     "https://other.app:344",
                 ],
                 "Conditions": [
                     {
                         "DnsProbe": {
                             "Host": "https://google.com:448",
                             "Result": "resolved",
                         },
                     },
                     {
                         "DnsProbe": {
                             "Host": "https://google.com",
                             "Result": "not_found",
                         },
                     },
                     {
                         "DnsProbe": {
                             "Host": "http://google.com",
                             "Result": "resolved",
                         },
                     },
                     {
                         "DnsProbe": {
                             "Host": "http://google.com:84",
                             "Result": "not_found",
                         },
                     },
                     {
                         "DnsProbe": {
                             "Host": "google.com:88",
                             "Result": "resolved",
                         },
                     },
                     {
                         "DnsProbe": {
                             "Host": "google.com",
                             "Result": "not_found",
                         },
                     }
                 ]
             }
         ])");

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));

  EXPECT_EQ(actual_config.value().proxy_override_rules().size(), 1u);

  const auto& rule = actual_config.value().proxy_override_rules().at(0);
  EXPECT_EQ(rule.destination_matchers.rules().size(), 2u);
  EXPECT_EQ(rule.destination_matchers.rules().at(0)->ToString(),
            "https://some.app.com");
  EXPECT_EQ(rule.destination_matchers.rules().at(1)->ToString(), "<-loopback>");

  EXPECT_EQ(rule.exclude_destination_matchers.rules().size(), 1u);
  EXPECT_EQ(rule.exclude_destination_matchers.rules().at(0)->ToString(),
            "<-loopback>");

  EXPECT_EQ(rule.proxy_list.size(), 1u);
  EXPECT_EQ(rule.proxy_list.AllChains().at(0),
            net::PacResultElementToProxyChain("HTTPS other.app:344"));

  EXPECT_EQ(rule.dns_conditions.size(), 6u);

  EXPECT_TRUE(rule.dns_conditions.at(0).host.IsValid());
  EXPECT_EQ(rule.dns_conditions.at(0).host,
            url::SchemeHostPort(GURL("https://google.com:448")));
  EXPECT_EQ(rule.dns_conditions.at(0).result,
            net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::kResolved);
  EXPECT_TRUE(rule.dns_conditions.at(1).host.IsValid());
  EXPECT_EQ(rule.dns_conditions.at(1).host,
            url::SchemeHostPort(GURL("https://google.com:443")));
  EXPECT_EQ(rule.dns_conditions.at(1).result,
            net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::kNotFound);
  EXPECT_TRUE(rule.dns_conditions.at(2).host.IsValid());
  EXPECT_EQ(rule.dns_conditions.at(2).host,
            url::SchemeHostPort(GURL("http://google.com:80")));
  EXPECT_EQ(rule.dns_conditions.at(2).result,
            net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::kResolved);
  EXPECT_TRUE(rule.dns_conditions.at(3).host.IsValid());
  EXPECT_EQ(rule.dns_conditions.at(3).host,
            url::SchemeHostPort(GURL("http://google.com:84")));
  EXPECT_EQ(rule.dns_conditions.at(3).result,
            net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::kNotFound);
  EXPECT_TRUE(rule.dns_conditions.at(4).host.IsValid());
  EXPECT_EQ(rule.dns_conditions.at(4).host,
            url::SchemeHostPort(GURL("http://google.com:88")));
  EXPECT_EQ(rule.dns_conditions.at(4).result,
            net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::kResolved);
  EXPECT_TRUE(rule.dns_conditions.at(5).host.IsValid());
  EXPECT_EQ(rule.dns_conditions.at(5).host,
            url::SchemeHostPort(GURL("http://google.com:80")));
  EXPECT_EQ(rule.dns_conditions.at(5).result,
            net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::kNotFound);
}

TEST_F(PrefProxyConfigOverrideRulesTest, NonListValues) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  for (const char* value : {"1234", "false", "null", "\"abce\""}) {
    SetOverrideRules(value);

    net::ProxyConfigWithAnnotation actual_config;
    EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
              proxy_config_service_->GetLatestProxyConfig(&actual_config));
    EXPECT_TRUE(actual_config.value().proxy_override_rules().empty());
  }
}

TEST_F(PrefProxyConfigOverrideRulesTest, InvalidTypesInList) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  SetOverrideRules(
      R"([
             1234,
             "abcd",
             false,
             null,
         ])");

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.value().proxy_override_rules().empty());
}

TEST_F(PrefProxyConfigOverrideRulesTest, InvalidDictsInList) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);

  // Only the first entry of the list should be kept in the resulting config.
  SetOverrideRules(
      R"([
             {
                 "DestinationMatchers": [
                     "https://some.app.com",
                     "https://other.app.com",
                 ],
                 "ExcludeDestinationMatchers": [
                     "https://exception.some.app.com",
                 ],
                 "ProxyList": [
                     "HTTPS proxy.app:443",
                     "DIRECT",
                 ],
                 "Conditions": [
                     {
                         "DnsProbe": {
                             "Host": "corp.ads",
                             "Result": "resolved",
                         },
                     }
                 ]
             },
             {
                 "DestinationMatchers": [
                     "https://some.app.com",
                     "https://other.app.com",
                 ],
                 "ExcludeDestinationMatchers": [
                     "https://exception.some.app.com",
                 ],
                 "Conditions": [
                     {
                         "DnsProbe": {
                             "Host": "corp.ads",
                             "Result": "resolved",
                         },
                     }
                 ]
             },
             {
                 "DestinationMatchers": [
                     "https://some.app.com",
                     "https://other.app.com",
                 ],
                 "ExcludeDestinationMatchers": [
                     "https://exception.some.app.com",
                 ],
                 "ProxyList": [
                     "HTTPS proxy.app:443",
                     1234,
                 ],
                 "Conditions": [
                     {
                         "DnsProbe": {
                             "Host": "corp.ads",
                             "Result": "resolved",
                         },
                     }
                 ]
             },
             {
                 "ExcludeDestinationMatchers": [
                     "https://exception.some.app.com",
                 ],
                 "ProxyList": [
                     "HTTPS proxy.app:443",
                     "DIRECT",
                 ],
                 "Conditions": [
                     {
                         "DnsProbe": {
                             "Host": "corp.ads",
                             "Result": "resolved",
                         },
                     }
                 ]
             },
             {
                 "DestinationMatchers": [
                     "https://some.app.com",
                     1234,
                 ],
                 "ExcludeDestinationMatchers": [
                     "https://exception.some.app.com",
                 ],
                 "ProxyList": [
                     "HTTPS proxy.app:443",
                     "DIRECT",
                 ],
                 "Conditions": [
                     {
                         "DnsProbe": {
                             "Host": "corp.ads",
                             "Result": "resolves",
                         },
                     }
                 ]
             },
             {
                 "DestinationMatchers": [
                     "https://some.app.com",
                     "https://other.app.com",
                 ],
                 "ExcludeDestinationMatchers": [
                     1234,
                 ],
                 "ProxyList": [
                     "HTTPS proxy.app:443",
                     "DIRECT",
                 ],
                 "Conditions": [
                     {
                         "DnsProbe": {
                             "Host": "corp.ads",
                             "Result": "resolved",
                         },
                     }
                 ]
             },
             {
                 "DestinationMatchers": [
                     "https://some.app.com",
                     "https://other.app.com",
                 ],
                 "ExcludeDestinationMatchers": [
                     "https://exception.some.app.com",
                 ],
                 "ProxyList": [
                     "HTTPS proxy.app:443",
                     "DIRECT",
                 ],
                 "Conditions": [
                     {}
                 ]
             },
             {
                 "DestinationMatchers": [
                     "https://some.app.com",
                     "https://other.app.com",
                 ],
                 "ExcludeDestinationMatchers": [
                     "https://exception.some.app.com",
                 ],
                 "ProxyList": [
                     "HTTPS proxy.app:443",
                     "DIRECT",
                 ],
                 "Conditions": [
                     {
                         "DnsProbe": {
                             "Result": "resolved",
                         },
                     }
                 ]
             },
             {
                 "DestinationMatchers": [
                     "https://some.app.com",
                     "https://other.app.com",
                 ],
                 "ExcludeDestinationMatchers": [
                     "https://exception.some.app.com",
                 ],
                 "ProxyList": [
                     "HTTPS proxy.app:443",
                     "DIRECT",
                 ],
                 "Conditions": [
                     {
                         "DnsProbe": {
                             "Host": "corp.ads",
                         },
                     }
                 ]
             },
             {
                 "DestinationMatchers": [
                     "https://some.app.com",
                     "https://other.app.com",
                 ],
                 "ExcludeDestinationMatchers": [
                     "https://exception.some.app.com",
                 ],
                 "ProxyList": [
                     "HTTPS proxy.app:443",
                     "DIRECT",
                 ],
                 "Conditions": [
                     {
                         "DnsProbe": {
                             "Host": "corp.ads",
                             "Result": 1234,
                         },
                     }
                 ]
             },
             {
                 "DestinationMatchers": [
                     "https://some.app.com",
                     "https://other.app.com",
                 ],
                 "ExcludeDestinationMatchers": [
                     "https://exception.some.app.com",
                 ],
                 "ProxyList": [
                     "HTTPS proxy.app:443",
                     "DIRECT",
                 ],
                 "Conditions": [
                     {
                         "DnsProbe": {
                             "Host": 1234,
                             "Result": "resolved",
                         },
                     }
                 ]
             },
             {
                 "DestinationMatchers": [
                     "https://some.app.com",
                     "https://other.app.com",
                 ],
                 "ExcludeDestinationMatchers": [
                     "https://exception.some.app.com",
                 ],
                 "ProxyList": [
                     "HTTPS proxy.app:443",
                     "DIRECT",
                 ],
                 "Conditions": [
                     {
                         "DnsProbe": {
                             "Host": "corp.ads",
                             "Result": "invalid_value",
                         },
                     }
                 ]
             },
         ])");

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_EQ(actual_config.value().proxy_override_rules().size(), 1u);

  const auto& rule = actual_config.value().proxy_override_rules().at(0);
  EXPECT_EQ(rule.destination_matchers.rules().size(), 3u);
  EXPECT_EQ(rule.destination_matchers.rules().at(0)->ToString(),
            "https://some.app.com");
  EXPECT_EQ(rule.destination_matchers.rules().at(1)->ToString(),
            "https://other.app.com");
  EXPECT_EQ(rule.destination_matchers.rules().at(2)->ToString(), "<-loopback>");

  EXPECT_EQ(rule.exclude_destination_matchers.rules().size(), 2u);
  EXPECT_EQ(rule.exclude_destination_matchers.rules().at(0)->ToString(),
            "https://exception.some.app.com");
  EXPECT_EQ(rule.exclude_destination_matchers.rules().at(1)->ToString(),
            "<-loopback>");

  EXPECT_EQ(rule.proxy_list.size(), 2u);
  EXPECT_EQ(rule.proxy_list.AllChains().at(0),
            net::PacResultElementToProxyChain("HTTPS proxy.app:443"));
  EXPECT_EQ(rule.proxy_list.AllChains().at(1),
            net::PacResultElementToProxyChain("DIRECT"));

  EXPECT_EQ(rule.dns_conditions.size(), 1u);
  EXPECT_TRUE(rule.dns_conditions.at(0).host.IsValid());
  EXPECT_EQ(rule.dns_conditions.at(0).host,
            url::SchemeHostPort(GURL("http://corp.ads")));
  EXPECT_EQ(rule.dns_conditions.at(0).result,
            net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::kResolved);
}

}  // namespace
