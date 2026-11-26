// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/pref_proxy_config_tracker_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/function_ref.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/buildflag.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
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

#if BUILDFLAG(ENTERPRISE_PROXY)
#include "components/enterprise/net/core/enterprise_proxy_service.h"
#include "components/enterprise/net/core/mock_enterprise_proxy_service.h"
#endif  // BUILDFLAG(ENTERPRISE_PROXY)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
#include "base/test/metrics/histogram_tester.h"
#include "components/enterprise/net/core/features.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/platform_management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)

using testing::_;
using testing::Mock;

#if BUILDFLAG(ENTERPRISE_PROXY)
using enterprise_net::MockEnterpriseProxyService;
#endif  // BUILDFLAG(ENTERPRISE_PROXY)

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
  PrefProxyConfigTrackerImplTest() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
    management_override_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::PlatformManagementService::GetInstance(),
            policy::EnterpriseManagementAuthority::NONE);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)
    ON_CALL(mock_policy_service_, GetPolicies(_))
        .WillByDefault(testing::ReturnRef(policy_map_));
  }

  // Initializes the proxy config service. The delegate config service has the
  // specified initial config availability.
  void InitConfigService(
      net::ProxyConfigService::ConfigAvailability delegate_config_availability,
      bool record_affiliation_metrics = false) {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    PrefProxyConfigTrackerImpl::RegisterPrefs(pref_service_->registry());
    net::ProxyConfig proxy_config;
    proxy_config.set_pac_url(GURL(kFixedPacUrl));
    fixed_config_ = net::ProxyConfigWithAnnotation(
        proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
    delegate_service_ =
        new TestProxyConfigService(fixed_config_, delegate_config_availability);
    proxy_config_tracker_ = std::make_unique<PrefProxyConfigTrackerImpl>(
        pref_service_.get(), base::SingleThreadTaskRunner::GetCurrentDefault(),
        policy_service_,
#if BUILDFLAG(ENTERPRISE_PROXY)
        enterprise_proxy_service_,
#endif  // BUILDFLAG(ENTERPRISE_PROXY)
        record_affiliation_metrics);
    proxy_config_service_ =
        proxy_config_tracker_->CreateTrackingProxyConfigService(
            std::unique_ptr<net::ProxyConfigService>(delegate_service_));
  }

  ~PrefProxyConfigTrackerImplTest() override {
    if (!proxy_config_tracker_) {
      return;
    }
    proxy_config_tracker_->DetachFromPrefService();
    base::RunLoop().RunUntilIdle();
    proxy_config_tracker_.reset();
    proxy_config_service_.reset();
  }

  void RunAndWaitForProxyConfigChange(base::FunctionRef<void()> action) {
    base::RunLoop run_loop;
    MockObserver observer;
    EXPECT_CALL(observer, OnProxyConfigChanged(testing::_, testing::_))
        .WillOnce([&] { run_loop.Quit(); });
    proxy_config_service_->AddObserver(&observer);
    action();
    run_loop.Run();
    proxy_config_service_->RemoveObserver(&observer);
  }

  // Sets `kProxy` to fixed servers (`proxy_uri`), verifies the parsed single
  // proxy chain (or empty list when `expected_chain` is `std::nullopt`), then
  // switches `kProxy` to auto-detect and verifies `auto_detect()`.
  void VerifyFixedServersThenAutoDetect(
      const std::string& proxy_uri,
      std::optional<net::ProxyChain> expected_chain) {
    InitConfigService(net::ProxyConfigService::CONFIG_VALID);
    RunAndWaitForProxyConfigChange([&] {
      pref_service_->SetManagedPref(
          proxy_config::prefs::kProxy,
          std::make_unique<base::Value>(
              ProxyConfigDictionary::CreateFixedServers(proxy_uri,
                                                        std::string())));
    });

    net::ProxyConfigWithAnnotation actual_config;
    EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
              proxy_config_service_->GetLatestProxyConfig(&actual_config));
    EXPECT_FALSE(actual_config.value().auto_detect());
    EXPECT_EQ(net::ProxyConfig::ProxyRules::Type::PROXY_LIST,
              actual_config.value().proxy_rules().type);
    if (expected_chain.has_value()) {
      EXPECT_EQ(actual_config.value().proxy_rules().single_proxies.First(),
                *expected_chain);
    } else {
      EXPECT_TRUE(actual_config.value().proxy_rules().single_proxies.IsEmpty());
    }

    RunAndWaitForProxyConfigChange([&] {
      pref_service_->SetManagedPref(
          proxy_config::prefs::kProxy,
          std::make_unique<base::Value>(
              ProxyConfigDictionary::CreateAutoDetect()));
    });

    EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
              proxy_config_service_->GetLatestProxyConfig(&actual_config));
    EXPECT_TRUE(actual_config.value().auto_detect());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  raw_ptr<TestProxyConfigService, DanglingUntriaged> delegate_service_;  // weak
  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  net::ProxyConfigWithAnnotation fixed_config_;
  std::unique_ptr<PrefProxyConfigTrackerImpl> proxy_config_tracker_;
#if BUILDFLAG(ENTERPRISE_PROXY)
  testing::NiceMock<MockEnterpriseProxyService> mock_enterprise_proxy_service_;
  raw_ptr<enterprise_net::EnterpriseProxyService> enterprise_proxy_service_ =
      &mock_enterprise_proxy_service_;
  void SetActiveDynamicRoutingConfig(
      const net::ProxyConfig::DynamicRoutingConfig& config) {
    EXPECT_CALL(mock_enterprise_proxy_service_, GetDynamicRoutingConfig())
        .WillRepeatedly(testing::Return(config));
    mock_enterprise_proxy_service_.NotifyObservers();
  }
#else
  void SetActiveDynamicRoutingConfig(
      const net::ProxyConfig::DynamicRoutingConfig& config) {}
#endif  // BUILDFLAG(ENTERPRISE_PROXY)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      management_override_;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)
  policy::PolicyMap policy_map_;
  testing::NiceMock<policy::MockPolicyService> mock_policy_service_;
  raw_ptr<policy::PolicyService> policy_service_ = &mock_policy_service_;
};

TEST_F(PrefProxyConfigTrackerImplTest, BaseConfiguration) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_EQ(GURL(kFixedPacUrl), actual_config.value().pac_url());
}

TEST_F(PrefProxyConfigTrackerImplTest, DynamicPrefOverrides) {
  VerifyFixedServersThenAutoDetect(
      "http://example.com:3128",
      net::ProxyUriToProxyChain("http://example.com:3128",
                                net::ProxyServer::SCHEME_HTTP));
}

#if BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
TEST_F(PrefProxyConfigTrackerImplTest, DynamicPrefOverridesSingleBracketedUri) {
  VerifyFixedServersThenAutoDetect(
      "[http://example.com:3128]",
      net::ProxyUriToProxyChain("http://example.com:3128",
                                net::ProxyServer::SCHEME_HTTP));
}

TEST_F(PrefProxyConfigTrackerImplTest, DynamicPrefOverridesMultiBracketedUris) {
  VerifyFixedServersThenAutoDetect(
      "[https://foopy:443 https://hoopy:443]",
      net::ProxyChain(
          {net::ProxyUriToProxyServer("https://foopy:443",
                                      net::ProxyServer::SCHEME_HTTPS),
           net::ProxyUriToProxyServer("https://hoopy:443",
                                      net::ProxyServer::SCHEME_HTTPS)}));
}
#else
// Ensure that bracketed URIs are not parsed in release builds.
TEST_F(PrefProxyConfigTrackerImplTest,
       DynamicPrefOverridesBracketedUriNotValidInReleaseBuilds) {
  VerifyFixedServersThenAutoDetect("[http://example.com:3128]", std::nullopt);
}
#endif

