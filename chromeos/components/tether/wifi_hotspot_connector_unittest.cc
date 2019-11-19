// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/wifi_hotspot_connector.h"

#include <memory>
#include <sstream>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/network/shill_property_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace tether {

namespace {

const char kSsid[] = "ssid";
const char kPassword[] = "password";

const char kOtherWifiServiceGuid[] = "otherWifiServiceGuid";

const char kTetherNetworkGuid[] = "tetherNetworkGuid";
const char kTetherNetworkGuid2[] = "tetherNetworkGuid2";

constexpr base::TimeDelta kConnectionToHotspotTime =
    base::TimeDelta::FromSeconds(20);

std::string CreateConfigurationJsonString(const std::string& guid) {
  std::stringstream ss;
  ss << "{"
     << "  \"GUID\": \"" << guid << "\","
     << "  \"Type\": \"" << shill::kTypeWifi << "\","
     << "  \"State\": \"" << shill::kStateIdle << "\""
     << "}";
  return ss.str();
}

}  // namespace

class WifiHotspotConnectorTest : public testing::Test {
 public:
  class TestNetworkConnect : public NetworkConnect {
   public:
    explicit TestNetworkConnect(NetworkStateTestHelper* helper)
        : helper_(helper) {}
    ~TestNetworkConnect() override = default;

    void set_is_running_in_test_task_runner(
        bool is_running_in_test_task_runner) {
      is_running_in_test_task_runner_ = is_running_in_test_task_runner;
    }

    base::DictionaryValue* last_configuration() {
      return last_configuration_.get();
    }

    std::string last_service_path_created() {
      return last_service_path_created_;
    }

    std::string network_id_to_connect() { return network_id_to_connect_; }
    std::string network_id_to_disconnect() { return network_id_to_disconnect_; }

    uint32_t num_connection_attempts() { return num_connection_attempts_; }
    uint32_t num_disconnection_attempts() {
      return num_disconnection_attempts_;
    }

    // Finish configuring the last specified Wi-Fi network config.
    void ConfigureServiceWithLastNetworkConfig() {
      std::string wifi_guid;
      EXPECT_TRUE(
          last_configuration_->GetString(shill::kGuidProperty, &wifi_guid));

      last_service_path_created_ =
          helper_->ConfigureService(CreateConfigurationJsonString(wifi_guid));
    }

    // NetworkConnect:
    void SetTechnologyEnabled(const chromeos::NetworkTypePattern& technology,
                              bool enabled_state) override {}
    void ShowMobileSetup(const std::string& network_id) override {}
    void ConfigureNetworkIdAndConnect(
        const std::string& network_id,
        const base::DictionaryValue& shill_properties,
        bool shared) override {}
    void CreateConfigurationAndConnect(base::DictionaryValue* shill_properties,
                                       bool shared) override {}

    void CreateConfiguration(base::DictionaryValue* shill_properties,
                             bool shared) override {
      EXPECT_FALSE(shared);
      last_configuration_ = shill_properties->CreateDeepCopy();

      // Prevent nested RunLoops when ConfigureServiceWithLastNetworkConfig()
      // calls NetworkStateTestHelper::ConfigureService(); that causes threading
      // issues. If |test_task_runner_| is causing this function to be run, the
      // client which triggered this call can manually call
      // ConfigureServiceWithLastNetworkConfig() once done.
      if (!is_running_in_test_task_runner_)
        ConfigureServiceWithLastNetworkConfig();
    }

    void ConnectToNetworkId(const std::string& network_id) override {
      num_connection_attempts_++;
      network_id_to_connect_ = network_id;
    }

    void DisconnectFromNetworkId(const std::string& network_id) override {
      num_disconnection_attempts_++;
      network_id_to_disconnect_ = network_id;
    }

   private:
    NetworkStateTestHelper* helper_;
    std::unique_ptr<base::DictionaryValue> last_configuration_;
    std::string last_service_path_created_;
    std::string network_id_to_connect_;
    std::string network_id_to_disconnect_;
    uint32_t num_connection_attempts_ = 0;
    uint32_t num_disconnection_attempts_ = 0;
    bool is_running_in_test_task_runner_ = false;
  };

  WifiHotspotConnectorTest() = default;
  ~WifiHotspotConnectorTest() override = default;

