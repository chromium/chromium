// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/proxy/ui_proxy_config_service.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/test/gtest_tags.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/components/onc/onc_utils.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kTestUserWifiGuid[] = "wifi1";
constexpr char kTestUserWifiConfig[] = R"({
    "GUID": "wifi1", "Type": "wifi", "Profile": "user_profile_path",
    "ProxySettings": {"Type": "WPAD"}})";

constexpr char kTestSharedWifiGuid[] = "wifi2";
constexpr char kTestSharedWifiConfig[] = R"({
    "GUID": "wifi2", "Type": "wifi", "Profile": "shared_profile_path"})";

constexpr char kTestUnconfiguredWifiGuid[] = "wifi2";
constexpr char kTestUnconfiguredWifiConfig[] = R"({
    "GUID": "wifi2", "Type": "wifi"})";

constexpr char kAugmentedOncValueTemplate[] =
    R"({"Active": $2, "Effective": "$1", "$1": $2, "UserEditable": $3})";

constexpr char kAugmentedOncValueWithUserSettingTemplate[] =
    R"({"Active": $2, "Effective": "$1", "$1": $2, "UserSetting": $3,
      "UserEditable": $4})";

std::unique_ptr<NetworkState> GetNetworkState(const std::string& guid) {
  auto network_state = std::make_unique<NetworkState>("path");
  network_state->PropertyChanged("Profile", base::Value("profile"));
  network_state->PropertyChanged("GUID", base::Value(guid));
  return network_state;
}

std::string UserSettingOncValue(const std::string& value) {
  return base::ReplaceStringPlaceholders(
      kAugmentedOncValueTemplate, {"UserSetting", value, "true"}, nullptr);
}

std::string UserPolicyOncValue(const std::string& value) {
  return base::ReplaceStringPlaceholders(
      kAugmentedOncValueTemplate, {"UserPolicy", value, "false"}, nullptr);
}

std::string DevicePolicyOncValue(const std::string& value) {
  return base::ReplaceStringPlaceholders(
      kAugmentedOncValueTemplate, {"DevicePolicy", value, "false"}, nullptr);
}

std::string ExtensionControlledOncValue(const std::string& value) {
  return base::ReplaceStringPlaceholders(
      R"({"Active": $1,"Effective": "ActiveExtension",
          "UserEditable": false})",
      {value}, nullptr);
}

std::string UserPolicyAndUserSettingOncValue(const std::string& policy,
                                             const std::string& user_setting) {
  return base::ReplaceStringPlaceholders(
      kAugmentedOncValueWithUserSettingTemplate,
      {
          "UserPolicy",
          policy,
          user_setting,
          "false",
      },
      nullptr);
}

std::string ExtensionControlledAndUserSettingOncValue(
    const std::string& extension,
    const std::string& user_setting) {
  return base::ReplaceStringPlaceholders(
      R"({"Active": $1,"Effective": "ActiveExtension", "UserSetting": $2,
          "UserEditable": false})",
      {extension, user_setting}, nullptr);
}

}  // namespace

class UIProxyConfigServiceTest : public testing::Test {
 public:
  UIProxyConfigServiceTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {
    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
    PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());
    ::onc::RegisterProfilePrefs(user_prefs_.registry());
    ::onc::RegisterPrefs(local_state_.registry());
  }

  void SetUp() override {
    base::AddTagToTestResult("feature_id",
                             "screenplay-bb5f01d1-9cf8-4aba-9aa6-925608747b02");
    ConfigureService(kTestUserWifiConfig);
    ConfigureService(kTestSharedWifiConfig);
    ConfigureService(kTestUnconfiguredWifiConfig);
  }

  ~UIProxyConfigServiceTest() override = default;

  void ConfigureService(const std::string& shill_json_string) {
    std::optional<base::Value::Dict> shill_json_dict =
        chromeos::onc::ReadDictionaryFromJson(shill_json_string);
    CHECK(shill_json_dict.has_value());
    ShillManagerClient::Get()->ConfigureService(
        *shill_json_dict, base::DoNothing(),
        base::BindOnce([](const std::string& name, const std::string& msg) {}));
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<UIProxyConfigService> CreateServiceOffLocalState() {
    return std::make_unique<UIProxyConfigService>(
        nullptr, &local_state_, NetworkHandler::Get()->network_state_handler(),
        NetworkHandler::Get()->network_profile_handler());
  }

  std::unique_ptr<UIProxyConfigService> CreateServiceForUser() {
    return std::make_unique<UIProxyConfigService>(
        &user_prefs_, &local_state_,
        NetworkHandler::Get()->network_state_handler(),
        NetworkHandler::Get()->network_profile_handler());
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;

 private:
  base::test::TaskEnvironment task_environment_;
  NetworkHandlerTestHelper network_handler_test_helper_;
};

TEST_F(UIProxyConfigServiceTest, UnknownNetwork) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value::Dict config;
  EXPECT_FALSE(service->MergeEnforcedProxyConfig("unkown_network", &config));
  EXPECT_EQ(base::Value::Dict(), config);
}

