// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/proxy_config_service.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/updater/protos/omaha_settings.pb.h"
#include "device_management_backend.pb.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_companion {

// A stub DMStorage for providing cloud policy data to the ProxyConfigService.
class FakeDMStorage : public device_management_storage::DMStorage {
 public:
  FakeDMStorage() = default;

  void SetProxyPolicies(
      std::optional<std::string> proxy_mode,
      std::optional<std::string> pac_url,
      std::optional<std::string> proxy_server,
      std::optional<bool> cloud_policy_overrides_platform_policy =
          std::nullopt) {
    wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto
        omaha_settings;
    if (proxy_mode) {
      omaha_settings.set_proxy_mode(*proxy_mode);
    }
    if (pac_url) {
      omaha_settings.set_proxy_pac_url(*pac_url);
    }
    if (proxy_server) {
      omaha_settings.set_proxy_server(*proxy_server);
    }
    if (cloud_policy_overrides_platform_policy) {
      omaha_settings.set_cloud_policy_overrides_platform_policy(
          *cloud_policy_overrides_platform_policy);
    }
    policy_data_ = enterprise_management::PolicyData();
    policy_data_->set_policy_value(omaha_settings.SerializeAsString());
  }

  void WriteInvalidPolicyData() {
    policy_data_ = enterprise_management::PolicyData();
    policy_data_->set_policy_value("this is not a real serialized proto.");
  }

  std::optional<enterprise_management::PolicyData> ReadPolicyData(
      const std::string& policy_type) override {
    return policy_data_;
  }

  std::string GetDeviceID() const override { return std::string(); }
  bool IsEnrollmentMandatory() const override { return false; }
  bool StoreEnrollmentToken(const std::string& enrollment_token) override {
    return false;
  }
  bool DeleteEnrollmentToken() override { return false; }
  std::string GetEnrollmentToken() const override { return std::string(); }
  bool StoreDmToken(const std::string& dm_token) override { return false; }
  std::string GetDmToken() const override { return std::string(); }
  bool InvalidateDMToken() override { return false; }
  bool DeleteDMToken() override { return false; }
  bool IsValidDMToken() const override { return false; }
  bool IsDeviceDeregistered() const override { return false; }
  bool CanPersistPolicies() const override { return false; }
  bool PersistPolicies(
      const device_management_storage::DMPolicyMap& policy_map) const override {
    return false;
  }
  bool RemoveAllPolicies() const override { return false; }
  std::unique_ptr<device_management_storage::CachedPolicyInfo>
  GetCachedPolicyInfo() const override {
    return nullptr;
  }
  base::FilePath policy_cache_folder() const override {
    return base::FilePath();
  }

 private:
  std::optional<enterprise_management::PolicyData> policy_data_;

  ~FakeDMStorage() override = default;
};

class MockProxyConfigService final : public net::ProxyConfigService {
 public:
  MockProxyConfigService() = default;
  ~MockProxyConfigService() override = default;

  void AddObserver(Observer*) override {}
  void RemoveObserver(Observer*) override {}

  MOCK_METHOD(ConfigAvailability,
              GetLatestProxyConfig,
              (net::ProxyConfigWithAnnotation*),
              (override));
};

class ProxyConfigServiceTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment environment_;
  scoped_refptr<FakeDMStorage> fake_dm_storage_ =
      base::MakeRefCounted<FakeDMStorage>();
  std::unique_ptr<MockProxyConfigService> mock_fallback_ =
      std::make_unique<MockProxyConfigService>();
  std::optional<ProxyConfigAndOverridePrecedence> system_policy_config_;

  std::unique_ptr<net::ProxyConfigService> CreateService() {
    return CreatePolicyProxyConfigService(
        scoped_refptr<device_management_storage::DMStorage>(fake_dm_storage_),
        base::BindLambdaForTesting([&] { return system_policy_config_; }),
        std::move(mock_fallback_));
  }
};

TEST_F(ProxyConfigServiceTest, ConfigFromCloudPolicy_Direct) {
  fake_dm_storage_->SetProxyPolicies("direct",
                                     /*pac_url=*/std::nullopt,
                                     /*proxy_server=*/std::nullopt);
  EXPECT_CALL(*mock_fallback_, GetLatestProxyConfig).Times(0);
  std::unique_ptr<net::ProxyConfigService> proxy_config_service =
      CreateService();

  net::ProxyConfigWithAnnotation config;
  ASSERT_EQ(proxy_config_service->GetLatestProxyConfig(&config),
            net::ProxyConfigService::ConfigAvailability::CONFIG_VALID);

  EXPECT_TRUE(config.value().Equals(net::ProxyConfig::CreateDirect()));
}