  void SetUp() override {
    other_wifi_service_path_.clear();
    connection_callback_responses_.clear();

    helper_.network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED);
    helper_.network_state_handler()->SetTechnologyEnabled(
        NetworkTypePattern::WiFi(), true /* enabled */,
        chromeos::network_handler::ErrorCallback());
    base::RunLoop().RunUntilIdle();

    SetUpShillState();

    test_network_connect_ = base::WrapUnique(new TestNetworkConnect(&helper_));

    helper_.network_state_handler()->AddTetherNetworkState(
        kTetherNetworkGuid, "tetherNetworkName" /* name */,
        "tetherNetworkCarrier" /* carrier */, 100 /* full battery */,
        100 /* full signal strength */, false /* has_connected_to_host */);

    helper_.network_state_handler()->AddTetherNetworkState(
        kTetherNetworkGuid2, "tetherNetworkName2" /* name */,
        "tetherNetworkCarrier2" /* carrier */, 100 /* full battery */,
        100 /* full signal strength */, false /* has_connected_to_host */);

    wifi_hotspot_connector_ = base::WrapUnique(new WifiHotspotConnector(
        helper_.network_state_handler(), test_network_connect_.get()));

    mock_timer_ = new base::MockOneShotTimer();
    test_clock_.SetNow(base::Time::UnixEpoch());
    test_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    wifi_hotspot_connector_->SetTestDoubles(base::WrapUnique(mock_timer_),
                                            &test_clock_, test_task_runner_);
  }

  void SetUpShillState() {
    // Add a Wi-Fi service unrelated to the one under test. In some tests, a
    // connection from another device is tested.
    other_wifi_service_path_ = helper_.ConfigureService(
        CreateConfigurationJsonString(std::string(kOtherWifiServiceGuid)));
  }

  void TearDown() override {
    wifi_hotspot_connector_.reset();
  }

  void VerifyTimerSet() {
    EXPECT_TRUE(mock_timer_->IsRunning());
    EXPECT_EQ(base::TimeDelta::FromSeconds(
                  WifiHotspotConnector::kConnectionTimeoutSeconds),
              mock_timer_->GetCurrentDelay());
  }

  void VerifyTimerStopped() { EXPECT_FALSE(mock_timer_->IsRunning()); }

  void InvokeTimerTask() {
    VerifyTimerSet();
    mock_timer_->Fire();
  }

  void RunTestTaskRunner() {
    test_network_connect_->set_is_running_in_test_task_runner(true);
    test_task_runner_->RunUntilIdle();
    test_network_connect_->set_is_running_in_test_task_runner(false);
  }

  void NotifyConnectable(const std::string& service_path) {
    helper_.SetServiceProperty(service_path,
                               std::string(shill::kConnectableProperty),
                               base::Value(true));
    RunTestTaskRunner();
  }

  void NotifyConnected(const std::string& service_path) {
    helper_.SetServiceProperty(service_path, std::string(shill::kStateProperty),
                               base::Value(shill::kStateReady));
    RunTestTaskRunner();
  }

  void VerifyConnectionToHotspotDurationRecorded(bool expected) {
    if (expected) {
      histogram_tester_.ExpectTimeBucketCount(
          "InstantTethering.Performance.ConnectToHotspotDuration",
          kConnectionToHotspotTime, 1);
    } else {
      histogram_tester_.ExpectTotalCount(
          "InstantTethering.Performance.ConnectToHotspotDuration", 0);
    }
  }

  // Verifies the last configuration has the expected SSID and password, and
  // returns the Wi-Fi GUID for the configuration.
  std::string VerifyLastConfiguration(const std::string& expected_ssid,
                                      const std::string& expected_password) {
    base::DictionaryValue* last_configuration =
        test_network_connect_->last_configuration();
    EXPECT_TRUE(last_configuration);

    std::string ssid = shill_property_util::GetSSIDFromProperties(
        *last_configuration, false, nullptr);
    EXPECT_EQ(expected_ssid, ssid);

    if (expected_password.empty()) {
      std::string security;
      EXPECT_TRUE(last_configuration->GetString(shill::kSecurityClassProperty,
                                                &security));
      EXPECT_EQ(std::string(shill::kSecurityNone), security);
    } else {
      std::string security;
      EXPECT_TRUE(last_configuration->GetString(shill::kSecurityClassProperty,
                                                &security));
      EXPECT_EQ(std::string(shill::kSecurityPsk), security);

      std::string password;
      EXPECT_TRUE(
          last_configuration->GetString(shill::kPassphraseProperty, &password));
      EXPECT_EQ(expected_password, password);
    }

    std::string wifi_guid;
    EXPECT_TRUE(
        last_configuration->GetString(shill::kGuidProperty, &wifi_guid));
    return wifi_guid;
  }

  void VerifyTetherAndWifiNetworkAssociation(
      const std::string& wifi_guid,
      const std::string& tether_guid,
      uint32_t expected_num_connection_attempts) {
    const NetworkState* wifi_network_state =
        helper_.network_state_handler()->GetNetworkStateFromGuid(wifi_guid);
    ASSERT_TRUE(wifi_network_state);
    EXPECT_EQ(tether_guid, wifi_network_state->tether_guid());

    const NetworkState* tether_network_state =
        helper_.network_state_handler()->GetNetworkStateFromGuid(tether_guid);
    ASSERT_TRUE(tether_network_state);
    EXPECT_EQ(wifi_guid, tether_network_state->tether_guid());

    EXPECT_EQ(expected_num_connection_attempts,
              test_network_connect_->num_connection_attempts());
  }

  void VerifyNetworkNotAssociated(const std::string& guid) {
    const NetworkState* network_state =
        helper_.network_state_handler()->GetNetworkStateFromGuid(guid);
    ASSERT_TRUE(network_state);
    EXPECT_TRUE(network_state->tether_guid().empty());
  }

  void WifiConnectionCallback(const std::string& wifi_guid) {
    connection_callback_responses_.push_back(wifi_guid);
  }

  NetworkStateHandler* network_state_handler() {
    return helper_.network_state_handler();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkStateTestHelper helper_{true /* use_default_devices_and_services */};

  std::string other_wifi_service_path_;
  std::vector<std::string> connection_callback_responses_;

  base::MockOneShotTimer* mock_timer_;
  base::SimpleTestClock test_clock_;
  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner_;
  std::unique_ptr<TestNetworkConnect> test_network_connect_;

  std::unique_ptr<WifiHotspotConnector> wifi_hotspot_connector_;

  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WifiHotspotConnectorTest);
};