TEST_F(UIProxyConfigServiceTest, UserConfigOnly) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value::Dict config;
  EXPECT_FALSE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));
  EXPECT_EQ(base::Value::Dict(), config);
}

TEST_F(UIProxyConfigServiceTest, LocalStatePrefIgnoredForUserService) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  local_state_.SetManagedPref(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreatePacScript("http://pac/script.pac", true));

  base::Value::Dict config;
  EXPECT_FALSE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));
  EXPECT_EQ(base::Value::Dict(), config);
}

TEST_F(UIProxyConfigServiceTest, LocalStatePolicyPrefForDeviceService) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceOffLocalState();

  local_state_.SetManagedPref(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreatePacScript("http://pac/script.pac", true));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {DevicePolicyOncValue(R"("PAC")"),
       DevicePolicyOncValue(R"("http://pac/script.pac")")},
      nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);
}

TEST_F(UIProxyConfigServiceTest,
       UserPolicyProxyNotMergedForUnconfiguredNetork) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceOffLocalState();

  user_prefs_.SetManagedPref(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreatePacScript("http://pac/script.pac", true));

  base::Value::Dict config;
  EXPECT_FALSE(
      service->MergeEnforcedProxyConfig(kTestUnconfiguredWifiGuid, &config));
  EXPECT_EQ(base::Value::Dict(), config);
}

TEST_F(UIProxyConfigServiceTest, ExtensionProxyNotMergedForUnconfiguredNetork) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceOffLocalState();

  user_prefs_.SetExtensionPref(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreatePacScript("http://pac/script.pac", true));

  base::Value::Dict config;
  EXPECT_FALSE(
      service->MergeEnforcedProxyConfig(kTestUnconfiguredWifiGuid, &config));
  EXPECT_EQ(base::Value::Dict(), config);
}

TEST_F(UIProxyConfigServiceTest, OncPolicyNotMergedForUnfongiuredNetork) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceOffLocalState();

  const std::string user_onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "WPAD"}}])",
      {kTestUnconfiguredWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::test::ParseJsonList(user_onc_config));

  base::Value::Dict config;
  EXPECT_FALSE(
      service->MergeEnforcedProxyConfig(kTestUnconfiguredWifiGuid, &config));
  EXPECT_EQ(base::Value::Dict(), config);
}

TEST_F(UIProxyConfigServiceTest, PacPolicyPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  user_prefs_.SetManagedPref(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreatePacScript("http://pac/script.pac", true));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {UserPolicyOncValue(R"("PAC")"),
       UserPolicyOncValue(R"("http://pac/script.pac")")},
      nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);
}

TEST_F(UIProxyConfigServiceTest, AutoDetectPolicyPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  user_prefs_.SetManagedPref(proxy_config::prefs::kProxy,
                             ProxyConfigDictionary::CreateAutoDetect());

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {UserPolicyOncValue(R"("WPAD")")}, nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);
}

TEST_F(UIProxyConfigServiceTest, DirectPolicyPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  user_prefs_.SetManagedPref(proxy_config::prefs::kProxy,
                             ProxyConfigDictionary::CreateDirect());

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {UserPolicyOncValue(R"("Direct")")}, nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);
}

