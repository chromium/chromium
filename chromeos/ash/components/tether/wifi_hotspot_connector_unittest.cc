// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/wifi_hotspot_connector.h"

#include <memory>
#include <sstream>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/shill_property_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace tether {

using ::testing::IsEmpty;
using ::testing::Not;

namespace {

const char kSsid[] = "ssid";
const char kPassword[] = "password";

const char kOtherWifiServiceGuid[] = "otherWifiServiceGuid";

const char kTetherNetworkGuid[] = "tetherNetworkGuid";
const char kTetherNetworkGuid2[] = "tetherNetworkGuid2";

constexpr base::TimeDelta kConnectionToHotspotTime = base::Seconds(20);

std::string CreateConfigurationJsonString(const std::string& guid) {
  std::stringstream ss;
  ss << "{"
     << "  \"GUID\": \"" << guid << "\","
     << "  \"Type\": \"" << shill::kTypeWifi << "\","
     << "  \"State\": \"" << shill::kStateIdle << "\""
     << "}";
  return ss.str();
}

std::string GetSsid(const base::Value::Dict& shill_properties) {
  return shill_property_util::GetSSIDFromProperties(
      base::Value(shill_properties.Clone()), false, nullptr);
}

std::string GetSecurityClass(const base::Value::Dict& shill_properties) {
  const std::string* security =
      shill_properties.FindString(shill::kSecurityClassProperty);
  return security ? *security : std::string();
}

std::string GetPassphrase(const base::Value::Dict& shill_properties) {
  const std::string* passphrase =
      shill_properties.FindString(shill::kPassphraseProperty);
  return passphrase ? *passphrase : std::string();
}

std::string GetGuid(const base::Value::Dict& shill_properties) {
  const std::string* guid = shill_properties.FindString(shill::kGuidProperty);
  return guid ? *guid : std::string();
}

// Checks properties of the service described by shill_properties|:
//   - has a SSID |expected_ssid| and
//   - has a non-empty GUID.
// Returns true if all checks were successful, false otherwise.
// Fills the GUID of the service into |*out_guid|.
bool VerifyBaseConfiguration(const base::Value::Dict& shill_properties,
                             const std::string& expected_ssid,
                             std::string* out_guid) {
  bool ok = true;

  std::string actual_ssid = GetSsid(shill_properties);
  if (actual_ssid != expected_ssid) {
    ADD_FAILURE() << "Expected SSID '" << expected_ssid << "' but had '"
                  << actual_ssid;
    ok = false;
  }
  *out_guid = GetGuid(shill_properties);
  if (out_guid->empty()) {
    ADD_FAILURE() << "Had empty GUID";
    ok = false;
  }
  return ok;
}

// Checks properties of the service described by shill_properties|:
//   - has a SSID |expected_ssid|,
//   - has a non-empty GUID,
//   - has security class PSK and
//   - has the passphrase |expected_passphrase|.
// Returns true if all checks were successful, false otherwise.
// Fills the GUID of the service into |*out_guid|.
bool VerifyPskConfiguration(const base::Value::Dict& shill_properties,
                            const std::string& expected_ssid,
                            const std::string& expected_passphrase,
                            std::string* out_guid) {
  if (shill_properties.empty()) {
    ADD_FAILURE() << "Received empty shill_properties";
    return false;
  }
  bool ok = VerifyBaseConfiguration(shill_properties, expected_ssid, out_guid);

  std::string actual_security_class = GetSecurityClass(shill_properties);
  if (actual_security_class != shill::kSecurityClassPsk) {
    ADD_FAILURE() << "Expected security class '" << shill::kSecurityClassPsk
                  << "' but had '" << actual_security_class;
    ok = false;
  }
  std::string actual_passphrase = GetPassphrase(shill_properties);
  if (actual_security_class != shill::kSecurityClassPsk) {
    ADD_FAILURE() << "Expected passphrase '" << expected_passphrase
                  << "' but had '" << actual_passphrase;
    ok = false;
  }
  return ok;
}

// Checks properties of the service described by shill_properties|:
//   - has a SSID |expected_ssid|,
//   - has a non-empty GUID and
//   - has security class None.
// Returns true if all checks were successful, false otherwise.
// Fills the GUID of the service into |*out_guid|.
bool VerifyOpenConfiguration(const base::Value::Dict& shill_properties,
                             const std::string& expected_ssid,
                             std::string* out_guid) {
  bool ok = VerifyBaseConfiguration(shill_properties, expected_ssid, out_guid);

  std::string actual_security_class = GetSecurityClass(shill_properties);
  if (actual_security_class != shill::kSecurityNone) {
    ADD_FAILURE() << "Expected security class '" << shill::kSecurityNone
                  << "' but had '" << actual_security_class;
    ok = false;
  }
  return ok;
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

    // Returns a Dict object with no entries if no configuration has been passed
    // to TestNetworkConnect yet.
    const base::Value::Dict GetLastConfiguration() {
      return last_configuration_.empty() ? base::Value::Dict()
                                         : last_configuration_.Clone();
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
      ASSERT_FALSE(last_configuration_.empty());
      const std::string* wifi_guid =
          last_configuration_.FindString(shill::kGuidProperty);
      ASSERT_TRUE(wifi_guid);

      last_service_path_created_ =
          helper_->ConfigureService(CreateConfigurationJsonString(*wifi_guid));
    }

    // NetworkConnect:
    void SetTechnologyEnabled(const NetworkTypePattern& technology,
                              bool enabled_state) override {}
    void ShowMobileSetup(const std::string& network_id) override {}
    void ShowCarrierAccountDetail(const std::string& network_id) override {}
    void ShowPortalSignin(const std::string& service_path,
                          NetworkConnect::Source source) override {}
    void ConfigureNetworkIdAndConnect(const std::string& network_id,
                                      const base::Value& shill_properties,
                                      bool shared) override {}
    void CreateConfigurationAndConnect(base::Value::Dict shill_properties,
                                       bool shared) override {}

    void CreateConfiguration(base::Value::Dict shill_properties,
                             bool shared) override {
      EXPECT_FALSE(shared);
      last_configuration_ = std::move(shill_properties);

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
    // Has type base::Value::Type::NONE if no configuration has been passed to
    // TestNetworkConnect yet.
    base::Value::Dict last_configuration_;
    std::string last_service_path_created_;
    std::string network_id_to_connect_;
    std::string network_id_to_disconnect_;
    uint32_t num_connection_attempts_ = 0;
    uint32_t num_disconnection_attempts_ = 0;
    bool is_running_in_test_task_runner_ = false;
  };

  WifiHotspotConnectorTest() = default;

  WifiHotspotConnectorTest(const WifiHotspotConnectorTest&) = delete;
  WifiHotspotConnectorTest& operator=(const WifiHotspotConnectorTest&) = delete;

  ~WifiHotspotConnectorTest() override = default;

  void SetUp() override {
    other_wifi_service_path_.clear();
    connection_callback_responses_.clear();

    helper_.network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED);
    helper_.network_state_handler()->SetTechnologyEnabled(
        NetworkTypePattern::WiFi(), true /* enabled */,
        network_handler::ErrorCallback());
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

  void TearDown() override { wifi_hotspot_connector_.reset(); }

  void VerifyTimerSet() {
    EXPECT_TRUE(mock_timer_->IsRunning());
    EXPECT_EQ(base::Seconds(WifiHotspotConnector::kConnectionTimeoutSeconds),
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
};

TEST_F(WifiHotspotConnectorTest, TestConnect_NetworkDoesNotBecomeConnectable) {
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      std::string(kSsid), std::string(kPassword), kTetherNetworkGuid,
      base::BindOnce(&WifiHotspotConnectorTest::WifiConnectionCallback,
                     base::Unretained(this)));

  std::string guid_unused;
  EXPECT_TRUE(
      VerifyPskConfiguration(test_network_connect_->GetLastConfiguration(),
                             kSsid, kPassword, &guid_unused));

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
      base::BindOnce(&WifiHotspotConnectorTest::WifiConnectionCallback,
                     base::Unretained(this)));

  std::string wifi_guid;
  EXPECT_TRUE(
      VerifyPskConfiguration(test_network_connect_->GetLastConfiguration(),
                             kSsid, kPassword, &wifi_guid));

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
      base::BindOnce(&WifiHotspotConnectorTest::WifiConnectionCallback,
                     base::Unretained(this)));