TEST_F(ProxyConfigServiceTest, ConfigFromCloudPolicy_AutoDetect) {
  fake_dm_storage_->SetProxyPolicies("auto_detect",
                                     /*pac_url=*/std::nullopt,
                                     /*proxy_server=*/std::nullopt);
  EXPECT_CALL(*mock_fallback_, GetLatestProxyConfig).Times(0);
  std::unique_ptr<net::ProxyConfigService> proxy_config_service =
      CreateService();

  net::ProxyConfigWithAnnotation config;
  ASSERT_EQ(proxy_config_service->GetLatestProxyConfig(&config),
            net::ProxyConfigService::ConfigAvailability::CONFIG_VALID);

  EXPECT_TRUE(config.value().Equals(net::ProxyConfig::CreateAutoDetect()));
}

TEST_F(ProxyConfigServiceTest, ConfigFromCloudPolicy_PAC) {
  fake_dm_storage_->SetProxyPolicies(
      "pac_script",
      /*pac_url=*/"http://not-a.real-domain:8082/proxy.pac",
      /*proxy_server=*/std::nullopt);
  EXPECT_CALL(*mock_fallback_, GetLatestProxyConfig).Times(0);
  std::unique_ptr<net::ProxyConfigService> proxy_config_service =
      CreateService();

  net::ProxyConfigWithAnnotation config;
  ASSERT_EQ(proxy_config_service->GetLatestProxyConfig(&config),
            net::ProxyConfigService::ConfigAvailability::CONFIG_VALID);

  EXPECT_TRUE(config.value().Equals(net::ProxyConfig::CreateFromCustomPacURL(
      GURL("http://not-a.real-domain:8082/proxy.pac"))));
}

TEST_F(ProxyConfigServiceTest, ConfigFromCloudPolicy_FixedServers) {
  fake_dm_storage_->SetProxyPolicies(
      "fixed_servers",
      /*pac_url=*/std::nullopt,
      /*proxy_server=*/"http://not-a.real-domain:7918");
  EXPECT_CALL(*mock_fallback_, GetLatestProxyConfig).Times(0);
  std::unique_ptr<net::ProxyConfigService> proxy_config_service =
      CreateService();

  net::ProxyConfigWithAnnotation config;
  ASSERT_EQ(proxy_config_service->GetLatestProxyConfig(&config),
            net::ProxyConfigService::ConfigAvailability::CONFIG_VALID);

  net::ProxyList proxy_list;
  proxy_list.SetSingleProxyServer(net::ProxyUriToProxyServer(
      "http://not-a.real-domain:7918", net::ProxyServer::SCHEME_HTTP));
  EXPECT_TRUE(
      config.value().Equals(net::ProxyConfig::CreateForTesting(proxy_list)));
}

TEST_F(ProxyConfigServiceTest, ConfigFromCloudPolicy_System) {
  fake_dm_storage_->SetProxyPolicies("system",
                                     /*pac_url=*/std::nullopt,
                                     /*proxy_server=*/std::nullopt);
  // The "system" mode should delegate to the fallback service.
  EXPECT_CALL(*mock_fallback_, GetLatestProxyConfig)
      .WillOnce([](net::ProxyConfigWithAnnotation* config) {
        *config = net::ProxyConfigWithAnnotation(
            net::ProxyConfig::CreateAutoDetect(), TRAFFIC_ANNOTATION_FOR_TESTS);
        return net::ProxyConfigService::ConfigAvailability::CONFIG_VALID;
      });
  std::unique_ptr<net::ProxyConfigService> proxy_config_service =
      CreateService();

  net::ProxyConfigWithAnnotation config;
  ASSERT_EQ(proxy_config_service->GetLatestProxyConfig(&config),
            net::ProxyConfigService::ConfigAvailability::CONFIG_VALID);

  EXPECT_TRUE(config.value().auto_detect());
}