TEST_F(UIProxyConfigServiceTest, ManualPolicyPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  user_prefs_.SetManagedPref(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreateFixedServers(
          "http=proxy1:81;https=proxy2:81;socks=proxy3:81", "localhost"));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1,
          "Manual": {
            "HTTPProxy": {"Host": $2, "Port": $5},
            "SecureHTTPProxy": {"Host": $3, "Port": $5},
            "SOCKS": {"Host": $4, "Port": $5}
          },
          "ExcludeDomains": $6
          })",
      {UserPolicyOncValue(R"("Manual")"), UserPolicyOncValue(R"("proxy1")"),
       UserPolicyOncValue(R"("proxy2")"), UserPolicyOncValue(R"("proxy3")"),
       UserPolicyOncValue("81"), UserPolicyOncValue(R"(["localhost"])")},
      nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);
}

TEST_F(UIProxyConfigServiceTest, PartialManualPolicyPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  user_prefs_.SetManagedPref(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreateFixedServers("http=proxy1:81;", ""));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1,
          "Manual": {
            "HTTPProxy": {"Host": $2, "Port": $3},
            "SecureHTTPProxy": {"Host": $4, "Port": $5},
            "SOCKS": {"Host": $4, "Port": $5}
          },
          "ExcludeDomains": $6
          })",
      {UserPolicyOncValue(R"("Manual")"), UserPolicyOncValue(R"("proxy1")"),
       UserPolicyOncValue("81"), UserPolicyOncValue(R"("")"),
       UserPolicyOncValue("0"), UserPolicyOncValue("[]")},
      nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);
}

TEST_F(UIProxyConfigServiceTest, ManualPolicyPrefWithPacPreset) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  user_prefs_.SetManagedPref(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreateFixedServers(
          "http=proxy:80;https=proxy:80;socks=proxy:80", "localhost"));

  std::string config_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {UserSettingOncValue(R"("PAC")"),
       UserSettingOncValue(R"("http://pac/test.script.pac")")},
      nullptr);
  base::Value::Dict config = base::test::ParseJsonDict(config_json);

  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1,
          "Manual": {
            "HTTPProxy": {"Host": $2, "Port": $3},
            "SecureHTTPProxy": {"Host": $2, "Port": $3},
            "SOCKS": {"Host": $2, "Port": $3}
          },
          "PAC": $4,
          "ExcludeDomains": $5
         })",
      {UserPolicyAndUserSettingOncValue(R"("Manual")", R"("PAC")"),
       UserPolicyOncValue(R"("proxy")"), UserPolicyOncValue("80"),
       UserSettingOncValue(R"("http://pac/test.script.pac")"),
       UserPolicyOncValue(R"(["localhost"])")},
      nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);
}

TEST_F(UIProxyConfigServiceTest, PacExtensionPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  user_prefs_.SetExtensionPref(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreatePacScript("http://pac/script.pac", true));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {ExtensionControlledOncValue(R"("PAC")"),
       ExtensionControlledOncValue(R"("http://pac/script.pac")")},
      nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);
}

TEST_F(UIProxyConfigServiceTest, AutoDetectExtensionPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  user_prefs_.SetExtensionPref(proxy_config::prefs::kProxy,
                               ProxyConfigDictionary::CreateAutoDetect());

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {ExtensionControlledOncValue(R"("WPAD")")}, nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);
}

TEST_F(UIProxyConfigServiceTest, DirectExtensionPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  user_prefs_.SetExtensionPref(proxy_config::prefs::kProxy,
                               ProxyConfigDictionary::CreateDirect());

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {ExtensionControlledOncValue(R"("Direct")")}, nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);
}

TEST_F(UIProxyConfigServiceTest, ManualExtensionPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  user_prefs_.SetExtensionPref(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreateFixedServers(
          "http=proxy1:81;https=proxy2:82;socks=proxy3:81", "localhost"));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1,
          "Manual": {
            "HTTPProxy": {"Host": $2, "Port": $3},
            "SecureHTTPProxy": {"Host": $4, "Port": $5},
            "SOCKS": {"Host": $6, "Port": $3}
          },
          "ExcludeDomains": $7
          })",
      {ExtensionControlledOncValue(R"("Manual")"),
       ExtensionControlledOncValue(R"("proxy1")"),
       ExtensionControlledOncValue("81"),
       ExtensionControlledOncValue(R"("proxy2")"),
       ExtensionControlledOncValue("82"),
       ExtensionControlledOncValue(R"("proxy3")"),
       ExtensionControlledOncValue(R"(["localhost"])")},
      nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);
}