  std::string wifi_guid;
  EXPECT_TRUE(
      VerifyPskConfiguration(test_network_connect_->GetLastConfiguration(),
                             kSsid, kPassword, &wifi_guid));

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
      base::BindOnce(&WifiHotspotConnectorTest::WifiConnectionCallback,
                     base::Unretained(this)));

  std::string wifi_guid;
  EXPECT_TRUE(
      VerifyPskConfiguration(test_network_connect_->GetLastConfiguration(),
                             kSsid, kPassword, &wifi_guid));

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
      base::BindOnce(&WifiHotspotConnectorTest::WifiConnectionCallback,
                     base::Unretained(this)));

  std::string wifi_guid;
  EXPECT_TRUE(
      VerifyPskConfiguration(test_network_connect_->GetLastConfiguration(),
                             kSsid, kPassword, &wifi_guid));

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
      base::BindOnce(&WifiHotspotConnectorTest::WifiConnectionCallback,
                     base::Unretained(this)));

  std::string wifi_guid;
  EXPECT_TRUE(VerifyOpenConfiguration(
      test_network_connect_->GetLastConfiguration(), kSsid, &wifi_guid));

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
      base::BindOnce(&WifiHotspotConnectorTest::WifiConnectionCallback,
                     base::Unretained(this)));

  std::string wifi_guid_1;
  EXPECT_TRUE(
      VerifyPskConfiguration(test_network_connect_->GetLastConfiguration(),
                             "ssid1", "password1", &wifi_guid_1));
  std::string service_path_1 =
      test_network_connect_->last_service_path_created();
  EXPECT_FALSE(service_path_1.empty());

  // Pass some arbitrary time -- this should not affect the
  // recorded duration because the start time should be reset
  // for a new network attempt.
  test_clock_.Advance(base::Seconds(13));

  // Before network becomes connectable, start the new connection.
  EXPECT_EQ(0u, connection_callback_responses_.size());
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      "ssid2", "password2", kTetherNetworkGuid2,
      base::BindOnce(&WifiHotspotConnectorTest::WifiConnectionCallback,
                     base::Unretained(this)));

  std::string wifi_guid_2;
  EXPECT_TRUE(
      VerifyPskConfiguration(test_network_connect_->GetLastConfiguration(),
                             "ssid2", "password2", &wifi_guid_2));
  std::string service_path_2 =
      test_network_connect_->last_service_path_created();
  EXPECT_FALSE(service_path_2.empty());
  VerifyNetworkNotAssociated(wifi_guid_1);

  EXPECT_NE(service_path_1, service_path_2);

  // The original connection attempt should have gotten an empty response.
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_TRUE(connection_callback_responses_[0].empty());

  // First network becomes connectable.
  NotifyConnectable(service_path_1);

  // A connection should not have started to that GUID.
  EXPECT_TRUE(test_network_connect_->network_id_to_connect().empty());
  EXPECT_EQ(1u, connection_callback_responses_.size());

  test_clock_.Advance(kConnectionToHotspotTime);

  // Second network becomes connectable.
  NotifyConnectable(service_path_2);
  VerifyTetherAndWifiNetworkAssociation(
      wifi_guid_2, kTetherNetworkGuid2,
      1u /* expected_num_connection_attempts */);
  EXPECT_EQ(wifi_guid_2, test_network_connect_->network_id_to_connect());
  EXPECT_EQ(1u, connection_callback_responses_.size());

  // Connection to network successful.
  NotifyConnected(service_path_2);
  EXPECT_EQ(2u, connection_callback_responses_.size());
  EXPECT_EQ(wifi_guid_2, connection_callback_responses_[1]);
  VerifyTimerStopped();

  VerifyConnectionToHotspotDurationRecorded(true /* expected */);
  EXPECT_EQ(0u, test_network_connect_->num_disconnection_attempts());
}