TEST_F(WifiHotspotConnectorTest, TestConnect_NetworkDoesNotBecomeConnectable) {
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      std::string(kSsid), std::string(kPassword), kTetherNetworkGuid,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  std::string wifi_guid =
      VerifyLastConfiguration(std::string(kSsid), std::string(kPassword));
  EXPECT_FALSE(wifi_guid.empty());

  // Network does not become connectable.
  EXPECT_TRUE(test_network_connect_->network_id_to_connect().empty());

  // Timeout timer fires.
  EXPECT_EQ(0u, connection_callback_responses_.size());
  InvokeTimerTask();
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_TRUE(connection_callback_responses_[0].empty());

  VerifyConnectionToHotspotDurationRecorded(false /* expected */);
  EXPECT_EQ(0u, test_network_connect_->num_disconnection_attempts());
}

TEST_F(WifiHotspotConnectorTest, TestConnect_AnotherNetworkBecomesConnectable) {
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      std::string(kSsid), std::string(kPassword), kTetherNetworkGuid,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  std::string wifi_guid =
      VerifyLastConfiguration(std::string(kSsid), std::string(kPassword));
  EXPECT_FALSE(wifi_guid.empty());

  // Another network becomes connectable. This should not cause the connection
  // to start.
  NotifyConnectable(other_wifi_service_path_);
  VerifyNetworkNotAssociated(wifi_guid);
  std::string other_wifi_guid = network_state_handler()
                                    ->GetNetworkState(other_wifi_service_path_)
                                    ->guid();
  VerifyNetworkNotAssociated(other_wifi_guid);
  EXPECT_TRUE(test_network_connect_->network_id_to_connect().empty());

  // Timeout timer fires.
  EXPECT_EQ(0u, connection_callback_responses_.size());
  InvokeTimerTask();
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_TRUE(connection_callback_responses_[0].empty());

  VerifyConnectionToHotspotDurationRecorded(false /* expected */);
  EXPECT_EQ(0u, test_network_connect_->num_disconnection_attempts());
}