TEST_F(UIProxyConfigServiceTest, ExtensionProxyOverridesDefault) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  user_prefs_.SetExtensionPref(proxy_config::prefs::kProxy,
                               ProxyConfigDictionary::CreatePacScript(
                                   "http://extension/script.pac", true));

  std::string config_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {UserSettingOncValue(R"("PAC")"),
       UserSettingOncValue(R"("http://default/script.pac")")},
      nullptr);
  base::Value::Dict config = base::test::ParseJsonDict(config_json);

  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {ExtensionControlledAndUserSettingOncValue(R"("PAC")", R"("PAC")"),
       ExtensionControlledAndUserSettingOncValue(
           R"("http://extension/script.pac")",
           R"("http://default/script.pac")")},
      nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);
}

TEST_F(UIProxyConfigServiceTest, PolicyPrefOverridesExtensionPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  user_prefs_.SetManagedPref(proxy_config::prefs::kProxy,
                             ProxyConfigDictionary::CreatePacScript(
                                 "http://managed/script.pac", true));

  user_prefs_.SetExtensionPref(proxy_config::prefs::kProxy,
                               ProxyConfigDictionary::CreateAutoDetect());

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {UserPolicyOncValue(R"("PAC")"),
       UserPolicyOncValue(R"("http://managed/script.pac")")},
      nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);

  auto network_state = GetNetworkState(kTestUserWifiGuid);
  EXPECT_EQ(service->ProxyModeForNetwork(network_state.get()),
            ProxyPrefs::MODE_PAC_SCRIPT);
}

TEST_F(UIProxyConfigServiceTest, PolicyPrefForSharedNetwork) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  user_prefs_.SetManagedPref(proxy_config::prefs::kProxy,
                             ProxyConfigDictionary::CreateAutoDetect());

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestSharedWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {UserPolicyOncValue(R"("WPAD")")}, nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);

  auto network_state = GetNetworkState(kTestSharedWifiGuid);
  EXPECT_EQ(service->ProxyModeForNetwork(network_state.get()),
            ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(UIProxyConfigServiceTest, ExtensionPrefForSharedNetwork) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  user_prefs_.SetExtensionPref(proxy_config::prefs::kProxy,
                               ProxyConfigDictionary::CreateAutoDetect());

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestSharedWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {ExtensionControlledOncValue(R"("WPAD")")}, nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);

  auto network_state = GetNetworkState(kTestSharedWifiGuid);
  EXPECT_EQ(service->ProxyModeForNetwork(network_state.get()),
            ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(UIProxyConfigServiceTest, PacOncUserPolicy) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "PAC",
           "PAC": "http://onc/script.pac"}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::test::ParseJsonList(onc_config));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {UserPolicyOncValue(R"("PAC")"),
       UserPolicyOncValue(R"("http://onc/script.pac")")},
      nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);

  auto network_state = GetNetworkState(kTestUserWifiGuid);
  EXPECT_EQ(service->ProxyModeForNetwork(network_state.get()),
            ProxyPrefs::MODE_PAC_SCRIPT);
}

TEST_F(UIProxyConfigServiceTest, AutoDetectOncUserPolicy) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "WPAD"}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::test::ParseJsonList(onc_config));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {UserPolicyOncValue(R"("WPAD")")}, nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);

  auto network_state = GetNetworkState(kTestUserWifiGuid);
  EXPECT_EQ(service->ProxyModeForNetwork(network_state.get()),
            ProxyPrefs::MODE_AUTO_DETECT);
}

// Tests that ONC policy configured networks without proxy settings force Direct
// connection.
TEST_F(UIProxyConfigServiceTest, OncUserPolicyWithoutProxySettings) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "AutoConnect": false}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::test::ParseJsonList(onc_config));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {UserPolicyOncValue(R"("Direct")")}, nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);

  auto network_state = GetNetworkState(kTestUserWifiGuid);
  EXPECT_EQ(service->ProxyModeForNetwork(network_state.get()),
            ProxyPrefs::MODE_DIRECT);
}