TEST_F(WifiHotspotConnectorTest,
       TestConnect_SecondConnectionWhileWaitingForFirstToConnect) {
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      "ssid1", "password1", kTetherNetworkGuid,
      base::BindOnce(&WifiHotspotConnectorTest::WifiConnectionCallback,
                     base::Unretained(this)));

  // Pass some arbitrary time -- this should not affect the
  // recorded duration because the start time should be reset
  // for a new network attempt.
  test_clock_.Advance(base::Seconds(13));

  std::string wifi_guid_1;
  EXPECT_TRUE(
      VerifyPskConfiguration(test_network_connect_->GetLastConfiguration(),
                             "ssid1", "password1", &wifi_guid_1));
  std::string service_path_1 =
      test_network_connect_->last_service_path_created();
  EXPECT_FALSE(service_path_1.empty());

  // First network becomes connectable.
  NotifyConnectable(service_path_1);
  VerifyTetherAndWifiNetworkAssociation(
      wifi_guid_1, kTetherNetworkGuid,
      1u /* expected_num_connection_attempts */);

  // After network becomes connectable, request a connection to second network.
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      "ssid2", "password2", kTetherNetworkGuid2,
      base::BindOnce(&WifiHotspotConnectorTest::WifiConnectionCallback,
                     base::Unretained(this)));

  // The first Tether and Wi-Fi networks should no longer be associated.
  VerifyNetworkNotAssociated(kTetherNetworkGuid);
  VerifyNetworkNotAssociated(wifi_guid_1);

  // The original connection attempt should have gotten an empty response.
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_TRUE(connection_callback_responses_[0].empty());

  // A disconnection attempt should have been initiated to the other network.
  EXPECT_EQ(1u, test_network_connect_->num_disconnection_attempts());
  EXPECT_EQ(wifi_guid_1, test_network_connect_->network_id_to_disconnect());

  std::string wifi_guid_2;
  EXPECT_TRUE(
      VerifyPskConfiguration(test_network_connect_->GetLastConfiguration(),
                             "ssid2", "password2", &wifi_guid_2));
  std::string service_path_2 =
      test_network_connect_->last_service_path_created();
  EXPECT_FALSE(service_path_2.empty());

  EXPECT_NE(service_path_1, service_path_2);

  test_clock_.Advance(kConnectionToHotspotTime);

  // Second network becomes connectable.
  NotifyConnectable(service_path_2);
  VerifyTetherAndWifiNetworkAssociation(
      wifi_guid_2, kTetherNetworkGuid2,
      2u /* expected_num_connection_attempts */);
  EXPECT_EQ(wifi_guid_2, test_network_connect_->network_id_to_connect());
  EXPECT_EQ(1u, connection_callback_responses_.size());

  // Connection to network successful.
  NotifyConnected(service_path_2);
  EXPECT_EQ(2u, connection_callback_responses_.size());
  EXPECT_EQ(wifi_guid_2, connection_callback_responses_[1]);
  VerifyTimerStopped();

  VerifyConnectionToHotspotDurationRecorded(true /* expected */);
}

