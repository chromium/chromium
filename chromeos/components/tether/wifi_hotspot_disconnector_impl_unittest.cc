// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/wifi_hotspot_disconnector_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "chromeos/components/tether/fake_network_configuration_remover.h"
#include "chromeos/components/tether/pref_names.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace tether {

namespace {

const char kSuccessResult[] = "success";
const char kWifiNetworkGuid[] = "wifiNetworkGuid";

std::string CreateConnectedWifiConfigurationJsonString(
    const std::string& wifi_network_guid) {
  std::stringstream ss;
  ss << "{"
     << "  \"GUID\": \"" << wifi_network_guid << "\","
     << "  \"Type\": \"" << shill::kTypeWifi << "\","
     << "  \"State\": \"" << shill::kStateOnline << "\""
     << "}";
  return ss.str();
}

class TestNetworkConnectionHandler : public NetworkConnectionHandler {
 public:
  explicit TestNetworkConnectionHandler(base::Closure disconnect_callback)
      : disconnect_callback_(disconnect_callback) {}
  ~TestNetworkConnectionHandler() override = default;

  std::string last_disconnect_service_path() {
    return last_disconnect_service_path_;
  }

  base::Closure last_disconnect_success_callback() {
    return last_disconnect_success_callback_;
  }

  network_handler::ErrorCallback last_disconnect_error_callback() {
    return last_disconnect_error_callback_;
  }

  // NetworkConnectionHandler:
  void DisconnectNetwork(
      const std::string& service_path,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback) override {
    last_disconnect_service_path_ = service_path;
    last_disconnect_success_callback_ = success_callback;
    last_disconnect_error_callback_ = error_callback;

    disconnect_callback_.Run();
  }
  void ConnectToNetwork(const std::string& service_path,
                        const base::Closure& success_callback,
                        const network_handler::ErrorCallback& error_callback,
                        bool check_error_state,
                        ConnectCallbackMode mode) override {}
  void Init(NetworkStateHandler* network_state_handler,
            NetworkConfigurationHandler* network_configuration_handler,
            ManagedNetworkConfigurationHandler*
                managed_network_configuration_handler) override {}

 private:
  base::Closure disconnect_callback_;

  std::string last_disconnect_service_path_;
  base::Closure last_disconnect_success_callback_;
  network_handler::ErrorCallback last_disconnect_error_callback_;
};

}  // namespace

class WifiHotspotDisconnectorImplTest : public testing::Test {
 public:
  WifiHotspotDisconnectorImplTest() = default;
  ~WifiHotspotDisconnectorImplTest() override = default;

  void SetUp() override {
    should_disconnect_successfully_ = true;

    test_network_connection_handler_ =
        base::WrapUnique(new TestNetworkConnectionHandler(
            base::Bind(&WifiHotspotDisconnectorImplTest::
                           OnNetworkConnectionManagerDisconnect,
                       base::Unretained(this))));
    fake_configuration_remover_ =
        std::make_unique<FakeNetworkConfigurationRemover>();
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();

    WifiHotspotDisconnectorImpl::RegisterPrefs(test_pref_service_->registry());
    wifi_hotspot_disconnector_ = std::make_unique<WifiHotspotDisconnectorImpl>(
        test_network_connection_handler_.get(), helper_.network_state_handler(),
        test_pref_service_.get(), fake_configuration_remover_.get());
  }

  void TearDown() override {
    wifi_hotspot_disconnector_.reset();
  }

  void SimulateConnectionToWifiNetwork() {
    wifi_service_path_ = helper_.ConfigureService(
        CreateConnectedWifiConfigurationJsonString(kWifiNetworkGuid));
    EXPECT_FALSE(wifi_service_path_.empty());
  }

  void SetWifiNetworkToDisconnected() {
    EXPECT_FALSE(wifi_service_path_.empty());
    helper_.SetServiceProperty(wifi_service_path_, shill::kStateProperty,
                               base::Value(shill::kStateIdle));
  }

  void SuccessCallback() { disconnection_result_ = kSuccessResult; }

  void ErrorCallback(const std::string& error_name) {
    disconnection_result_ = error_name;
  }

  void CallDisconnect(const std::string& wifi_network_guid) {
    wifi_hotspot_disconnector_->DisconnectFromWifiHotspot(
        wifi_network_guid,
        base::Bind(&WifiHotspotDisconnectorImplTest::SuccessCallback,
                   base::Unretained(this)),
        base::Bind(&WifiHotspotDisconnectorImplTest::ErrorCallback,
                   base::Unretained(this)));
  }