#if BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)
// Ensure that QUIC proxies are correctly parsed when the build flag for QUIC
// proxy support, `ENABLE_QUIC_PROXY_SUPPORT`, is enabled.
TEST_F(PrefProxyConfigTrackerImplTest,
       DynamicPrefOverridesQuicProxySupportValidWhenBuildFlagEnabled) {
  VerifyFixedServersThenAutoDetect(
      "quic://foopy:443", net::ProxyUriToProxyChain(
                              "quic://foopy:443", net::ProxyServer::SCHEME_HTTP,
                              /*is_quic_allowed=*/true));
}
#else
// Ensure that QUIC proxy support is not valid/parsed when the build flag for
// QUIC support, `ENABLE_QUIC_PROXY_SUPPORT`, is disabled.
TEST_F(PrefProxyConfigTrackerImplTest,
       DynamicPrefOverridesQuicProxySupportNotValidWhenBuildFlagDisabled) {
  VerifyFixedServersThenAutoDetect("quic://foopy:443", std::nullopt);
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

net::ProxyConfigWithAnnotation MakeConfigWithTestOverlays(
    net::ProxyConfig base_proxy = net::ProxyConfig()) {
  net::ProxyConfig::ProxyOverrideRule override_rule;
  override_rule.destination_matchers.AddRuleFromString("https://app.example");
  override_rule.proxy_list.AddProxyChain(net::ProxyUriToProxyChain(
      "https://por-proxy.example:443", net::ProxyServer::SCHEME_HTTPS));
  base_proxy.set_proxy_override_rules({override_rule});

  net::ProxyConfig::DynamicRoutingConfig dynamic_config;
  dynamic_config.is_update_in_progress = true;
  base_proxy.set_dynamic_routing_config(dynamic_config);
  return net::ProxyConfigWithAnnotation(base_proxy,
                                        TRAFFIC_ANNOTATION_FOR_TESTS);
}

net::ProxyConfig EvaluateEffectiveProxyConfig(
    ProxyPrefs::ConfigState pref_state,
    const net::ProxyConfigWithAnnotation& pref_config,
    net::ProxyConfigService::ConfigAvailability system_availability,
    const net::ProxyConfigWithAnnotation& system_config,
    bool ignore_fallback_config,
    net::ProxyConfigService::ConfigAvailability expected_availability,
    ProxyPrefs::ConfigState expected_state,
    bool expect_overlays) {
  ProxyPrefs::ConfigState effective_state;
  net::ProxyConfigWithAnnotation effective_config;
  EXPECT_EQ(expected_availability,
            PrefProxyConfigTrackerImpl::GetEffectiveProxyConfig(
                pref_state, pref_config, system_availability, system_config,
                ignore_fallback_config, &effective_state, &effective_config));
  EXPECT_EQ(effective_state, expected_state);
  EXPECT_EQ(effective_config.value().proxy_override_rules().size(),
            expect_overlays ? 1u : 0u);
  EXPECT_EQ(
      effective_config.value().dynamic_routing_config().is_update_in_progress,
      expect_overlays);
  return effective_config.value();
}

// Verifies `GetEffectiveProxyConfig()` overlay arbitration when
// `system_availability` is `CONFIG_UNSET` across both enabled and disabled
// states of `kProxyOverrideRulesFixUnsetSystemProxy`.
TEST_F(PrefProxyConfigTrackerImplTest,
       UnsetSystemProxyOverlayArbitrationByFeatureFlag) {
  const auto override_only_pref_config = MakeConfigWithTestOverlays();
  const auto fallback_pref_config = MakeConfigWithTestOverlays(
      net::ProxyConfig::CreateFromCustomPacURL(GURL(kFixedPacUrl)));
  const auto unset_system_config =
      net::ProxyConfigWithAnnotation::CreateDirect();
  const net::ProxyConfigWithAnnotation valid_system_config(
      net::ProxyConfig::CreateAutoDetect(), TRAFFIC_ANNOTATION_FOR_TESTS);

  // With `kProxyOverrideRulesFixUnsetSystemProxy` enabled (default):
  // 1) `CONFIG_POLICY_OVERRIDE` with `CONFIG_UNSET` system proxy attaches
  //    overlays onto `DIRECT`.
  net::ProxyConfig effective = EvaluateEffectiveProxyConfig(
      ProxyPrefs::CONFIG_POLICY_OVERRIDE, override_only_pref_config,
      net::ProxyConfigService::CONFIG_UNSET, unset_system_config,
      /*ignore_fallback_config=*/false, net::ProxyConfigService::CONFIG_VALID,
      ProxyPrefs::CONFIG_POLICY_OVERRIDE, /*expect_overlays=*/true);
  EXPECT_FALSE(effective.has_pac_url());
  EXPECT_TRUE(effective.proxy_rules().empty());

  // 2) `CONFIG_FALLBACK` with `ignore_fallback_config=true` preserves legacy
  //    behavior (retaining both the fallback PAC base and overlays).
  effective = EvaluateEffectiveProxyConfig(
      ProxyPrefs::CONFIG_FALLBACK, fallback_pref_config,
      net::ProxyConfigService::CONFIG_UNSET, unset_system_config,
      /*ignore_fallback_config=*/true, net::ProxyConfigService::CONFIG_VALID,
      ProxyPrefs::CONFIG_FALLBACK, /*expect_overlays=*/true);
  EXPECT_EQ(effective.pac_url(), GURL(kFixedPacUrl));

  // With `kProxyOverrideRulesFixUnsetSystemProxy` disabled:
  base::test::ScopedFeatureList disabled_feature;
  disabled_feature.InitAndDisableFeature(
      kProxyOverrideRulesFixUnsetSystemProxy);

  // 1) `CONFIG_POLICY_OVERRIDE` with `CONFIG_UNSET` system proxy falls back to
  //    pure `DIRECT` without overlays.
  effective = EvaluateEffectiveProxyConfig(
      ProxyPrefs::CONFIG_POLICY_OVERRIDE, override_only_pref_config,
      net::ProxyConfigService::CONFIG_UNSET, unset_system_config,
      /*ignore_fallback_config=*/false, net::ProxyConfigService::CONFIG_VALID,
      ProxyPrefs::CONFIG_POLICY_OVERRIDE, /*expect_overlays=*/false);
  EXPECT_TRUE(effective.Equals(net::ProxyConfig::CreateDirect()));

  // 2) `CONFIG_FALLBACK` with `ignore_fallback_config=true` also falls back to
  //    pure `DIRECT` without overlays, whereas `ignore_fallback_config=false`
  //    still retains the fallback config and its overlays.
  effective = EvaluateEffectiveProxyConfig(
      ProxyPrefs::CONFIG_FALLBACK, fallback_pref_config,
      net::ProxyConfigService::CONFIG_UNSET, unset_system_config,
      /*ignore_fallback_config=*/true, net::ProxyConfigService::CONFIG_VALID,
      ProxyPrefs::CONFIG_FALLBACK, /*expect_overlays=*/false);
  EXPECT_TRUE(effective.Equals(net::ProxyConfig::CreateDirect()));

  effective = EvaluateEffectiveProxyConfig(
      ProxyPrefs::CONFIG_FALLBACK, fallback_pref_config,
      net::ProxyConfigService::CONFIG_UNSET, unset_system_config,
      /*ignore_fallback_config=*/false, net::ProxyConfigService::CONFIG_VALID,
      ProxyPrefs::CONFIG_FALLBACK, /*expect_overlays=*/true);
  EXPECT_EQ(effective.pac_url(), GURL(kFixedPacUrl));

  // 3) When `system_availability` is `CONFIG_VALID`, overlays still attach onto
  //    `system_config` even with `kProxyOverrideRulesFixUnsetSystemProxy`
  //    disabled.
  effective = EvaluateEffectiveProxyConfig(
      ProxyPrefs::CONFIG_POLICY_OVERRIDE, override_only_pref_config,
      net::ProxyConfigService::CONFIG_VALID, valid_system_config,
      /*ignore_fallback_config=*/false, net::ProxyConfigService::CONFIG_VALID,
      ProxyPrefs::CONFIG_SYSTEM, /*expect_overlays=*/true);
  EXPECT_TRUE(effective.auto_detect());
}

// Verifies that `GetEffectiveProxyConfig()` ignores stale overlay rules inside
// `pref_config` when `pref_state` is `CONFIG_UNSET`, honoring the invariant
// that `pref_config` is only valid when `pref_state != CONFIG_UNSET`.
TEST_F(PrefProxyConfigTrackerImplTest,
       ConfigUnsetIgnoresStaleOverlaysInPrefConfig) {
  const net::ProxyConfigWithAnnotation system_config(
      net::ProxyConfig::CreateFromCustomPacURL(GURL(kFixedPacUrl)),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  net::ProxyConfig effective = EvaluateEffectiveProxyConfig(
      ProxyPrefs::CONFIG_UNSET, MakeConfigWithTestOverlays(),
      net::ProxyConfigService::CONFIG_VALID, system_config,
      /*ignore_fallback_config=*/false, net::ProxyConfigService::CONFIG_VALID,
      ProxyPrefs::CONFIG_SYSTEM, /*expect_overlays=*/false);
  EXPECT_EQ(effective.pac_url(), GURL(kFixedPacUrl));
}

// Verifies that when `system_availability` is `CONFIG_PENDING` (system proxy
// settings have not finished loading yet), Phase 2 overlay attachment keeps
// returning `CONFIG_PENDING` for both `CONFIG_POLICY_OVERRIDE` and
// `CONFIG_FALLBACK` with active `ProxyOverrideRules` / `DynamicRoutingConfig`
// overlays.
TEST_F(PrefProxyConfigTrackerImplTest,
       SystemPendingPreservedWithActiveOverlays) {
  const auto override_only_pref_config = MakeConfigWithTestOverlays();
  const auto fallback_pref_config = MakeConfigWithTestOverlays(
      net::ProxyConfig::CreateFromCustomPacURL(GURL(kFixedPacUrl)));
  const auto pending_system_config =
      net::ProxyConfigWithAnnotation::CreateDirect();

  EvaluateEffectiveProxyConfig(
      ProxyPrefs::CONFIG_POLICY_OVERRIDE, override_only_pref_config,
      net::ProxyConfigService::CONFIG_PENDING, pending_system_config,
      /*ignore_fallback_config=*/false, net::ProxyConfigService::CONFIG_PENDING,
      ProxyPrefs::CONFIG_SYSTEM, /*expect_overlays=*/true);

  for (bool ignore_fallback_config : {false, true}) {
    EvaluateEffectiveProxyConfig(
        ProxyPrefs::CONFIG_FALLBACK, fallback_pref_config,
        net::ProxyConfigService::CONFIG_PENDING, pending_system_config,
        ignore_fallback_config, net::ProxyConfigService::CONFIG_PENDING,
        ProxyPrefs::CONFIG_SYSTEM, /*expect_overlays=*/true);
  }
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
  void SetOverrideRulesInternal(const std::string& pref,
                                bool is_extension = false,
                                bool is_valid = true) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
    policy_map_.Set(policy::key::kEnableProxyOverrideRulesForAllUsers,
                    policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                    base::Value(1), nullptr);
    pref_service_->SetInteger(
        proxy_config::prefs::kEnableProxyOverrideRulesForAllUsers, 1);
#endif
    auto set_pref = [&] {
      auto val = std::make_unique<base::Value>(
          *base::JSONReader::Read(pref, base::JSON_ALLOW_TRAILING_COMMAS));
      if (is_extension) {
        pref_service_->SetExtensionPref(
            proxy_config::prefs::kProxyOverrideRules, std::move(val));
      } else {
        pref_service_->SetManagedPref(proxy_config::prefs::kProxyOverrideRules,
                                      std::move(val));
      }
    };

    if (is_valid) {
      RunAndWaitForProxyConfigChange(set_pref);
    } else {
      set_pref();
    }
  }

  void SetOverrideRules(const std::string& pref, bool is_valid = true) {
    SetOverrideRulesInternal(pref, /*is_extension=*/false, is_valid);
  }

  void SetExtensionOverrideRules(const std::string& pref,
                                 bool is_valid = true) {
    SetOverrideRulesInternal(pref, /*is_extension=*/true, is_valid);
  }

  static void ExpectMatcherRules(
      const net::ProxyHostMatchingRules& matchers,
      std::initializer_list<std::string_view> expected) {
    ASSERT_EQ(matchers.rules().size(), expected.size());
    size_t i = 0;
    for (std::string_view rule_str : expected) {
      EXPECT_EQ(matchers.rules().at(i++)->ToString(), rule_str);
    }
  }

  static void ExpectProxyChains(
      const net::ProxyList& proxy_list,
      std::initializer_list<std::string_view> expected_pac) {
    ASSERT_EQ(proxy_list.size(), expected_pac.size());
    size_t i = 0;
    for (std::string_view pac : expected_pac) {
      EXPECT_EQ(proxy_list.AllChains().at(i++),
                net::PacResultElementToProxyChain(pac));
    }
  }

  static void ExpectDnsCondition(
      const net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition& cond,
      std::string_view expected_url,
      net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::Result result) {
    EXPECT_TRUE(cond.host.IsValid());
    EXPECT_EQ(cond.host, url::SchemeHostPort(GURL(expected_url)));
    EXPECT_EQ(cond.result, result);
  }

  void VerifyOverrideRuleWithSystemAvailability(
      net::ProxyConfigService::ConfigAvailability system_availability,
      bool is_extension,
      std::string_view destination,
      std::string_view pac_proxy) {
    InitConfigService(system_availability);
    auto rule_dict =
        base::DictValue()
            .Set("DestinationMatchers", base::ListValue().Append(destination))
            .Set("ProxyList", base::ListValue().Append(pac_proxy));
    SetOverrideRulesInternal(
        *base::WriteJson(base::ListValue().Append(std::move(rule_dict))),
        is_extension);

    net::ProxyConfigWithAnnotation actual_config;
    EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
              proxy_config_service_->GetLatestProxyConfig(&actual_config));
    if (system_availability == net::ProxyConfigService::CONFIG_UNSET) {
      EXPECT_FALSE(actual_config.value().has_pac_url());
      EXPECT_TRUE(actual_config.value().proxy_rules().empty());
    } else {
      EXPECT_EQ(actual_config.value().pac_url(), GURL(kFixedPacUrl));
    }
    ASSERT_EQ(actual_config.value().proxy_override_rules().size(), 1u);
    const auto& rule = actual_config.value().proxy_override_rules().at(0);
    // AddRulesToSubtractImplicit() appends an implicit "<-loopback>" rule.
    ExpectMatcherRules(rule.destination_matchers, {destination, "<-loopback>"});
    ExpectProxyChains(rule.proxy_list, {pac_proxy});
  }

 private:
  base::test::ScopedFeatureList scoped_features_{kEnableProxyOverrideRules};
};