TEST_F(UIProxyConfigServiceTest, DirectOncUserPolicy) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi",
           "ProxySettings": {"Type": "Direct"}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::test::ParseJsonList(onc_config));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {UserPolicyOncValue(R"("Direct")")}, nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);
}

TEST_F(UIProxyConfigServiceTest, ManualOncUserPolicy) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {
           "Type": "Manual",
           "ExcludeDomains": ["foo.test", "localhost"],
           "Manual": {
             "HTTPProxy": {"Host": "proxy1", "Port": 81},
             "SecureHTTPProxy": {"Host": "proxy2", "Port": 82},
             "SOCKS": {"Host": "proxy3", "Port": 83}}}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::test::ParseJsonList(onc_config));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1,
          "Manual": {
            "HTTPProxy": {"Host": $2, "Port": $3},
            "SecureHTTPProxy": {"Host": $4, "Port": $5},
            "SOCKS": {"Host": $7, "Port": $6}
          },
          "ExcludeDomains": $8
          })",
      {UserPolicyOncValue(R"("Manual")"), UserPolicyOncValue(R"("proxy1")"),
       UserPolicyOncValue("81"), UserPolicyOncValue(R"("proxy2")"),
       UserPolicyOncValue("82"), UserPolicyOncValue("83"),
       UserPolicyOncValue(R"("proxy3")"),
       UserPolicyOncValue(R"(["foo.test", "localhost"])")},
      nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);

  auto network_state = GetNetworkState(kTestUserWifiGuid);
  EXPECT_EQ(service->ProxyModeForNetwork(network_state.get()),
            ProxyPrefs::MODE_FIXED_SERVERS);
}

TEST_F(UIProxyConfigServiceTest, PartialManualOncUserPolicy) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {
           "Type": "Manual",
           "Manual": {
             "HTTPProxy": {"Host": "proxy1", "Port": 81},
             "SOCKS": {"Host": "proxy4", "Port": 84}}}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::test::ParseJsonList(onc_config));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1,
          "Manual": {
            "HTTPProxy": {"Host": $2, "Port": $3},
            "SOCKS": {"Host": $4, "Port": $5},
            "SecureHTTPProxy": {"Host": $6, "Port": $7}
          },
          "ExcludeDomains": $8
          })",
      {UserPolicyOncValue(R"("Manual")"), UserPolicyOncValue(R"("proxy1")"),
       UserPolicyOncValue("81"), UserPolicyOncValue(R"("proxy4")"),
       UserPolicyOncValue("84"), UserPolicyOncValue(R"("")"),
       UserPolicyOncValue("0"), UserPolicyOncValue(R"([])")},
      nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);

  auto network_state = GetNetworkState(kTestUserWifiGuid);
  EXPECT_EQ(service->ProxyModeForNetwork(network_state.get()),
            ProxyPrefs::MODE_FIXED_SERVERS);
}

TEST_F(UIProxyConfigServiceTest, OncDevicePolicy) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "PAC",
           "PAC": "http://onc/script.pac"}}])",
      {kTestUserWifiGuid}, nullptr);
  local_state_.SetManagedPref(::onc::prefs::kDeviceOpenNetworkConfiguration,
                              base::test::ParseJsonList(onc_config));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {DevicePolicyOncValue(R"("PAC")"),
       DevicePolicyOncValue(R"("http://onc/script.pac")")},
      nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);

  auto network_state = GetNetworkState(kTestUserWifiGuid);
  EXPECT_EQ(service->ProxyModeForNetwork(network_state.get()),
            ProxyPrefs::MODE_PAC_SCRIPT);
}

TEST_F(UIProxyConfigServiceTest, OncUserPolicyForSharedNetwork) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "WPAD"}}])",
      {kTestSharedWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::test::ParseJsonList(onc_config));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestSharedWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {UserPolicyOncValue(R"("WPAD")")}, nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);

  auto network_state = GetNetworkState(kTestSharedWifiGuid);
  EXPECT_EQ(service->ProxyModeForNetwork(network_state.get()),
            ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(UIProxyConfigServiceTest, OncDevicePolicyForSharedNetwork) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "PAC",
           "PAC": "http://onc/script.pac"}}])",
      {kTestSharedWifiGuid}, nullptr);
  local_state_.SetManagedPref(::onc::prefs::kDeviceOpenNetworkConfiguration,
                              base::test::ParseJsonList(onc_config));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestSharedWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {DevicePolicyOncValue(R"("PAC")"),
       DevicePolicyOncValue(R"("http://onc/script.pac")")},
      nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);

  auto network_state = GetNetworkState(kTestSharedWifiGuid);
  EXPECT_EQ(service->ProxyModeForNetwork(network_state.get()),
            ProxyPrefs::MODE_PAC_SCRIPT);
}

