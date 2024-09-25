// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/proxy/proxy_config_service_impl.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/pref_names.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service_common_unittest.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(b/369239876): Clean up the tests.

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

// Only used for ProxyConfigServiceImplWithDescriptionTest.
namespace {

enum class Mode {
  kDirect,
  kAutoDetect,
  kPac,
  kSingleProxy,
  kPerSchemeProxy,
};

struct Input {
  Mode mode;
  std::string pac_url;
  std::string server;
  std::string bypass_rules;
};

// Builds an identifier for each test in an array.
#define TEST_DESC(desc) base::StringPrintf("at line %d <%s>", __LINE__, desc)

// Inspired from net/proxy_resolution/proxy_config_service_linux_unittest.cc.
const struct TestParams {
  // Short description to identify the test
  std::string description;

  Input input;

  // Expected outputs from fields of net::ProxyConfig (via IO).
  bool auto_detect;
  const char* pac_url;
  net::ProxyRulesExpectation proxy_rules;
} tests[] = {
    {
        // 0
        TEST_DESC("No proxying"),

        {
            // Input.
            Mode::kDirect,  // mode
        },

        // Expected result.
        false,                                // auto_detect
        "",                                   // pac_url
        net::ProxyRulesExpectation::Empty(),  // proxy_rules
    },

    {
        // 1
        TEST_DESC("Auto detect"),

        {
            // Input.
            Mode::kAutoDetect,  // mode
        },

        // Expected result.
        true,                                 // auto_detect
        "",                                   // pac_url
        net::ProxyRulesExpectation::Empty(),  // proxy_rules
    },

    {
        // 2
        TEST_DESC("Valid PAC URL"),

        {
            // Input.
            Mode::kPac,              // mode
            "http://wpad/wpad.dat",  // pac_url
        },

        // Expected result.
        false,                                // auto_detect
        "http://wpad/wpad.dat",               // pac_url
        net::ProxyRulesExpectation::Empty(),  // proxy_rules
    },

    {
        // 3
        TEST_DESC("Invalid PAC URL"),

        {
            // Input.
            Mode::kPac,  // mode
            "wpad.dat",  // pac_url
        },

        // Expected result.
        false,                                // auto_detect
        "",                                   // pac_url
        net::ProxyRulesExpectation::Empty(),  // proxy_rules
    },

    {
        // 4
        TEST_DESC("Single-host in proxy list"),

        {
            // Input.
            Mode::kSingleProxy,  // mode
            "",                  // pac_url
            "www.google.com",    // server
        },

        // Expected result.
        false,                               // auto_detect
        "",                                  // pac_url
        net::ProxyRulesExpectation::Single(  // proxy_rules
            "www.google.com:80",             // single proxy
            "<local>"),                      // bypass rules
    },

    {
        // 5
        TEST_DESC("Single-host, different port"),

        {
            // Input.
            Mode::kSingleProxy,   // mode
            "",                   // pac_url
            "www.google.com:99",  // server
        },

        // Expected result.
        false,                               // auto_detect
        "",                                  // pac_url
        net::ProxyRulesExpectation::Single(  // proxy_rules
            "www.google.com:99",             // single
            "<local>"),                      // bypass rules
    },

    {
        // 6
        TEST_DESC("Tolerate a scheme"),

        {
            // Input.
            Mode::kSingleProxy,          // mode
            "",                          // pac_url
            "http://www.google.com:99",  // server
        },

        // Expected result.
        false,                               // auto_detect
        "",                                  // pac_url
        net::ProxyRulesExpectation::Single(  // proxy_rules
            "www.google.com:99",             // single proxy
            "<local>"),                      // bypass rules
    },

    {
        // 7
        TEST_DESC("Per-scheme proxy rules"),

        {
            // Input.
            Mode::kPerSchemeProxy,  // mode
            "",                     // pac_url
            "http=www.google.com:80;https=https://www.foo.com:110;"
            "ftp=ftp.foo.com:121;socks=socks5://socks.com:888",  // server
        },

        // Expected result.
        false,                                           // auto_detect
        "",                                              // pac_url
        net::ProxyRulesExpectation::PerSchemeWithSocks(  // proxy_rules
            "www.google.com:80",                         // http
            "https://www.foo.com:110",                   // https
            "ftp.foo.com:121",                           // ftp
            "socks5://socks.com:888",                    // fallback proxy
            "<local>"),                                  // bypass rules
    },

    {
        // 8
        TEST_DESC("Bypass rules"),

        {
            // Input.
            Mode::kSingleProxy,  // mode
            "",                  // pac_url
            "www.google.com",    // server
            "*.google.com, *foo.com:99, 1.2.3.4:22, 127.0.0.1/8",
            // bypass_rules
        },

        // Expected result.
        false,                               // auto_detect
        "",                                  // pac_url
        net::ProxyRulesExpectation::Single(  // proxy_rules
            "www.google.com:80",             // single proxy
                                             // bypass_rules
            "<local>,*.google.com,*foo.com:99,1.2.3.4:22,127.0.0.1/8"),
    },
};  // tests

const char kEthernetPolicy[] =
    "    { \"GUID\": \"{485d6076-dd44-6b6d-69787465725f5040}\","
    "      \"Type\": \"Ethernet\","
    "      \"Name\": \"MyEthernet\","
    "      \"Ethernet\": {"
    "        \"Authentication\": \"None\" },"
    "      \"ProxySettings\": {"
    "        \"PAC\": \"http://domain.com/x\","
    "        \"Type\": \"PAC\" }"
    "    }";

const char kUserProfilePath[] = "user_profile";

}  // namespace