TEST_F(PrefProxyConfigOverrideRulesTest, DynamicPolicy) {
  using DnsCondition = net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition;
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  SetOverrideRules(R"([
    {
      "DestinationMatchers": ["https://some.app.com", "https://other.app.com"],
      "ExcludeDestinationMatchers": ["https://exception.some.app.com"],
      "ProxyList": ["HTTPS proxy.app:443", "DIRECT"],
      "Conditions": [{"DnsProbe": {"Host": "corp.ads", "Result": "resolved"}}]
    }
  ])");

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  ASSERT_EQ(actual_config.value().proxy_override_rules().size(), 1u);

  const auto& rule = actual_config.value().proxy_override_rules().at(0);
  ExpectMatcherRules(
      rule.destination_matchers,
      {"https://some.app.com", "https://other.app.com", "<-loopback>"});
  ExpectMatcherRules(rule.exclude_destination_matchers,
                     {"https://exception.some.app.com", "<-loopback>"});
  ExpectProxyChains(rule.proxy_list, {"HTTPS proxy.app:443", "DIRECT"});
  ASSERT_EQ(rule.dns_conditions.size(), 1u);
  ExpectDnsCondition(rule.dns_conditions.at(0), "http://corp.ads",
                     DnsCondition::kResolved);

  // Setting the pref again in the same test scope validates the policy is
  // dynamic and that the retrieved config changes appropriately.
  SetOverrideRules(R"([
    {
      "DestinationMatchers": ["https://some.other.app.com"],
      "ProxyList": [
        "DIRECT", "PROXY some.host:123", "HTTPS proxy.app:443", "DIRECT"
      ],
      "Conditions": [
        {"DnsProbe": {"Host": "corp.ads", "Result": "resolved"}},
        {"DnsProbe": {"Host": "ads.corp", "Result": "not_found"}}
      ]
    },
    {
      "DestinationMatchers": ["https://some.special.app.com"],
      "ExcludeDestinationMatchers": ["https://exception.some.special.app.com"],
      "ProxyList": ["DIRECT"]
    }
  ])");

  net::ProxyConfigWithAnnotation updated_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&updated_config));
  ASSERT_EQ(updated_config.value().proxy_override_rules().size(), 2u);

  const auto& rule_0 = updated_config.value().proxy_override_rules().at(0);
  ExpectMatcherRules(rule_0.destination_matchers,
                     {"https://some.other.app.com", "<-loopback>"});
  ExpectMatcherRules(rule_0.exclude_destination_matchers, {"<-loopback>"});
  ExpectProxyChains(rule_0.proxy_list, {"DIRECT", "PROXY some.host:123",
                                        "HTTPS proxy.app:443", "DIRECT"});
  ASSERT_EQ(rule_0.dns_conditions.size(), 2u);
  ExpectDnsCondition(rule_0.dns_conditions.at(0), "http://corp.ads",
                     DnsCondition::kResolved);
  ExpectDnsCondition(rule_0.dns_conditions.at(1), "http://ads.corp",
                     DnsCondition::kNotFound);

  const auto& rule_1 = updated_config.value().proxy_override_rules().at(1);
  ExpectMatcherRules(rule_1.destination_matchers,
                     {"https://some.special.app.com", "<-loopback>"});
  ExpectMatcherRules(rule_1.exclude_destination_matchers,
                     {"https://exception.some.special.app.com", "<-loopback>"});
  ExpectProxyChains(rule_1.proxy_list, {"DIRECT"});
  EXPECT_TRUE(rule_1.dns_conditions.empty());

  // Changing `kProxy` should preserve `kProxyOverrideRules` alongside the new
  // base `kProxy` settings.
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
  using DnsCondition = net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition;
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);

  // The first two entries of "ProxyList" are ignored due to not being valid PAC
  // strings or URLs.
  SetOverrideRules(R"([
    {
      "DestinationMatchers": ["https://some.app.com", "https://other.app.com"],
      "ProxyList": [
        "proxy://bad.value",
        "some_random_bad_value",
        "HTTPS proxy.app:443",
        "https://other.app:344",
        "DIRECT"
      ],
      "Conditions": [{"DnsProbe": {"Host": "corp.ads", "Result": "resolved"}}]
    }
  ])");

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  ASSERT_EQ(actual_config.value().proxy_override_rules().size(), 1u);

  const auto& rule = actual_config.value().proxy_override_rules().at(0);
  ExpectMatcherRules(
      rule.destination_matchers,
      {"https://some.app.com", "https://other.app.com", "<-loopback>"});
  ExpectMatcherRules(rule.exclude_destination_matchers, {"<-loopback>"});
  ExpectProxyChains(rule.proxy_list,
                    {"HTTPS proxy.app:443", "HTTPS other.app:344", "DIRECT"});
  ASSERT_EQ(rule.dns_conditions.size(), 1u);
  ExpectDnsCondition(rule.dns_conditions.at(0), "http://corp.ads",
                     DnsCondition::kResolved);
}