TEST_F(UIProxyConfigServiceTest, OncUserAndDevicePolicy) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string user_onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "WPAD"}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::test::ParseJsonList(user_onc_config));

  const std::string device_onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "PAC",
           "PAC": "http://onc/script.pac"}}])",
      {kTestUserWifiGuid}, nullptr);
  local_state_.SetManagedPref(::onc::prefs::kDeviceOpenNetworkConfiguration,
                              base::test::ParseJsonList(device_onc_config));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {UserPolicyOncValue(R"("WPAD")")}, nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);

  auto network_state = GetNetworkState(kTestUserWifiGuid);
  EXPECT_EQ(service->ProxyModeForNetwork(network_state.get()),
            ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(UIProxyConfigServiceTest, OncUserAndDevicePolicyBuiltOffLocalState) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceOffLocalState();

  const std::string user_onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "WPAD"}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::test::ParseJsonList(user_onc_config));

  const std::string device_onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "PAC",
           "PAC": "http://onc/script.pac"}}])",
      {kTestUserWifiGuid}, nullptr);
  local_state_.SetManagedPref(::onc::prefs::kDeviceOpenNetworkConfiguration,
                              base::test::ParseJsonList(device_onc_config));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {DevicePolicyOncValue(R"("PAC")"),
       DevicePolicyOncValue(R"("http://onc/script.pac")")},
      nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);

  auto network_state = GetNetworkState(kTestUserWifiGuid);
  EXPECT_EQ(service->ProxyModeForNetwork(network_state.get()),
            ProxyPrefs::MODE_PAC_SCRIPT);
}

TEST_F(UIProxyConfigServiceTest, OncUserPolicyOverridesUserSettings) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string user_onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "WPAD"}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::test::ParseJsonList(user_onc_config));

  std::string config_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {UserSettingOncValue(R"("PAC")"),
       UserSettingOncValue(R"("http://pac/test.script.pac")")},
      nullptr);
  base::Value::Dict config = base::test::ParseJsonDict(config_json);

  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {UserPolicyAndUserSettingOncValue(R"("WPAD")", R"("PAC")"),
       UserSettingOncValue(R"("http://pac/test.script.pac")")},
      nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);

  auto network_state = GetNetworkState(kTestUserWifiGuid);
  EXPECT_EQ(service->ProxyModeForNetwork(network_state.get()),
            ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(UIProxyConfigServiceTest, PolicyPrefOverridesOncPolicy) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  user_prefs_.SetManagedPref(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreatePacScript("http://pac/script.pac", true));

  const std::string user_onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "WPAD"}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::test::ParseJsonList(user_onc_config));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {UserPolicyOncValue(R"("PAC")"),
       UserPolicyOncValue(R"("http://pac/script.pac")")},
      nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);

  auto network_state = GetNetworkState(kTestUserWifiGuid);
  EXPECT_EQ(service->ProxyModeForNetwork(network_state.get()),
            ProxyPrefs::MODE_PAC_SCRIPT);
}

TEST_F(UIProxyConfigServiceTest, ExtensionPrefOverridesOncPolicy) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  user_prefs_.SetExtensionPref(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreatePacScript("http://pac/script.pac", true));

  const std::string user_onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "WPAD"}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::test::ParseJsonList(user_onc_config));

  base::Value::Dict config;
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {ExtensionControlledOncValue(R"("PAC")"),
       ExtensionControlledOncValue(R"("http://pac/script.pac")")},
      nullptr);
  base::Value::Dict expected = base::test::ParseJsonDict(expected_json);
  EXPECT_EQ(expected, config);

  auto network_state = GetNetworkState(kTestUserWifiGuid);
  EXPECT_EQ(service->ProxyModeForNetwork(network_state.get()),
            ProxyPrefs::MODE_PAC_SCRIPT);
}

}  // namespace ash