TEST_F(ProxyConfigServiceTest, NoPolicies_Fallback) {
  EXPECT_CALL(*mock_fallback_, GetLatestProxyConfig)
      .WillOnce([](net::ProxyConfigWithAnnotation* config) {
        *config = net::ProxyConfigWithAnnotation(
            net::ProxyConfig::CreateAutoDetect(), TRAFFIC_ANNOTATION_FOR_TESTS);
        return net::ProxyConfigService::ConfigAvailability::CONFIG_VALID;
      });
  std::unique_ptr<net::ProxyConfigService> proxy_config_service =
      CreateService();

  net::ProxyConfigWithAnnotation config;
  ASSERT_EQ(proxy_config_service->GetLatestProxyConfig(&config),
            net::ProxyConfigService::ConfigAvailability::CONFIG_VALID);

  EXPECT_TRUE(config.value().auto_detect());
}

TEST_F(ProxyConfigServiceTest, EmptyPolicies_Fallback) {
  fake_dm_storage_->SetProxyPolicies(/*proxy_mode=*/std::nullopt,
                                     /*pac_url=*/std::nullopt,
                                     /*proxy_server=*/std::nullopt);
  EXPECT_CALL(*mock_fallback_, GetLatestProxyConfig)
      .WillOnce([](net::ProxyConfigWithAnnotation* config) {
        *config = net::ProxyConfigWithAnnotation(
            net::ProxyConfig::CreateAutoDetect(), TRAFFIC_ANNOTATION_FOR_TESTS);
        return net::ProxyConfigService::ConfigAvailability::CONFIG_VALID;
      });
  std::unique_ptr<net::ProxyConfigService> proxy_config_service =
      CreateService();

  net::ProxyConfigWithAnnotation config;
  ASSERT_EQ(proxy_config_service->GetLatestProxyConfig(&config),
            net::ProxyConfigService::ConfigAvailability::CONFIG_VALID);

  EXPECT_TRUE(config.value().auto_detect());
}

TEST_F(ProxyConfigServiceTest, UnknownProxyMode_Fallback) {
  fake_dm_storage_->SetProxyPolicies(/*proxy_mode=*/"not_a_real_proxy_mode",
                                     /*pac_url=*/std::nullopt,
                                     /*proxy_server=*/std::nullopt);
  EXPECT_CALL(*mock_fallback_, GetLatestProxyConfig)
      .WillOnce([](net::ProxyConfigWithAnnotation* config) {
        *config = net::ProxyConfigWithAnnotation(
            net::ProxyConfig::CreateAutoDetect(), TRAFFIC_ANNOTATION_FOR_TESTS);
        return net::ProxyConfigService::ConfigAvailability::CONFIG_VALID;
      });
  std::unique_ptr<net::ProxyConfigService> proxy_config_service =
      CreateService();

  net::ProxyConfigWithAnnotation config;
  ASSERT_EQ(proxy_config_service->GetLatestProxyConfig(&config),
            net::ProxyConfigService::ConfigAvailability::CONFIG_VALID);

  EXPECT_TRUE(config.value().auto_detect());
}

TEST_F(ProxyConfigServiceTest, UnparsableOmahaSettings_Fallback) {
  fake_dm_storage_->WriteInvalidPolicyData();
  EXPECT_CALL(*mock_fallback_, GetLatestProxyConfig)
      .WillOnce([](net::ProxyConfigWithAnnotation* config) {
        *config = net::ProxyConfigWithAnnotation(
            net::ProxyConfig::CreateAutoDetect(), TRAFFIC_ANNOTATION_FOR_TESTS);
        return net::ProxyConfigService::ConfigAvailability::CONFIG_VALID;
      });
  std::unique_ptr<net::ProxyConfigService> proxy_config_service =
      CreateService();

  net::ProxyConfigWithAnnotation config;
  ASSERT_EQ(proxy_config_service->GetLatestProxyConfig(&config),
            net::ProxyConfigService::ConfigAvailability::CONFIG_VALID);

  EXPECT_TRUE(config.value().auto_detect());
}