class ProxyConfigServiceImplWithDescriptionTest : public testing::Test {
 protected:
  ProxyConfigServiceImplWithDescriptionTest() = default;

  void SetUp() override {
    PrefProxyConfigTrackerImpl::RegisterPrefs(pref_service_.registry());
    ::onc::RegisterPrefs(pref_service_.registry());
    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(profile_prefs_.registry());
    ::onc::RegisterProfilePrefs(profile_prefs_.registry());
  }

  void SetUpProxyConfigService(PrefService* profile_prefs) {
    config_service_impl_ = std::make_unique<ProxyConfigServiceImpl>(
        profile_prefs, &pref_service_, content::GetIOThreadTaskRunner({}));
    proxy_config_service_ =
        config_service_impl_->CreateTrackingProxyConfigService(
            std::unique_ptr<net::ProxyConfigService>());

    // CreateTrackingProxyConfigService triggers update of initial prefs proxy
    // config by tracker to chrome proxy config service, so flush all pending
    // tasks so that tests start fresh.
    base::RunLoop().RunUntilIdle();
  }

  void SetUpPrivateWiFi() {
    ShillProfileClient::TestInterface* profile_test =
        network_handler_test_helper_.profile_test();
    ShillServiceClient::TestInterface* service_test =
        network_handler_test_helper_.service_test();

    // Process any pending notifications before clearing services.
    base::RunLoop().RunUntilIdle();
    service_test->ClearServices();

    // Sends a notification about the added profile.
    profile_test->AddProfile(kUserProfilePath, "user_hash");

    service_test->AddService("/service/stub_wifi2", "stub_wifi2" /* guid */,
                             "wifi2_PSK", shill::kTypeWifi, shill::kStateOnline,
                             true /* visible */);
    profile_test->AddService(kUserProfilePath, "/service/stub_wifi2");

    base::RunLoop().RunUntilIdle();
  }

  void SetUpSharedEthernet() {
    ShillProfileClient::TestInterface* profile_test =
        network_handler_test_helper_.profile_test();
    ShillServiceClient::TestInterface* service_test =
        network_handler_test_helper_.service_test();

    // Process any pending notifications before clearing services.
    base::RunLoop().RunUntilIdle();
    service_test->ClearServices();

    // Sends a notification about the added profile.
    profile_test->AddProfile(kUserProfilePath, "user_hash");

    service_test->AddService("/service/ethernet", "stub_ethernet" /* guid */,
                             "eth0", shill::kTypeEthernet, shill::kStateOnline,
                             true /* visible */);
    profile_test->AddService(NetworkProfileHandler::GetSharedProfilePath(),
                             "/service/ethernet");

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    config_service_impl_->DetachFromPrefService();
    base::RunLoop().RunUntilIdle();
    config_service_impl_.reset();
    proxy_config_service_.reset();
  }

  base::Value::Dict InitConfigWithTestInput(const Input& input) {
    switch (input.mode) {
      case Mode::kDirect:
        return ProxyConfigDictionary::CreateDirect();
      case Mode::kAutoDetect:
        return ProxyConfigDictionary::CreateAutoDetect();
      case Mode::kPac:
        return ProxyConfigDictionary::CreatePacScript(input.pac_url, false);
      case Mode::kSingleProxy:
      case Mode::kPerSchemeProxy:
        return ProxyConfigDictionary::CreateFixedServers(input.server,
                                                         input.bypass_rules);
    }
    NOTREACHED_IN_MIGRATION();
    return base::Value::Dict();
  }