  void OnNetworkConnectionManagerDisconnect() {
    EXPECT_EQ(wifi_service_path_,
              test_network_connection_handler_->last_disconnect_service_path());

    if (should_disconnect_successfully_) {
      SetWifiNetworkToDisconnected();
    }

    // Before the callbacks are invoked, the network configuration should not
    // yet have been cleared, and the disconnecting GUID should still be in
    // prefs.
    EXPECT_TRUE(
        fake_configuration_remover_->last_removed_wifi_network_path().empty());
    EXPECT_FALSE(GetDisconnectingWifiPathFromPrefs().empty());

    if (should_disconnect_successfully_) {
      EXPECT_FALSE(
          test_network_connection_handler_->last_disconnect_success_callback()
              .is_null());
      test_network_connection_handler_->last_disconnect_success_callback()
          .Run();
    } else {
      EXPECT_FALSE(
          test_network_connection_handler_->last_disconnect_error_callback()
              .is_null());
      network_handler::RunErrorCallback(
          test_network_connection_handler_->last_disconnect_error_callback(),
          wifi_service_path_, NetworkConnectionHandler::kErrorDisconnectFailed,
          std::string() /* error_detail */);
    }

    // Now that the callbacks have been invoked, both the network
    // configuration and the disconnecting GUID should have cleared.
    EXPECT_FALSE(
        fake_configuration_remover_->last_removed_wifi_network_path().empty());
    EXPECT_TRUE(GetDisconnectingWifiPathFromPrefs().empty());
  }

  std::string GetResultAndReset() {
    std::string result;
    result.swap(disconnection_result_);
    return result;
  }

  std::string GetDisconnectingWifiPathFromPrefs() {
    return test_pref_service_->GetString(prefs::kDisconnectingWifiNetworkPath);
  }

  std::string GetServiceStringProperty(const std::string& service_path,
                                       const std::string& key) {
    return helper_.GetServiceStringProperty(service_path, key);
  }

  base::test::TaskEnvironment task_environment_;
  NetworkStateTestHelper helper_{true /* use_default_devices_and_services */};

  std::unique_ptr<TestNetworkConnectionHandler>
      test_network_connection_handler_;
  std::unique_ptr<FakeNetworkConfigurationRemover> fake_configuration_remover_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;

  std::string wifi_service_path_;
  std::string disconnection_result_;
  bool should_disconnect_successfully_;

  std::unique_ptr<WifiHotspotDisconnectorImpl> wifi_hotspot_disconnector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WifiHotspotDisconnectorImplTest);
};

TEST_F(WifiHotspotDisconnectorImplTest, NetworkDoesNotExist) {
  CallDisconnect("nonexistentWifiGuid");
  EXPECT_EQ(NetworkConnectionHandler::kErrorNotFound, GetResultAndReset());

  // Configuration should not have been removed.
  EXPECT_TRUE(
      fake_configuration_remover_->last_removed_wifi_network_path().empty());
}

TEST_F(WifiHotspotDisconnectorImplTest, NetworkNotActuallyConnected) {
  // Start with the network disconnected.
  SimulateConnectionToWifiNetwork();
  SetWifiNetworkToDisconnected();

  CallDisconnect(kWifiNetworkGuid);
  EXPECT_EQ(NetworkConnectionHandler::kErrorNotConnected, GetResultAndReset());

  // Configuration should not have been removed.
  EXPECT_TRUE(
      fake_configuration_remover_->last_removed_wifi_network_path().empty());
}

TEST_F(WifiHotspotDisconnectorImplTest, WifiDisconnectionFails) {
  SimulateConnectionToWifiNetwork();

  should_disconnect_successfully_ = false;

  CallDisconnect(kWifiNetworkGuid);
  EXPECT_EQ(NetworkConnectionHandler::kErrorDisconnectFailed,
            GetResultAndReset());

  // The Wi-Fi network should still be connected since disconnection failed.
  EXPECT_EQ(
      shill::kStateOnline,
      GetServiceStringProperty(wifi_service_path_, shill::kStateProperty));

  // Configuration should have been removed despite the failure.
  EXPECT_FALSE(
      fake_configuration_remover_->last_removed_wifi_network_path().empty());
}

TEST_F(WifiHotspotDisconnectorImplTest, WifiDisconnectionSucceeds) {
  SimulateConnectionToWifiNetwork();

  CallDisconnect(kWifiNetworkGuid);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());

  // The Wi-Fi network should be disconnected.
  EXPECT_EQ(shill::kStateIdle, GetServiceStringProperty(wifi_service_path_,
                                                        shill::kStateProperty));

  // Configuration should have been removed.
  EXPECT_FALSE(
      fake_configuration_remover_->last_removed_wifi_network_path().empty());
}

}  // namespace tether

}  // namespace chromeos