TEST_F(ProxyConfigServiceTest, ConfigFromLocalPolicy) {
  system_policy_config_ = ProxyConfigAndOverridePrecedence();
  system_policy_config_->config = net::ProxyConfig::CreateDirect();
  EXPECT_CALL(*mock_fallback_, GetLatestProxyConfig).Times(0);
  std::unique_ptr<net::ProxyConfigService> proxy_config_service =
      CreateService();

  net::ProxyConfigWithAnnotation config;
  ASSERT_EQ(proxy_config_service->GetLatestProxyConfig(&config),
            net::ProxyConfigService::ConfigAvailability::CONFIG_VALID);

  EXPECT_TRUE(config.value().Equals(net::ProxyConfig::CreateDirect()));
}

TEST_F(ProxyConfigServiceTest, LocalPolicyOverridesByDefault) {
  fake_dm_storage_->SetProxyPolicies("direct",
                                     /*pac_url=*/std::nullopt,
                                     /*proxy_server=*/std::nullopt);
  system_policy_config_ = ProxyConfigAndOverridePrecedence();
  system_policy_config_->config = net::ProxyConfig::CreateAutoDetect();
  std::unique_ptr<net::ProxyConfigService> proxy_config_service =
      CreateService();

  net::ProxyConfigWithAnnotation config;
  ASSERT_EQ(proxy_config_service->GetLatestProxyConfig(&config),
            net::ProxyConfigService::ConfigAvailability::CONFIG_VALID);

  EXPECT_TRUE(config.value().Equals(net::ProxyConfig::CreateAutoDetect()));
}

TEST_F(ProxyConfigServiceTest, CloudPolicyOverridesPlatformPolicy_ByLocal) {
  fake_dm_storage_->SetProxyPolicies("direct",
                                     /*pac_url=*/std::nullopt,
                                     /*proxy_server=*/std::nullopt);
  system_policy_config_ = ProxyConfigAndOverridePrecedence();
  system_policy_config_->config = net::ProxyConfig::CreateAutoDetect();
  system_policy_config_->cloud_policy_overrides_platform_policy = true;
  std::unique_ptr<net::ProxyConfigService> proxy_config_service =
      CreateService();

  net::ProxyConfigWithAnnotation config;
  ASSERT_EQ(proxy_config_service->GetLatestProxyConfig(&config),
            net::ProxyConfigService::ConfigAvailability::CONFIG_VALID);

  EXPECT_TRUE(config.value().Equals(net::ProxyConfig::CreateDirect()));
}

TEST_F(ProxyConfigServiceTest, CloudPolicyOverridesPlatformPolicy_ByCloud) {
  fake_dm_storage_->SetProxyPolicies(
      "direct",
      /*pac_url=*/std::nullopt,
      /*proxy_server=*/std::nullopt,
      /*cloud_policy_overrides_platform_policy=*/true);
  system_policy_config_ = ProxyConfigAndOverridePrecedence();
  system_policy_config_->config = net::ProxyConfig::CreateAutoDetect();
  std::unique_ptr<net::ProxyConfigService> proxy_config_service =
      CreateService();

  net::ProxyConfigWithAnnotation config;
  ASSERT_EQ(proxy_config_service->GetLatestProxyConfig(&config),
            net::ProxyConfigService::ConfigAvailability::CONFIG_VALID);

  EXPECT_TRUE(config.value().Equals(net::ProxyConfig::CreateDirect()));
}

// If the "CloudPolicyOverridesPlatformPolicy" from cloud policy disagrees with
// that from local policy, the cloud policy should be honored.
TEST_F(ProxyConfigServiceTest,
       CloudPolicyOverridesPlatformPolicy_CloudAndLocalConflict) {
  fake_dm_storage_->SetProxyPolicies(
      "direct",
      /*pac_url=*/std::nullopt,
      /*proxy_server=*/std::nullopt,
      /*cloud_policy_overrides_platform_policy=*/true);
  system_policy_config_ = ProxyConfigAndOverridePrecedence();
  system_policy_config_->config = net::ProxyConfig::CreateAutoDetect();
  system_policy_config_->cloud_policy_overrides_platform_policy = false;
  std::unique_ptr<net::ProxyConfigService> proxy_config_service =
      CreateService();

  net::ProxyConfigWithAnnotation config;
  ASSERT_EQ(proxy_config_service->GetLatestProxyConfig(&config),
            net::ProxyConfigService::ConfigAvailability::CONFIG_VALID);

  EXPECT_TRUE(config.value().Equals(net::ProxyConfig::CreateDirect()));
}

}  // namespace enterprise_companion