TEST_F(WifiHotspotConnectorTest, TestConnect_WifiDisabled_Success) {
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), false /* enabled */,
      network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  wifi_hotspot_connector_->ConnectToWifiHotspot(
      std::string(kSsid), std::string(kPassword), kTetherNetworkGuid,
      base::BindOnce(&WifiHotspotConnectorTest::WifiConnectionCallback,
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

  std::string wifi_guid;
  EXPECT_TRUE(
      VerifyPskConfiguration(test_network_connect_->GetLastConfiguration(),
                             kSsid, kPassword, &wifi_guid));

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
      network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  wifi_hotspot_connector_->ConnectToWifiHotspot(
      std::string(kSsid), std::string(kPassword), kTetherNetworkGuid,
      base::BindOnce(&WifiHotspotConnectorTest::WifiConnectionCallback,
                     base::Unretained(this)));

  // Ensure that WifiHotspotConnector only begins configuring the Wi-Fi network
  // once Wi-Fi is enabled.
  wifi_hotspot_connector_->DeviceListChanged();
  wifi_hotspot_connector_->DeviceListChanged();
  EXPECT_TRUE(test_network_connect_->GetLastConfiguration().empty());

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

  std::string wifi_guid;
  EXPECT_TRUE(
      VerifyPskConfiguration(test_network_connect_->GetLastConfiguration(),
                             kSsid, kPassword, &wifi_guid));

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
      network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  wifi_hotspot_connector_->ConnectToWifiHotspot(
      std::string(kSsid), std::string(kPassword), kTetherNetworkGuid,
      base::BindOnce(&WifiHotspotConnectorTest::WifiConnectionCallback,
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
  EXPECT_TRUE(test_network_connect_->GetLastConfiguration().empty());

  VerifyConnectionToHotspotDurationRecorded(false /* expected */);
  EXPECT_EQ(0u, test_network_connect_->num_disconnection_attempts());
}

TEST_F(WifiHotspotConnectorTest,
       TestConnect_WifiDisabled_SecondConnectionWhileWaitingForWifiEnabled) {
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), false /* enabled */,
      network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  wifi_hotspot_connector_->ConnectToWifiHotspot(
      "ssid1", "password1", kTetherNetworkGuid,
      base::BindOnce(&WifiHotspotConnectorTest::WifiConnectionCallback,
                     base::Unretained(this)));

  // Pass some arbitrary time -- this should not affect the
  // recorded duration because the start time should be reset
  // for a new network attempt.
  test_clock_.Advance(base::Seconds(13));

  EXPECT_TRUE(test_network_connect_->GetLastConfiguration().empty());

  wifi_hotspot_connector_->ConnectToWifiHotspot(
      "ssid2", "password2", kTetherNetworkGuid2,
      base::BindOnce(&WifiHotspotConnectorTest::WifiConnectionCallback,
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

  std::string wifi_guid_2;
  EXPECT_TRUE(
      VerifyPskConfiguration(test_network_connect_->GetLastConfiguration(),
                             "ssid2", "password2", &wifi_guid_2));
  std::string service_path_2 =
      test_network_connect_->last_service_path_created();
  EXPECT_FALSE(service_path_2.empty());

  // Second network becomes connectable.
  NotifyConnectable(service_path_2);
  VerifyTetherAndWifiNetworkAssociation(
      wifi_guid_2, kTetherNetworkGuid2,
      1u /* expected_num_connection_attempts */);
  EXPECT_EQ(wifi_guid_2, test_network_connect_->network_id_to_connect());
  EXPECT_EQ(1u, connection_callback_responses_.size());

  test_clock_.Advance(kConnectionToHotspotTime);

  // Connection to network successful.
  NotifyConnected(service_path_2);
  EXPECT_EQ(2u, connection_callback_responses_.size());
  EXPECT_EQ(wifi_guid_2, connection_callback_responses_[1]);
  VerifyTimerStopped();

  VerifyConnectionToHotspotDurationRecorded(true /* expected */);
  EXPECT_EQ(0u, test_network_connect_->num_disconnection_attempts());
}

}  // namespace tether

}  // namespace ash