TEST_F(WifiHotspotConnectorTest, TestConnect_CannotConnectToNetwork) {
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      std::string(kSsid), std::string(kPassword), kTetherNetworkGuid,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  std::string wifi_guid =
      VerifyLastConfiguration(std::string(kSsid), std::string(kPassword));
  EXPECT_FALSE(wifi_guid.empty());

  // Network becomes connectable.
  NotifyConnectable(test_network_connect_->last_service_path_created());
  VerifyTetherAndWifiNetworkAssociation(
      wifi_guid, kTetherNetworkGuid, 1u /* expected_num_connection_attempts */);
  EXPECT_EQ(wifi_guid, test_network_connect_->network_id_to_connect());

  // Network connection does not occur.
  EXPECT_EQ(0u, connection_callback_responses_.size());

  // Timeout timer fires.
  InvokeTimerTask();
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_TRUE(connection_callback_responses_[0].empty());
  EXPECT_EQ(1u, test_network_connect_->num_disconnection_attempts());
  EXPECT_EQ(wifi_guid, test_network_connect_->network_id_to_disconnect());

  VerifyConnectionToHotspotDurationRecorded(false /* expected */);
}

TEST_F(WifiHotspotConnectorTest, TestConnect_DeletedWhileConnectionPending) {
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      std::string(kSsid), std::string(kPassword), kTetherNetworkGuid,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  std::string wifi_guid =
      VerifyLastConfiguration(std::string(kSsid), std::string(kPassword));
  EXPECT_FALSE(wifi_guid.empty());

  // Network becomes connectable.
  NotifyConnectable(test_network_connect_->last_service_path_created());
  VerifyTetherAndWifiNetworkAssociation(
      wifi_guid, kTetherNetworkGuid, 1u /* expected_num_connection_attempts */);
  EXPECT_EQ(wifi_guid, test_network_connect_->network_id_to_connect());
  EXPECT_EQ(0u, connection_callback_responses_.size());

  // Delete the connector; this should trigger a disconnection attempt.
  wifi_hotspot_connector_.reset();
  EXPECT_EQ(1u, test_network_connect_->num_disconnection_attempts());
  EXPECT_EQ(wifi_guid, test_network_connect_->network_id_to_disconnect());
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_EQ(std::string(), connection_callback_responses_[0]);
  VerifyConnectionToHotspotDurationRecorded(false /* expected */);
}

TEST_F(WifiHotspotConnectorTest, TestConnect_Success) {
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      std::string(kSsid), std::string(kPassword), kTetherNetworkGuid,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  std::string wifi_guid =
      VerifyLastConfiguration(std::string(kSsid), std::string(kPassword));
  EXPECT_FALSE(wifi_guid.empty());

  // Network becomes connectable.
  NotifyConnectable(test_network_connect_->last_service_path_created());
  VerifyTetherAndWifiNetworkAssociation(
      wifi_guid, kTetherNetworkGuid, 1u /* expected_num_connection_attempts */);
  EXPECT_EQ(wifi_guid, test_network_connect_->network_id_to_connect());
  EXPECT_EQ(0u, connection_callback_responses_.size());

  test_clock_.Advance(kConnectionToHotspotTime);

  // Connection to network successful.
  NotifyConnected(test_network_connect_->last_service_path_created());
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_EQ(wifi_guid, connection_callback_responses_[0]);
  VerifyTimerStopped();

  VerifyConnectionToHotspotDurationRecorded(true /* expected */);
  EXPECT_EQ(0u, test_network_connect_->num_disconnection_attempts());
}

TEST_F(WifiHotspotConnectorTest, TestConnect_Success_EmptyPassword) {
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      std::string(kSsid), std::string() /* password */, kTetherNetworkGuid,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  std::string wifi_guid = VerifyLastConfiguration(kSsid, std::string());
  EXPECT_FALSE(wifi_guid.empty());

  // Network becomes connectable.
  NotifyConnectable(test_network_connect_->last_service_path_created());
  VerifyTetherAndWifiNetworkAssociation(
      wifi_guid, kTetherNetworkGuid, 1u /* expected_num_connection_attempts */);
  EXPECT_EQ(wifi_guid, test_network_connect_->network_id_to_connect());
  EXPECT_EQ(0u, connection_callback_responses_.size());

  test_clock_.Advance(kConnectionToHotspotTime);

  // Connection to network successful.
  NotifyConnected(test_network_connect_->last_service_path_created());
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_EQ(wifi_guid, connection_callback_responses_[0]);
  VerifyTimerStopped();

  VerifyConnectionToHotspotDurationRecorded(true /* expected */);
  EXPECT_EQ(0u, test_network_connect_->num_disconnection_attempts());
}