TEST_F(PrefProxyConfigOverrideRulesTest, IPAddressMatchers) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  SetOverrideRules(R"([
    {
      "DestinationMatchers": ["https://32.123.34.123", "35.234.543.12"],
      "ExcludeDestinationMatchers": ["12.345.678.90"],
      "ProxyList": ["HTTPS proxy.app:443"]
    }
  ])");

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  ASSERT_EQ(actual_config.value().proxy_override_rules().size(), 1u);

  const auto& rule = actual_config.value().proxy_override_rules()[0];
  EXPECT_TRUE(rule.MatchesDestination(GURL("https://32.123.34.123")));
  EXPECT_TRUE(rule.MatchesDestination(GURL("http://35.234.543.12")));
  EXPECT_FALSE(rule.MatchesDestination(GURL("http://12.345.678.90")));
}

TEST_F(PrefProxyConfigOverrideRulesTest, DNSProbeHostValues) {
  using DnsCondition = net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition;
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  SetOverrideRules(R"([
    {
      "DestinationMatchers": ["https://some.app.com"],
      "ProxyList": ["https://other.app:344"],
      "Conditions": [
        {"DnsProbe": {"Host": "https://google.com:448", "Result": "resolved"}},
        {"DnsProbe": {"Host": "https://google.com", "Result": "not_found"}},
        {"DnsProbe": {"Host": "http://google.com", "Result": "resolved"}},
        {"DnsProbe": {"Host": "http://google.com:84", "Result": "not_found"}},
        {"DnsProbe": {"Host": "google.com:88", "Result": "resolved"}},
        {"DnsProbe": {"Host": "google.com", "Result": "not_found"}}
      ]
    }
  ])");

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  ASSERT_EQ(actual_config.value().proxy_override_rules().size(), 1u);

  const auto& rule = actual_config.value().proxy_override_rules().at(0);
  ExpectMatcherRules(rule.destination_matchers,
                     {"https://some.app.com", "<-loopback>"});
  ExpectMatcherRules(rule.exclude_destination_matchers, {"<-loopback>"});
  ExpectProxyChains(rule.proxy_list, {"HTTPS other.app:344"});

  const struct {
    const char* expected_url;
    DnsCondition::Result expected_result;
  } kExpectedConditions[] = {
      {"https://google.com:448", DnsCondition::kResolved},
      {"https://google.com:443", DnsCondition::kNotFound},
      {"http://google.com:80", DnsCondition::kResolved},
      {"http://google.com:84", DnsCondition::kNotFound},
      {"http://google.com:88", DnsCondition::kResolved},
      {"http://google.com:80", DnsCondition::kNotFound},
  };
  ASSERT_EQ(rule.dns_conditions.size(), std::size(kExpectedConditions));
  size_t i = 0;
  for (const auto& expected : kExpectedConditions) {
    ExpectDnsCondition(rule.dns_conditions.at(i++), expected.expected_url,
                       expected.expected_result);
  }
}