  void SetUserConfigInShill(const base::Value::Dict* pref_proxy_config_dict) {
    std::string proxy_config;
    if (pref_proxy_config_dict) {
      base::JSONWriter::Write(*pref_proxy_config_dict, &proxy_config);
    }

    NetworkStateHandler* network_state_handler =
        NetworkHandler::Get()->network_state_handler();
    const NetworkState* network = network_state_handler->DefaultNetwork();
    ASSERT_TRUE(network);
    network_handler_test_helper_.service_test()->SetServiceProperty(
        network->path(), shill::kProxyConfigProperty,
        base::Value(proxy_config));
  }

  // Synchronously gets the latest proxy config.
  void SyncGetLatestProxyConfig(net::ProxyConfigWithAnnotation* config) {
    *config = net::ProxyConfigWithAnnotation();
    // Let message loop process all messages. This will run
    // ChromeProxyConfigService::UpdateProxyConfig, which is posted on IO from
    // PrefProxyConfigTrackerImpl::OnProxyConfigChanged.
    base::RunLoop().RunUntilIdle();
    net::ProxyConfigService::ConfigAvailability availability =
        proxy_config_service_->GetLatestProxyConfig(config);

    EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID, availability);
  }

  content::BrowserTaskEnvironment task_environment_;
  NetworkHandlerTestHelper network_handler_test_helper_;
  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  std::unique_ptr<ProxyConfigServiceImpl> config_service_impl_;
  TestingPrefServiceSimple pref_service_;
  sync_preferences::TestingPrefServiceSyncable profile_prefs_;
};

TEST_F(ProxyConfigServiceImplWithDescriptionTest, NetworkProxy) {
  SetUpPrivateWiFi();
  // Create a ProxyConfigServiceImpl like for the system request context.
  SetUpProxyConfigService(nullptr /* no profile prefs */);
  for (size_t i = 0; i < std::size(tests); ++i) {
    SCOPED_TRACE(
        base::StringPrintf("Test[%" PRIuS "] %s", i,
                           UNSAFE_BUFFERS(tests[i]).description.c_str()));

    base::Value::Dict test_config =
        InitConfigWithTestInput(UNSAFE_BUFFERS(tests[i]).input);
    SetUserConfigInShill(&test_config);

    net::ProxyConfigWithAnnotation config;
    SyncGetLatestProxyConfig(&config);

    EXPECT_EQ(UNSAFE_BUFFERS(tests[i]).auto_detect,
              config.value().auto_detect());
    EXPECT_EQ(GURL(UNSAFE_BUFFERS(tests[i]).pac_url), config.value().pac_url());
    EXPECT_TRUE(UNSAFE_BUFFERS(tests[i]).proxy_rules.Matches(
        config.value().proxy_rules()));
  }
}