TEST_F(WifiHotspotConnectorTest,
       TestConnect_SecondConnectionWhileWaitingForFirstToBecomeConnectable) {
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      "ssid1", "password1", "tetherNetworkGuid1",
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  std::string wifi_guid1 = VerifyLastConfiguration("ssid1", "password1");
  EXPECT_FALSE(wifi_guid1.empty());
  std::string service_path1 =
      test_network_connect_->last_service_path_created();
  EXPECT_FALSE(service_path1.empty());

  // Pass some arbitrary time -- this should not affect the
  // recorded duration because the start time should be reset
  // for a new network attempt.
  test_clock_.Advance(base::TimeDelta::FromSeconds(13));

  // Before network becomes connectable, start the new connection.
  EXPECT_EQ(0u, connection_callback_responses_.size());
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      "ssid2", "password2", kTetherNetworkGuid2,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  std::string wifi_guid2 = VerifyLastConfiguration("ssid2", "password2");
  EXPECT_FALSE(wifi_guid2.empty());
  std::string service_path2 =
      test_network_connect_->last_service_path_created();
  EXPECT_FALSE(service_path2.empty());
  VerifyNetworkNotAssociated(wifi_guid1);

  EXPECT_NE(service_path1, service_path2);

  // The original connection attempt should have gotten an empty response.
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_TRUE(connection_callback_responses_[0].empty());

  // First network becomes connectable.
  NotifyConnectable(service_path1);

  // A connection should not have started to that GUID.
  EXPECT_TRUE(test_network_connect_->network_id_to_connect().empty());
  EXPECT_EQ(1u, connection_callback_responses_.size());

  test_clock_.Advance(kConnectionToHotspotTime);

  // Second network becomes connectable.
  NotifyConnectable(service_path2);
  VerifyTetherAndWifiNetworkAssociation(
      wifi_guid2, kTetherNetworkGuid2,
      1u /* expected_num_connection_attempts */);
  EXPECT_EQ(wifi_guid2, test_network_connect_->network_id_to_connect());
  EXPECT_EQ(1u, connection_callback_responses_.size());

  // Connection to network successful.
  NotifyConnected(service_path2);
  EXPECT_EQ(2u, connection_callback_responses_.size());
  EXPECT_EQ(wifi_guid2, connection_callback_responses_[1]);
  VerifyTimerStopped();

  VerifyConnectionToHotspotDurationRecorded(true /* expected */);
  EXPECT_EQ(0u, test_network_connect_->num_disconnection_attempts());
}

TEST_F(WifiHotspotConnectorTest,
       TestConnect_SecondConnectionWhileWaitingForFirstToConnect) {
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      "ssid1", "password1", kTetherNetworkGuid,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  // Pass some arbitrary time -- this should not affect the
  // recorded duration because the start time should be reset
  // for a new network attempt.
  test_clock_.Advance(base::TimeDelta::FromSeconds(13));

  std::string wifi_guid1 = VerifyLastConfiguration("ssid1", "password1");
  EXPECT_FALSE(wifi_guid1.empty());
  std::string service_path1 =
      test_network_connect_->last_service_path_created();
  EXPECT_FALSE(service_path1.empty());

  // First network becomes connectable.
  NotifyConnectable(service_path1);
  VerifyTetherAndWifiNetworkAssociation(
      wifi_guid1, kTetherNetworkGuid,
      1u /* expected_num_connection_attempts */);

  // After network becomes connectable, request a connection to second network.
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      "ssid2", "password2", kTetherNetworkGuid2,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  // The first Tether and Wi-Fi networks should no longer be associated.
  VerifyNetworkNotAssociated(kTetherNetworkGuid);
  VerifyNetworkNotAssociated(wifi_guid1);

  // The original connection attempt should have gotten an empty response.
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_TRUE(connection_callback_responses_[0].empty());

  // A disconnection attempt should have been initiated to the other network.
  EXPECT_EQ(1u, test_network_connect_->num_disconnection_attempts());
  EXPECT_EQ(wifi_guid1, test_network_connect_->network_id_to_disconnect());

  std::string wifi_guid2 = VerifyLastConfiguration("ssid2", "password2");
  EXPECT_FALSE(wifi_guid2.empty());
  std::string service_path2 =
      test_network_connect_->last_service_path_created();
  EXPECT_FALSE(service_path2.empty());

  EXPECT_NE(service_path1, service_path2);

  test_clock_.Advance(kConnectionToHotspotTime);

  // Second network becomes connectable.
  NotifyConnectable(service_path2);
  VerifyTetherAndWifiNetworkAssociation(
      wifi_guid2, kTetherNetworkGuid2,
      2u /* expected_num_connection_attempts */);
  EXPECT_EQ(wifi_guid2, test_network_connect_->network_id_to_connect());
  EXPECT_EQ(1u, connection_callback_responses_.size());

  // Connection to network successful.
  NotifyConnected(service_path2);
  EXPECT_EQ(2u, connection_callback_responses_.size());
  EXPECT_EQ(wifi_guid2, connection_callback_responses_[1]);
  VerifyTimerStopped();

  VerifyConnectionToHotspotDurationRecorded(true /* expected */);
}