TEST_F(PrefProxyConfigOverrideRulesTest, NonListValues) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  for (const char* value : {"1234", "false", "null", "\"abce\""}) {
    SetOverrideRules(value, /*is_valid=*/false);

    net::ProxyConfigWithAnnotation actual_config;
    EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
              proxy_config_service_->GetLatestProxyConfig(&actual_config));
    EXPECT_TRUE(actual_config.value().proxy_override_rules().empty());
  }
}

TEST_F(PrefProxyConfigOverrideRulesTest, InvalidTypesInList) {
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);
  SetOverrideRules(R"([1234, "abcd", false, null])", /*is_valid=*/false);

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.value().proxy_override_rules().empty());
}

TEST_F(PrefProxyConfigOverrideRulesTest, InvalidDictsInList) {
  using DnsCondition = net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition;
  InitConfigService(net::ProxyConfigService::CONFIG_VALID);

  base::DictValue valid_rule = base::JSONReader::Read(
                                   R"({
            "DestinationMatchers": [
              "https://some.app.com", "https://other.app.com"
            ],
            "ExcludeDestinationMatchers": ["https://exception.some.app.com"],
            "ProxyList": ["HTTPS proxy.app:443", "DIRECT"],
            "Conditions": [
              {"DnsProbe": {"Host": "corp.ads", "Result": "resolved"}}
            ]
          })",
                                   base::JSON_PARSE_RFC)
                                   ->GetDict()
                                   .Clone();

  base::ListValue rules;
  rules.Append(valid_rule.Clone());
  auto append_invalid = [&](base::FunctionRef<void(base::DictValue&)> mutate) {
    base::DictValue copy = valid_rule.Clone();
    mutate(copy);
    rules.Append(std::move(copy));
  };
  auto dns_probe = [](base::DictValue& r) -> base::DictValue& {
    return *(*r.FindList("Conditions"))[0].GetDict().FindDict("DnsProbe");
  };

  append_invalid([](base::DictValue& r) { r.Remove("ProxyList"); });
  append_invalid([](base::DictValue& r) {
    (*r.FindList("ProxyList"))[1] = base::Value(1234);
  });
  append_invalid([](base::DictValue& r) { r.Remove("DestinationMatchers"); });
  append_invalid([](base::DictValue& r) {
    (*r.FindList("DestinationMatchers"))[1] = base::Value(1234);
  });
  append_invalid([](base::DictValue& r) {
    (*r.FindList("ExcludeDestinationMatchers"))[0] = base::Value(1234);
  });
  append_invalid([](base::DictValue& r) {
    (*r.FindList("Conditions"))[0] = base::Value(base::DictValue());
  });
  append_invalid([&](base::DictValue& r) { dns_probe(r).Remove("Host"); });
  append_invalid([&](base::DictValue& r) { dns_probe(r).Remove("Result"); });
  append_invalid([&](base::DictValue& r) { dns_probe(r).Set("Result", 1234); });
  append_invalid([&](base::DictValue& r) { dns_probe(r).Set("Host", 1234); });
  append_invalid(
      [&](base::DictValue& r) { dns_probe(r).Set("Result", "invalid_value"); });

  // Only the first entry of `rules` should be kept in the resulting config.
  SetOverrideRules(*base::WriteJson(rules));

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  ASSERT_EQ(actual_config.value().proxy_override_rules().size(), 1u);

  const auto& rule = actual_config.value().proxy_override_rules().at(0);
  ExpectMatcherRules(
      rule.destination_matchers,
      {"https://some.app.com", "https://other.app.com", "<-loopback>"});
  ExpectMatcherRules(rule.exclude_destination_matchers,
                     {"https://exception.some.app.com", "<-loopback>"});
  ExpectProxyChains(rule.proxy_list, {"HTTPS proxy.app:443", "DIRECT"});
  ASSERT_EQ(rule.dns_conditions.size(), 1u);
  ExpectDnsCondition(rule.dns_conditions.at(0), "http://corp.ads",
                     DnsCondition::kResolved);
}

TEST_F(PrefProxyConfigOverrideRulesTest,
       PolicyOverrideRulesWhenSystemProxyUnset) {
  VerifyOverrideRuleWithSystemAvailability(
      net::ProxyConfigService::CONFIG_UNSET, /*is_extension=*/false,
      "https://managed.example.com", "HTTPS managed-proxy.example.com:443");
}

TEST_F(PrefProxyConfigOverrideRulesTest,
       PolicyOverrideRulesWhenSystemProxyValid) {
  VerifyOverrideRuleWithSystemAvailability(
      net::ProxyConfigService::CONFIG_VALID, /*is_extension=*/false,
      "https://managed.example.com", "HTTPS managed-proxy.example.com:443");
}

TEST_F(PrefProxyConfigOverrideRulesTest,
       ExtensionOverrideRulesWhenSystemProxyUnset) {
  VerifyOverrideRuleWithSystemAvailability(
      net::ProxyConfigService::CONFIG_UNSET, /*is_extension=*/true,
      "https://extension.example.com", "HTTPS ext-proxy.example.com:443");
}