TEST_F(ProxyConfigServiceImplWithDescriptionTest, DynamicPrefsOverride) {
  SetUpPrivateWiFi();
  // Create a ProxyConfigServiceImpl like for the system request context.
  SetUpProxyConfigService(nullptr /* no profile prefs */);
  // Groupings of 3 test inputs to use for managed, recommended and network
  // proxies respectively.  Only valid and non-direct test inputs are used.
  const size_t proxies[][3] = {
      // clang-format off
    { 1, 2, 4, },
    { 1, 4, 2, },
    { 4, 2, 1, },
    { 2, 1, 4, },
    { 2, 4, 5, },
    { 2, 5, 4, },
    { 5, 4, 2, },
    { 4, 2, 5, },
    { 4, 5, 6, },
    { 4, 6, 5, },
    { 6, 5, 4, },
    { 5, 4, 6, },
    { 5, 6, 7, },
    { 5, 7, 6, },
    { 7, 6, 5, },
    { 6, 5, 7, },
    { 6, 7, 8, },
    { 6, 8, 7, },
    { 8, 7, 6, },
    { 7, 6, 8, },
      // clang-format on
  };
  for (size_t i = 0; i < std::size(proxies); ++i) {
    const TestParams& managed_params = UNSAFE_BUFFERS(tests[proxies[i][0]]);
    const TestParams& recommended_params = UNSAFE_BUFFERS(tests[proxies[i][1]]);
    const TestParams& network_params = UNSAFE_BUFFERS(tests[proxies[i][2]]);

    SCOPED_TRACE(base::StringPrintf(
        "Test[%" PRIuS "] managed=[%s], recommended=[%s], network=[%s]", i,
        managed_params.description.c_str(),
        recommended_params.description.c_str(),
        network_params.description.c_str()));

    base::Value managed_config(InitConfigWithTestInput(managed_params.input));
    base::Value recommended_config(
        InitConfigWithTestInput(recommended_params.input));

    // Managed proxy pref should take effect over recommended proxy and
    // non-existent network proxy.
    SetUserConfigInShill(nullptr);
    pref_service_.SetManagedPref(
        ::proxy_config::prefs::kProxy,
        base::Value::ToUniquePtrValue(managed_config.Clone()));
    pref_service_.SetRecommendedPref(
        ::proxy_config::prefs::kProxy,
        base::Value::ToUniquePtrValue(recommended_config.Clone()));
    net::ProxyConfigWithAnnotation actual_config;
    SyncGetLatestProxyConfig(&actual_config);
    EXPECT_EQ(managed_params.auto_detect, actual_config.value().auto_detect());
    EXPECT_EQ(GURL(managed_params.pac_url), actual_config.value().pac_url());
    EXPECT_TRUE(managed_params.proxy_rules.Matches(
        actual_config.value().proxy_rules()));

    // Recommended proxy pref should take effect when managed proxy pref is
    // removed.
    pref_service_.RemoveManagedPref(::proxy_config::prefs::kProxy);
    SyncGetLatestProxyConfig(&actual_config);
    EXPECT_EQ(recommended_params.auto_detect,
              actual_config.value().auto_detect());
    EXPECT_EQ(GURL(recommended_params.pac_url),
              actual_config.value().pac_url());
    EXPECT_TRUE(recommended_params.proxy_rules.Matches(
        actual_config.value().proxy_rules()));

    base::Value::Dict network_config =
        InitConfigWithTestInput(network_params.input);
    // Network proxy should take take effect over recommended proxy pref.
    SetUserConfigInShill(&network_config);
    SyncGetLatestProxyConfig(&actual_config);
    EXPECT_EQ(network_params.auto_detect, actual_config.value().auto_detect());
    EXPECT_EQ(GURL(network_params.pac_url), actual_config.value().pac_url());
    EXPECT_TRUE(network_params.proxy_rules.Matches(
        actual_config.value().proxy_rules()));

    // Managed proxy pref should take effect over network proxy.
    pref_service_.SetManagedPref(
        ::proxy_config::prefs::kProxy,
        base::Value::ToUniquePtrValue(managed_config.Clone()));
    SyncGetLatestProxyConfig(&actual_config);
    EXPECT_EQ(managed_params.auto_detect, actual_config.value().auto_detect());
    EXPECT_EQ(GURL(managed_params.pac_url), actual_config.value().pac_url());
    EXPECT_TRUE(managed_params.proxy_rules.Matches(
        actual_config.value().proxy_rules()));

    // Network proxy should take effect over recommended proxy pref when managed
    // proxy pref is removed.
    pref_service_.RemoveManagedPref(::proxy_config::prefs::kProxy);
    SyncGetLatestProxyConfig(&actual_config);
    EXPECT_EQ(network_params.auto_detect, actual_config.value().auto_detect());
    EXPECT_EQ(GURL(network_params.pac_url), actual_config.value().pac_url());
    EXPECT_TRUE(network_params.proxy_rules.Matches(
        actual_config.value().proxy_rules()));

    // Removing recommended proxy pref should have no effect on network proxy.
    pref_service_.RemoveRecommendedPref(::proxy_config::prefs::kProxy);
    SyncGetLatestProxyConfig(&actual_config);
    EXPECT_EQ(network_params.auto_detect, actual_config.value().auto_detect());
    EXPECT_EQ(GURL(network_params.pac_url), actual_config.value().pac_url());
    EXPECT_TRUE(network_params.proxy_rules.Matches(
        actual_config.value().proxy_rules()));
  }
}

// Tests whether the proxy settings from user policy are used for ethernet even
// if 'UseSharedProxies' is set to false.
// See https://crbug.com/454966 .
TEST_F(ProxyConfigServiceImplWithDescriptionTest, SharedEthernetAndUserPolicy) {
  SetUpSharedEthernet();
  SetUpProxyConfigService(&profile_prefs_);

  std::optional<base::Value::Dict> ethernet_policy =
      chromeos::onc::ReadDictionaryFromJson(kEthernetPolicy);
  ASSERT_TRUE(ethernet_policy.has_value());

  base::Value::List network_configs;
  network_configs.Append(std::move(*ethernet_policy));

  profile_prefs_.SetUserPref(::proxy_config::prefs::kUseSharedProxies,
                             std::make_unique<base::Value>(false));
  profile_prefs_.SetManagedPref(
      ::onc::prefs::kOpenNetworkConfiguration,
      std::make_unique<base::Value>(std::move(network_configs)));

  net::ProxyConfigWithAnnotation actual_config;
  SyncGetLatestProxyConfig(&actual_config);
  net::ProxyConfigWithAnnotation expected_config(
      net::ProxyConfig::CreateFromCustomPacURL(GURL("http://domain.com/x")),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(expected_config.value().Equals(actual_config.value()));
}

}  // namespace ash