TEST_F(WifiHotspotConnectorTest, TestConnect_WifiDisabled_Success) {
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), false /* enabled */,
      chromeos::network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  wifi_hotspot_connector_->ConnectToWifiHotspot(
      std::string(kSsid), std::string(kPassword), kTetherNetworkGuid,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  // Allow the asyncronous call to NetworkStateHandler::SetTechnologyEnabled()
  // within WifiHotspotConnector::ConnectToWifiHotspot() to synchronously
  // run. After this call, Wi-Fi should be enabled and WifiHotspotConnector
  // will have called TestNetworkConnect::CreateConfiguration().
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  // Run the task and manually invoke ConfigureServiceWithLastNetworkConfig() to
  // prevent nested RunLoop errors.
  RunTestTaskRunner();
  test_network_connect_->ConfigureServiceWithLastNetworkConfig();

  std::string wifi_guid =
      VerifyLastConfiguration(std::string(kSsid), std::string(kPassword));
  EXPECT_FALSE(wifi_guid.empty());

  test_clock_.Advance(kConnectionToHotspotTime);

  // Network becomes connectable.
  NotifyConnectable(test_network_connect_->last_service_path_created());
  VerifyTetherAndWifiNetworkAssociation(
      wifi_guid, kTetherNetworkGuid, 1u /* expected_num_connection_attempts */);
  EXPECT_EQ(wifi_guid, test_network_connect_->network_id_to_connect());
  EXPECT_EQ(0u, connection_callback_responses_.size());

  // Connection to network successful.
  NotifyConnected(test_network_connect_->last_service_path_created());
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_EQ(wifi_guid, connection_callback_responses_[0]);
  VerifyTimerStopped();

  VerifyConnectionToHotspotDurationRecorded(true /* expected */);
  EXPECT_EQ(0u, test_network_connect_->num_disconnection_attempts());
}

TEST_F(WifiHotspotConnectorTest,
       TestConnect_WifiDisabled_Success_OtherDeviceStatesChange) {
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), false /* enabled */,
      chromeos::network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  wifi_hotspot_connector_->ConnectToWifiHotspot(
      std::string(kSsid), std::string(kPassword), kTetherNetworkGuid,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  // Ensure that WifiHotspotConnector only begins configuring the Wi-Fi network
  // once Wi-Fi is enabled.
  wifi_hotspot_connector_->DeviceListChanged();
  wifi_hotspot_connector_->DeviceListChanged();
  EXPECT_FALSE(test_network_connect_->last_configuration());

  // Allow the asyncronous call to NetworkStateHandler::SetTechnologyEnabled()
  // within WifiHotspotConnector::ConnectToWifiHotspot() to synchronously
  // run. After this call, Wi-Fi should be enabled and WifiHotspotConnector
  // will have called TestNetworkConnect::CreateConfiguration().
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  // Run the task and manually invoke ConfigureServiceWithLastNetworkConfig() to
  // prevent nested RunLoop errors.
  RunTestTaskRunner();
  test_network_connect_->ConfigureServiceWithLastNetworkConfig();

  std::string wifi_guid =
      VerifyLastConfiguration(std::string(kSsid), std::string(kPassword));
  EXPECT_FALSE(wifi_guid.empty());

  // Network becomes connectable.
  NotifyConnectable(test_network_connect_->last_service_path_created());
  VerifyTetherAndWifiNetworkAssociation(
      wifi_guid, kTetherNetworkGuid, 1u /* expected_num_connection_attempts */);
  EXPECT_EQ(wifi_guid, test_network_connect_->network_id_to_connect());
  EXPECT_EQ(0u, connection_callback_responses_.size());

  test_clock_.Advance(kConnectionToHotspotTime);

  // Connection to network successful.
  NotifyConnected(test_network_connect_->last_service_path_created());
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_EQ(wifi_guid, connection_callback_responses_[0]);
  VerifyTimerStopped();

  VerifyConnectionToHotspotDurationRecorded(true /* expected */);
  EXPECT_EQ(0u, test_network_connect_->num_disconnection_attempts());
}