TEST_F(PrefProxyConfigOverrideRulesTest,
       ExtensionOverrideRulesWhenSystemProxyValid) {
  VerifyOverrideRuleWithSystemAvailability(
      net::ProxyConfigService::CONFIG_VALID, /*is_extension=*/true,
      "https://extension.example.com", "HTTPS ext-proxy.example.com:443");
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
// Verifies that `PrefProxyConfigTrackerImpl` dynamically re-evaluates and
// applies or removes `ProxyOverrideRules` from the active proxy configuration
// when `kEnableProxyOverrideRulesForAllUsers` or user affiliation status in
// `PolicyService` changes at runtime (and on ChromeOS, verifies that override
// rules are always allowed even when unaffiliated).
TEST_F(PrefProxyConfigOverrideRulesTest,
       DynamicAffiliationAndEnableForAllUsers) {
  InitConfigService(net::ProxyConfigService::CONFIG_UNSET);

  policy::PolicyMap policy_map;
  policy_map.SetDeviceAffiliationIds({"device_id"});
  policy_map.SetUserAffiliationIds({"other_id"});
#if !BUILDFLAG(IS_CHROMEOS)
  policy_map.Set(policy::key::kEnableProxyOverrideRulesForAllUsers,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
#endif
  ON_CALL(mock_policy_service_, GetPolicies(_))
      .WillByDefault(testing::ReturnRef(policy_map));

  // On Linux/Mac/Win/Android, machine-scoped
  // `EnableProxyOverrideRulesForAllUsers` = 1 allows extension rules; on
  // ChromeOS, override rules are always allowed even when unaffiliated.
  SetExtensionOverrideRules(
      R"([
             {
                 "DestinationMatchers": [
                     "https://extension.example.com",
                 ],
                 "ProxyList": [
                     "HTTPS ext-proxy.example.com:443",
                 ]
             }
         ])");

  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_EQ(actual_config.value().proxy_override_rules().size(), 1u);

#if !BUILDFLAG(IS_CHROMEOS)
  // Switching `EnableProxyOverrideRulesForAllUsers` = 1 from machine scope to
  // user scope must remove the override rules even while the pref stays 1.
  RunAndWaitForProxyConfigChange([&] {
    policy_map.Set(policy::key::kEnableProxyOverrideRulesForAllUsers,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
    proxy_config_tracker_->OnProviderUpdatePropagated(nullptr);
  });
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.value().proxy_override_rules().empty());

  // Restoring machine scope re-applies the override rules, and setting it to 0
  // at machine scope removes them again.
  RunAndWaitForProxyConfigChange([&] {
    policy_map.Set(policy::key::kEnableProxyOverrideRulesForAllUsers,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                   policy::POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
    proxy_config_tracker_->OnProviderUpdatePropagated(nullptr);
  });
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_EQ(actual_config.value().proxy_override_rules().size(), 1u);

  RunAndWaitForProxyConfigChange([&] {
    policy_map.Set(policy::key::kEnableProxyOverrideRulesForAllUsers,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                   policy::POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
    pref_service_->SetInteger(
        proxy_config::prefs::kEnableProxyOverrideRulesForAllUsers, 0);
    proxy_config_tracker_->OnProviderUpdatePropagated(nullptr);
  });
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.value().proxy_override_rules().empty());

  // Dynamically becoming affiliated via a PolicyService update should restore
  // the override rules even while kEnableProxyOverrideRulesForAllUsers is 0.
  RunAndWaitForProxyConfigChange([&] {
    policy_map.SetUserAffiliationIds({"device_id"});
    proxy_config_tracker_->OnProviderUpdatePropagated(nullptr);
  });
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_EQ(actual_config.value().proxy_override_rules().size(), 1u);

  // Dynamically becoming unaffiliated again should remove the override rules.
  RunAndWaitForProxyConfigChange([&] {
    policy_map.SetUserAffiliationIds({"other_id"});
    proxy_config_tracker_->OnProviderUpdatePropagated(nullptr);
  });
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.value().proxy_override_rules().empty());
#endif  // !BUILDFLAG(IS_CHROMEOS)
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
class PrefProxyConfigOverrideRulesAffiliationMetricsTest
    : public PrefProxyConfigOverrideRulesTest {
 protected:
  static constexpr char kPolicyHistogram[] =
      "Enterprise.ProxyOverrideRules.PlatformAffiliationImpact.Policy";
  static constexpr char kExtensionHistogram[] =
      "Enterprise.ProxyOverrideRules.PlatformAffiliationImpact.Extension";
  static constexpr char kValidRuleJson[] = R"([
    {
      "DestinationMatchers": ["https://app.example.com"],
      "ProxyList": ["HTTPS proxy.example.com:443"]
    }
  ])";

  PrefProxyConfigOverrideRulesAffiliationMetricsTest() {
    affiliation_features_.InitAndDisableFeature(
        enterprise_net::kEnforceProxyAffiliationOnPlatformManagedDevices);
    ON_CALL(mock_policy_service_,
            IsInitializationComplete(policy::POLICY_DOMAIN_CHROME))
        .WillByDefault([this](policy::PolicyDomain) {
          return policy_service_initialized_;
        });
  }

  void InitMetricsTracker(bool record_affiliation_metrics = true,
                          bool policy_service_initialized = true) {
    if (proxy_config_tracker_) {
      proxy_config_tracker_->DetachFromPrefService();
      proxy_config_tracker_.reset();
      proxy_config_service_.reset();
    }
    policy_service_initialized_ = policy_service_initialized;
    InitConfigService(net::ProxyConfigService::CONFIG_UNSET,
                      record_affiliation_metrics);
  }

  void RecreateTrackerPreservingPrefs(bool record_affiliation_metrics = true,
                                      bool policy_service_initialized = true) {
    if (proxy_config_tracker_) {
      proxy_config_tracker_->DetachFromPrefService();
      proxy_config_tracker_.reset();
      proxy_config_service_.reset();
    }
    policy_service_initialized_ = policy_service_initialized;
    delegate_service_ = new TestProxyConfigService(
        fixed_config_, net::ProxyConfigService::CONFIG_UNSET);
    proxy_config_tracker_ = std::make_unique<PrefProxyConfigTrackerImpl>(
        pref_service_.get(), base::SingleThreadTaskRunner::GetCurrentDefault(),
        policy_service_,
#if BUILDFLAG(ENTERPRISE_PROXY)
        enterprise_proxy_service_,
#endif  // BUILDFLAG(ENTERPRISE_PROXY)
        record_affiliation_metrics);
    proxy_config_service_ =
        proxy_config_tracker_->CreateTrackingProxyConfigService(
            std::unique_ptr<net::ProxyConfigService>(delegate_service_));
  }

  void SetPlatformManaged(bool managed) {
    management_override_.reset();
    management_override_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::PlatformManagementService::GetInstance(),
            managed ? policy::EnterpriseManagementAuthority::CLOUD
                    : policy::EnterpriseManagementAuthority::NONE);
  }

  static std::unique_ptr<base::Value> ParseValidOverrideRules() {
    return std::make_unique<base::Value>(
        *base::JSONReader::Read(kValidRuleJson, base::JSON_PARSE_RFC));
  }

  void SetManagedOverrideRules(policy::PolicyScope scope) {
    pref_service_->SetInteger(proxy_config::prefs::kProxyOverrideRulesScope,
                              scope);
    pref_service_->SetManagedPref(proxy_config::prefs::kProxyOverrideRules,
                                  ParseValidOverrideRules());
  }

  void SetExtensionControlledOverrideRules() {
    pref_service_->SetExtensionPref(proxy_config::prefs::kProxyOverrideRules,
                                    ParseValidOverrideRules());
  }

  void SetForAllUsers(
      int value,
      policy::PolicyScope scope = policy::POLICY_SCOPE_MACHINE) {
    policy_map_.Set(policy::key::kEnableProxyOverrideRulesForAllUsers,
                    policy::POLICY_LEVEL_MANDATORY, scope,
                    policy::POLICY_SOURCE_CLOUD, base::Value(value), nullptr);
    pref_service_->SetInteger(
        proxy_config::prefs::kEnableProxyOverrideRulesForAllUsers, value);
  }

  void SetDeviceAffiliated(bool affiliated) {
    if (affiliated) {
      policy_map_.SetDeviceAffiliationIds({"id"});
    } else {
      policy_map_.SetDeviceAffiliationIds({});
    }
    proxy_config_tracker_->OnProviderUpdatePropagated(nullptr);
  }

  void ExpectBuckets(const base::HistogramTester& tester,
                     std::string_view histogram,
                     int unaffected,
                     int allowed_by_for_all_users,
                     int would_be_blocked) {
    using Impact = proxy_config::ProxyPlatformAffiliationImpact;
    tester.ExpectBucketCount(histogram, Impact::kUnaffected, unaffected);
    tester.ExpectBucketCount(histogram, Impact::kAllowedByForAllUsers,
                             allowed_by_for_all_users);
    tester.ExpectBucketCount(histogram, Impact::kWouldBeBlocked,
                             would_be_blocked);
    tester.ExpectTotalCount(
        histogram, unaffected + allowed_by_for_all_users + would_be_blocked);
  }

  base::test::ScopedFeatureList affiliation_features_;
  bool policy_service_initialized_ = true;
  base::HistogramTester histogram_tester_;
};

