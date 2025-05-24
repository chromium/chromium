// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/ash/app_mode/test/network_state_mixin.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "chromeos/ash/components/network/proxy/ui_proxy_config_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_prefs.h"
#include "content/public/test/browser_test.h"
#include "net/proxy_resolution/proxy_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// Helper class to observe changes in NetworkStateInformer.
class TestNetworkStateInformerObserver
    : public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  TestNetworkStateInformerObserver() : update_count_(0), ready_count_(0) {}

  TestNetworkStateInformerObserver(const TestNetworkStateInformerObserver&) =
      delete;
  TestNetworkStateInformerObserver& operator=(
      const TestNetworkStateInformerObserver&) = delete;

  ~TestNetworkStateInformerObserver() override = default;

  // NetworkStateInformerObserver:
  void UpdateState(NetworkError::ErrorReason reason) override {
    ++update_count_;
    reasons_.push_back(reason);  // Add reason to the list
  }

  void OnNetworkReady() override { ++ready_count_; }

  bool HasProxyConfigChangedReason() {
    return std::any_of(
        reasons_.begin(), reasons_.end(), [](NetworkError::ErrorReason reason) {
          return reason == NetworkError::ERROR_REASON_PROXY_CONFIG_CHANGED;
        });
  }

  int update_count() const { return update_count_; }
  int ready_count() const { return ready_count_; }
  const std::vector<NetworkError::ErrorReason>& reasons() const {
    return reasons_;
  }

 private:
  int update_count_;
  int ready_count_;
  std::vector<NetworkError::ErrorReason> reasons_;  // List of reasons
};

// Test fixture for NetworkStateInformer.
class NetworkStateInformerTest : public MixinBasedInProcessBrowserTest {
 public:
  NetworkStateInformerTest() = default;

  NetworkStateInformerTest(const NetworkStateInformerTest&) = delete;
  NetworkStateInformerTest& operator=(const NetworkStateInformerTest&) = delete;

  ~NetworkStateInformerTest() override = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void InitializeNetworkInformer() {
    observer_ = std::make_unique<TestNetworkStateInformerObserver>();
    informer_ = base::MakeRefCounted<NetworkStateInformer>();
    informer_->AddObserver(observer_.get());
    informer_->Init();
  }

  NetworkStateTestHelper& network_state_test_helper() {
    return network_state_.network_state_test_helper();
  }

  void SetupDefaultNetworkEnvironment() { network_state_.SimulateOnline(); }

  void SetupDefaultNetworkEnvironmentWithPacProxy() {
    network_state_test_helper().device_test()->AddDevice(
        "/device/stub_wifi_device", shill::kTypeWifi, "stub_wifi_device");
    network_state_test_helper().service_test()->AddService(
        "stub_wifi_service", "stub_wifi_guid", "wifi1", shill::kTypeWifi,
        shill::kStateOnline, true);
    network_state_test_helper().service_test()->SetServiceProperty(
        "stub_wifi_service", shill::kProfileProperty,
        base::Value("user_profile"));
    network_state_test_helper().service_test()->SetServiceProperty(
        "stub_wifi_service", shill::kProxyConfigProperty,
        base::Value(ProxyConfigDictionary::CreatePacScript(
                        "http://wpad/wpad.dat", false)
                        .DebugString()));
    base::RunLoop().RunUntilIdle();
  }

  void SetupDefaultWifiNetworkWithDirectProxy() {
    network_state_test_helper().service_test()->SetServiceProperty(
        "/service/wifi_0", shill::kProxyConfigProperty,
        base::Value(ProxyConfigDictionary::CreateDirect().DebugString()));
    base::RunLoop().RunUntilIdle();
  }

  void SetupDefaultWifiNetworkWithPacProxy() {
    network_state_test_helper().service_test()->SetServiceProperty(
        "/service/wifi_0", shill::kProxyConfigProperty,
        base::Value(ProxyConfigDictionary::CreatePacScript(
                        "http://wpad/wpad.dat", false)
                        .DebugString()));
    base::RunLoop().RunUntilIdle();
  }

  void TearDownOnMainThread() override {
    informer_.reset();
    observer_.reset();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  NetworkStateMixin network_state_{&mixin_host_};
  std::unique_ptr<TestNetworkStateInformerObserver> observer_;
  scoped_refptr<NetworkStateInformer> informer_;
};

// Verifies that the default state is OFFLINE.
IN_PROC_BROWSER_TEST_F(NetworkStateInformerTest, DefaultState) {
  InitializeNetworkInformer();
  EXPECT_EQ(NetworkStateInformer::OFFLINE, informer_->state());
  EXPECT_EQ("", informer_->network_path());
  EXPECT_FALSE(informer_->GetProxyConfigForTesting().has_value());
}

IN_PROC_BROWSER_TEST_F(NetworkStateInformerTest, Init) {
  SetupDefaultNetworkEnvironment();
  InitializeNetworkInformer();
  EXPECT_EQ(NetworkStateInformer::ONLINE, informer_->state());
  EXPECT_EQ("/service/wifi_0", informer_->network_path());
  EXPECT_FALSE(informer_->GetProxyConfigForTesting().has_value());
}

// Confirms successful proxy initialization and verifies that UpdateState event
// is triggered without ERROR_REASON_PROXY_CONFIG_CHANGED.
IN_PROC_BROWSER_TEST_F(NetworkStateInformerTest, ProxyInitialization) {
  SetupDefaultNetworkEnvironmentWithPacProxy();
  InitializeNetworkInformer();
  informer_->DefaultNetworkChanged(
      network_state_test_helper().network_state_handler()->DefaultNetwork());
  EXPECT_EQ(NetworkStateInformer::ONLINE, informer_->state());
  EXPECT_EQ("stub_wifi_service", informer_->network_path());
  EXPECT_TRUE(informer_->GetProxyConfigForTesting().has_value());
  EXPECT_EQ(1, observer_->update_count());
  EXPECT_EQ(1, observer_->ready_count());
  EXPECT_FALSE(observer_->HasProxyConfigChangedReason());
}

IN_PROC_BROWSER_TEST_F(NetworkStateInformerTest, UpdateNetowrkToDirectProxy) {
  SetupDefaultNetworkEnvironment();
  InitializeNetworkInformer();
  SetupDefaultWifiNetworkWithDirectProxy();
  EXPECT_EQ(NetworkStateInformer::ONLINE, informer_->state());
  EXPECT_EQ("/service/wifi_0", informer_->network_path());
  EXPECT_TRUE(informer_->GetProxyConfigForTesting().has_value());
  EXPECT_EQ(2, observer_->update_count());
  EXPECT_EQ(1, observer_->ready_count());
  EXPECT_FALSE(observer_->HasProxyConfigChangedReason());
}

IN_PROC_BROWSER_TEST_F(NetworkStateInformerTest, UpdateNetowrkToPacProxy) {
  SetupDefaultNetworkEnvironment();
  InitializeNetworkInformer();
  SetupDefaultWifiNetworkWithPacProxy();
  EXPECT_EQ(NetworkStateInformer::ONLINE, informer_->state());
  EXPECT_EQ("/service/wifi_0", informer_->network_path());
  EXPECT_TRUE(informer_->GetProxyConfigForTesting().has_value());
  EXPECT_EQ(2, observer_->update_count());
  EXPECT_EQ(1, observer_->ready_count());
  EXPECT_TRUE(observer_->HasProxyConfigChangedReason());
}

}  // namespace
}  // namespace ash