TEST_F(WifiHotspotConnectorTest, TestConnect_WifiDisabled_AttemptTimesOut) {
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), false /* enabled */,
      chromeos::network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  wifi_hotspot_connector_->ConnectToWifiHotspot(
      std::string(kSsid), std::string(kPassword), kTetherNetworkGuid,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  // Timeout timer fires.
  InvokeTimerTask();
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_TRUE(connection_callback_responses_[0].empty());

  // Allow the asyncronous call to NetworkStateHandler::SetTechnologyEnabled()
  // within WifiHotspotConnector::ConnectToWifiHotspot() to synchronously
  // run. After this call, Wi-Fi should be enabled, but the connection attempt
  // has timed out and therefore a new Wi-Fi configuration should not exist.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  // No configuration should have been created since the connection attempt
  // timed out before Wi-Fi was successfully enabled.
  EXPECT_FALSE(test_network_connect_->last_configuration());

  VerifyConnectionToHotspotDurationRecorded(false /* expected */);
  EXPECT_EQ(0u, test_network_connect_->num_disconnection_attempts());
}

TEST_F(WifiHotspotConnectorTest,
       TestConnect_WifiDisabled_SecondConnectionWhileWaitingForWifiEnabled) {
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), false /* enabled */,
      chromeos::network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  wifi_hotspot_connector_->ConnectToWifiHotspot(
      "ssid1", "password1", kTetherNetworkGuid,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  // Pass some arbitrary time -- this should not affect the
  // recorded duration because the start time should be reset
  // for a new network attempt.
  test_clock_.Advance(base::TimeDelta::FromSeconds(13));

  EXPECT_FALSE(test_network_connect_->last_configuration());

  wifi_hotspot_connector_->ConnectToWifiHotspot(
      "ssid2", "password2", kTetherNetworkGuid2,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  // Allow the asyncronous call to NetworkStateHandler::SetTechnologyEnabled()
  // within WifiHotspotConnector::ConnectToWifiHotspot() to synchronously
  // run. After this call, Wi-Fi should be enabled and WifiHotspotConnector
  // will have called TestNetworkConnect::CreateConfiguration().
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  // Run the task and manually invoke ConfigureServiceWithLastNetworkConfig() to
  // prevent nested RunLoop errors.
  RunTestTaskRunner();
  test_network_connect_->ConfigureServiceWithLastNetworkConfig();

  std::string wifi_guid2 = VerifyLastConfiguration("ssid2", "password2");
  EXPECT_FALSE(wifi_guid2.empty());
  std::string service_path2 =
      test_network_connect_->last_service_path_created();
  EXPECT_FALSE(service_path2.empty());

  // Second network becomes connectable.
  NotifyConnectable(service_path2);
  VerifyTetherAndWifiNetworkAssociation(
      wifi_guid2, kTetherNetworkGuid2,
      1u /* expected_num_connection_attempts */);
  EXPECT_EQ(wifi_guid2, test_network_connect_->network_id_to_connect());
  EXPECT_EQ(1u, connection_callback_responses_.size());

  test_clock_.Advance(kConnectionToHotspotTime);

  // Connection to network successful.
  NotifyConnected(service_path2);
  EXPECT_EQ(2u, connection_callback_responses_.size());
  EXPECT_EQ(wifi_guid2, connection_callback_responses_[1]);
  VerifyTimerStopped();

  VerifyConnectionToHotspotDurationRecorded(true /* expected */);
  EXPECT_EQ(0u, test_network_connect_->num_disconnection_attempts());
}

}  // namespace tether

}  // namespace chromeos