TEST_F(PrefProxyConfigOverrideRulesAffiliationMetricsTest,
       InitializationAndRecordingGates) {
  SetPlatformManaged(true);

  // Auxiliary trackers (`record_affiliation_metrics = false`) never emit.
  InitMetricsTracker(/*record_affiliation_metrics=*/false);
  SetManagedOverrideRules(policy::POLICY_SCOPE_USER);
  ExpectBuckets(histogram_tester_, kPolicyHistogram, 0, 0, 0);
  ExpectBuckets(histogram_tester_, kExtensionHistogram, 0, 0, 0);

  // Detaching before `PolicyService` initializes removes the init observer
  // cleanly without emitting; `OnPolicyServiceInitialized()` also ignores
  // non-`POLICY_DOMAIN_CHROME` domains.
  EXPECT_CALL(mock_policy_service_,
              AddObserver(policy::POLICY_DOMAIN_CHROME, _))
      .Times(1);
  InitMetricsTracker(/*record_affiliation_metrics=*/true,
                     /*policy_service_initialized=*/false);
  SetManagedOverrideRules(policy::POLICY_SCOPE_USER);
  policy_service_initialized_ = true;
  proxy_config_tracker_->OnPolicyServiceInitialized(
      policy::POLICY_DOMAIN_EXTENSIONS);
  ExpectBuckets(histogram_tester_, kPolicyHistogram, 0, 0, 0);

  policy_service_initialized_ = false;
  EXPECT_CALL(mock_policy_service_, RemoveObserver(policy::POLICY_DOMAIN_CHROME,
                                                   proxy_config_tracker_.get()))
      .Times(1);
  proxy_config_tracker_->DetachFromPrefService();
  proxy_config_tracker_.reset();
  proxy_config_service_.reset();
  ExpectBuckets(histogram_tester_, kPolicyHistogram, 0, 0, 0);

  // Primary trackers wait until `POLICY_DOMAIN_CHROME` initialization
  // completes, then remove the init observer and emit.
  EXPECT_CALL(mock_policy_service_,
              AddObserver(policy::POLICY_DOMAIN_CHROME, _))
      .Times(1);
  InitMetricsTracker(/*record_affiliation_metrics=*/true,
                     /*policy_service_initialized=*/false);
  SetManagedOverrideRules(policy::POLICY_SCOPE_USER);
  ExpectBuckets(histogram_tester_, kPolicyHistogram, 0, 0, 0);

  EXPECT_CALL(mock_policy_service_, RemoveObserver(policy::POLICY_DOMAIN_CHROME,
                                                   proxy_config_tracker_.get()))
      .Times(1);
  policy_service_initialized_ = true;
  proxy_config_tracker_->OnPolicyServiceInitialized(
      policy::POLICY_DOMAIN_CHROME);
  ExpectBuckets(histogram_tester_, kPolicyHistogram, 0, 0, 1);
  ExpectBuckets(histogram_tester_, kExtensionHistogram, 0, 0, 0);

  // When `PolicyService` is already initialized and `kProxyOverrideRules` is
  // pre-populated in `pref_service_` before tracker construction, the
  // constructor immediately records the initial sample.
  RecreateTrackerPreservingPrefs(/*record_affiliation_metrics=*/true,
                                 /*policy_service_initialized=*/true);
  ExpectBuckets(histogram_tester_, kPolicyHistogram, 0, 0, 2);
  ExpectBuckets(histogram_tester_, kExtensionHistogram, 0, 0, 0);
}

TEST_F(PrefProxyConfigOverrideRulesAffiliationMetricsTest,
       PolicyBucketsDeduplicationAndTransitions) {
  InitMetricsTracker();

  // Non-POR proxy preferences alone do not emit any sample.
  SetPlatformManaged(true);
  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      std::make_unique<base::Value>(ProxyConfigDictionary::CreateAutoDetect()));
  ExpectBuckets(histogram_tester_, kPolicyHistogram, 0, 0, 0);

  struct TransitionStep {
    const char* name;
    policy::PolicyScope scope;
    bool platform_managed;
    int for_all_users;
    policy::PolicyScope for_all_users_scope;
    bool cbcm_enrolled;
    int expected_unaffected;
    int expected_allowed;
    int expected_blocked;
  };
  const TransitionStep kSteps[] = {
      // Machine-scoped policy on a platform-managed non-CBCM device.
      {"machine_scope", policy::POLICY_SCOPE_MACHINE, true, 0,
       policy::POLICY_SCOPE_MACHINE, false, 1, 0, 0},
      // User scope on an unmanaged device stays kUnaffected (deduplicated).
      {"unmanaged_user_scope", policy::POLICY_SCOPE_USER, false, 0,
       policy::POLICY_SCOPE_MACHINE, false, 1, 0, 0},
      // User scope on a platform-managed non-CBCM device -> kWouldBeBlocked.
      {"platform_managed_blocked", policy::POLICY_SCOPE_USER, true, 0,
       policy::POLICY_SCOPE_MACHINE, false, 1, 0, 1},
      // Repeated identical state -> deduplicated.
      {"repeated_no_op", policy::POLICY_SCOPE_USER, true, 0,
       policy::POLICY_SCOPE_MACHINE, false, 1, 0, 1},
      // User-scoped EnableProxyOverrideRulesForAllUsers=1 (or USER_STORE pref)
      // does not satisfy machine-scope requirement -> stays kWouldBeBlocked.
      {"user_scoped_for_all_users_ignored", policy::POLICY_SCOPE_USER, true, 1,
       policy::POLICY_SCOPE_USER, false, 1, 0, 1},
      // Machine-scoped EnableProxyOverrideRulesForAllUsers=1 ->
      // kAllowedByForAllUsers.
      {"for_all_users_enabled", policy::POLICY_SCOPE_USER, true, 1,
       policy::POLICY_SCOPE_MACHINE, false, 1, 1, 1},
      // Enrolling in CBCM (device affiliation IDs populated) -> kUnaffected.
      {"cbcm_enrolled", policy::POLICY_SCOPE_USER, true, 1,
       policy::POLICY_SCOPE_MACHINE, true, 2, 1, 1},
  };

  SetManagedOverrideRules(policy::POLICY_SCOPE_MACHINE);
  for (const auto& step : kSteps) {
    SCOPED_TRACE(step.name);
    SetPlatformManaged(step.platform_managed);
    pref_service_->SetInteger(proxy_config::prefs::kProxyOverrideRulesScope,
                              step.scope);
    SetForAllUsers(step.for_all_users, step.for_all_users_scope);
    SetDeviceAffiliated(step.cbcm_enrolled);
    ExpectBuckets(histogram_tester_, kPolicyHistogram, step.expected_unaffected,
                  step.expected_allowed, step.expected_blocked);
  }
  ExpectBuckets(histogram_tester_, kExtensionHistogram, 0, 0, 0);
}

TEST_F(PrefProxyConfigOverrideRulesAffiliationMetricsTest,
       PolicyAndExtensionPrecedence) {
  SetPlatformManaged(true);

  // 1. Policy set first, then Extension: `MANAGED_STORE` shadows
  //    `EXTENSION_STORE`, so `.Extension` is not recorded until policy is
  //    removed and the extension actually takes control.
  InitMetricsTracker();
  SetManagedOverrideRules(policy::POLICY_SCOPE_MACHINE);
  ExpectBuckets(histogram_tester_, kPolicyHistogram, 1, 0, 0);
  ExpectBuckets(histogram_tester_, kExtensionHistogram, 0, 0, 0);

  SetExtensionControlledOverrideRules();
  ExpectBuckets(histogram_tester_, kPolicyHistogram, 1, 0, 0);
  ExpectBuckets(histogram_tester_, kExtensionHistogram, 0, 0, 0);

  pref_service_->RemoveManagedPref(proxy_config::prefs::kProxyOverrideRules);
  ExpectBuckets(histogram_tester_, kPolicyHistogram, 1, 0, 0);
  ExpectBuckets(histogram_tester_, kExtensionHistogram, 0, 0, 1);

  // 2. Extension set first, then overridden by Policy, then Policy removed:
  //    each source is recorded once when it takes control, and falling back to
  //    the unchanged extension state remains deduplicated per source.
  base::HistogramTester reverse_tester;
  pref_service_->RemoveExtensionPref(proxy_config::prefs::kProxyOverrideRules);
  InitMetricsTracker();

  SetExtensionControlledOverrideRules();
  ExpectBuckets(reverse_tester, kPolicyHistogram, 0, 0, 0);
  ExpectBuckets(reverse_tester, kExtensionHistogram, 0, 0, 1);

  SetManagedOverrideRules(policy::POLICY_SCOPE_USER);
  ExpectBuckets(reverse_tester, kPolicyHistogram, 0, 0, 1);
  ExpectBuckets(reverse_tester, kExtensionHistogram, 0, 0, 1);

  pref_service_->RemoveManagedPref(proxy_config::prefs::kProxyOverrideRules);
  pref_service_->RemoveExtensionPref(proxy_config::prefs::kProxyOverrideRules);
  SetExtensionControlledOverrideRules();
  ExpectBuckets(reverse_tester, kPolicyHistogram, 0, 0, 1);
  ExpectBuckets(reverse_tester, kExtensionHistogram, 0, 0, 1);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENTERPRISE_PROXY)
TEST_F(PrefProxyConfigTrackerImplTest, DynamicRoutingConfigStatusChanged) {
  InitConfigService(net::ProxyConfigService::CONFIG_UNSET);

  MockObserver observer;
  proxy_config_service_->AddObserver(&observer);

  auto update_and_capture =
      [&](const net::ProxyConfig::DynamicRoutingConfig& config) {
        base::RunLoop run_loop;
        net::ProxyConfigWithAnnotation captured_config;
        EXPECT_CALL(observer,
                    OnProxyConfigChanged(testing::_,
                                         net::ProxyConfigService::CONFIG_VALID))
            .WillOnce(
                [&](const net::ProxyConfigWithAnnotation& cfg,
                    net::ProxyConfigService::ConfigAvailability availability) {
                  captured_config = cfg;
                  run_loop.Quit();
                });

        SetActiveDynamicRoutingConfig(config);
        run_loop.Run();
        return captured_config;
      };

  // Dynamic routing update with in_progress = true.
  net::ProxyConfig::DynamicRoutingConfig in_progress_config;
  in_progress_config.is_update_in_progress = true;
  net::ProxyConfigWithAnnotation captured_in_progress =
      update_and_capture(in_progress_config);
  EXPECT_TRUE(captured_in_progress.value()
                  .dynamic_routing_config()
                  .is_update_in_progress);
  EXPECT_TRUE(captured_in_progress.value()
                  .dynamic_routing_config()
                  .routing_rules.empty());

  // Another dynamic routing update but with in_progress = false.
  net::ProxyConfig::DynamicRoutingConfig resolved_config;
  resolved_config.is_update_in_progress = false;
  net::ProxyConfig::DynamicRoutingRule rule;
  rule.proxy_list.AddProxyChain(net::ProxyUriToProxyChain(
      "https://proxy.example.com:443", net::ProxyServer::SCHEME_HTTPS));
  resolved_config.routing_rules.push_back(rule);
  net::ProxyConfigWithAnnotation captured_resolved =
      update_and_capture(resolved_config);
  EXPECT_FALSE(
      captured_resolved.value().dynamic_routing_config().is_update_in_progress);
  EXPECT_EQ(
      1u,
      captured_resolved.value().dynamic_routing_config().routing_rules.size());

  // Policy pref change combined with active dynamic routing rules.
  {
    base::RunLoop run_loop;
    net::ProxyConfigWithAnnotation captured_config;
    EXPECT_CALL(
        observer,
        OnProxyConfigChanged(testing::_, net::ProxyConfigService::CONFIG_VALID))
        .WillOnce(
            [&](const net::ProxyConfigWithAnnotation& config,
                net::ProxyConfigService::ConfigAvailability availability) {
              captured_config = config;
              run_loop.Quit();
            });

    pref_service_->SetManagedPref(
        proxy_config::prefs::kProxy,
        std::make_unique<base::Value>(ProxyConfigDictionary::CreateFixedServers(
            "http://policy-proxy.example.com:8080", std::string())));
    run_loop.Run();

    EXPECT_EQ(net::ProxyConfig::ProxyRules::Type::PROXY_LIST,
              captured_config.value().proxy_rules().type);
    EXPECT_EQ(
        1u,
        captured_config.value().dynamic_routing_config().routing_rules.size());
  }

  proxy_config_service_->RemoveObserver(&observer);
}

// Verifies that if EnterpriseProxyService already has an active dynamic routing
// state before PrefProxyConfigTrackerImpl is constructed, that initial state is
// immediately propagated to ProxyConfigService without needing a subsequent
// OnDynamicProxyConfigsStatusChanged() notification.
TEST_F(PrefProxyConfigTrackerImplTest, DynamicRoutingConfigInitialState) {
  // Simulate EnterpriseProxyService already having an in-progress refresh and
  // cached routing rules prior to tracker construction.
  net::ProxyConfig::DynamicRoutingConfig initial_dynamic_config;
  initial_dynamic_config.is_update_in_progress = true;
  net::ProxyConfig::DynamicRoutingRule rule;
  rule.proxy_list.AddProxyChain(net::ProxyUriToProxyChain(
      "https://proxy.example.com:443", net::ProxyServer::SCHEME_HTTPS));
  initial_dynamic_config.routing_rules.push_back(rule);

  EXPECT_CALL(mock_enterprise_proxy_service_, GetDynamicRoutingConfig())
      .WillRepeatedly(testing::Return(initial_dynamic_config));

  InitConfigService(net::ProxyConfigService::CONFIG_UNSET);

  // Without firing NotifyObservers(), GetLatestProxyConfig() should already
  // return CONFIG_VALID with the initial dynamic routing config attached.
  net::ProxyConfigWithAnnotation actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(
      actual_config.value().dynamic_routing_config().is_update_in_progress);
  EXPECT_EQ(
      1u, actual_config.value().dynamic_routing_config().routing_rules.size());
}
#endif

}  // namespace
