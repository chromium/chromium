// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_disconnector_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/ash/components/tether/fake_active_host.h"
#include "chromeos/ash/components/tether/fake_disconnect_tethering_request_sender.h"
#include "chromeos/ash/components/tether/fake_tether_connector.h"
#include "chromeos/ash/components/tether/fake_tether_session_completion_logger.h"
#include "chromeos/ash/components/tether/fake_wifi_hotspot_disconnector.h"
#include "chromeos/ash/components/tether/tether_session_completion_logger.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

namespace {

const char kSuccessResult[] = "success";
const char kWifiNetworkGuid[] = "wifiNetworkGuid";

}  // namespace

class TetherDisconnectorImplTest : public testing::Test {
 public:
  TetherDisconnectorImplTest()
      : test_devices_(multidevice::CreateRemoteDeviceRefListForTest(2u)) {}

  TetherDisconnectorImplTest(const TetherDisconnectorImplTest&) = delete;
  TetherDisconnectorImplTest& operator=(const TetherDisconnectorImplTest&) =
      delete;

  ~TetherDisconnectorImplTest() override = default;

  void SetUp() override {
    fake_active_host_ = std::make_unique<FakeActiveHost>();
    fake_wifi_hotspot_disconnector_ =
        std::make_unique<FakeWifiHotspotDisconnector>();
    fake_disconnect_tethering_request_sender_ =
        std::make_unique<FakeDisconnectTetheringRequestSender>();
    fake_tether_connector_ = std::make_unique<FakeTetherConnector>();
    device_id_tether_network_guid_map_ =
        std::make_unique<DeviceIdTetherNetworkGuidMap>();
    fake_tether_session_completion_logger_ =
        std::make_unique<FakeTetherSessionCompletionLogger>();

    tether_disconnector_ = std::make_unique<TetherDisconnectorImpl>(
        fake_active_host_.get(), fake_wifi_hotspot_disconnector_.get(),
        fake_disconnect_tethering_request_sender_.get(),
        fake_tether_connector_.get(), device_id_tether_network_guid_map_.get(),
        fake_tether_session_completion_logger_.get());
  }

  std::string GetTetherNetworkGuid(const std::string& device_id) {
    return device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
        device_id);
  }

  void SuccessCallback() { disconnection_result_ = kSuccessResult; }

  void ErrorCallback(const std::string& error_name) {
    disconnection_result_ = error_name;
  }

  void CallDisconnect(
      const std::string& tether_network_guid,
      const TetherSessionCompletionLogger::SessionCompletionReason&
          session_completion_reason) {
    tether_disconnector_->DisconnectFromNetwork(
        tether_network_guid,
        base::BindOnce(&TetherDisconnectorImplTest::SuccessCallback,
                       base::Unretained(this)),
        base::BindOnce(&TetherDisconnectorImplTest::ErrorCallback,
                       base::Unretained(this)),
        session_completion_reason);
  }

  std::string GetResultAndReset() {
    std::string result;
    result.swap(disconnection_result_);
    return result;
  }

  // Verifies that no Wi-Fi disconnection was requested and that no
  // DisconnectTetheringRequest message was sent.
  void VerifyNoDisconnectionOccurred() {
    EXPECT_TRUE(
        fake_wifi_hotspot_disconnector_->last_disconnected_wifi_network_guid()
            .empty());
    EXPECT_TRUE(
        fake_disconnect_tethering_request_sender_->device_ids_sent_requests()
            .empty());
  }

  void VerifySessionCompletionReasonRecorded(
      TetherSessionCompletionLogger::SessionCompletionReason
          expected_session_completion_reason) {
    EXPECT_EQ(expected_session_completion_reason,
              *fake_tether_session_completion_logger_
                   ->last_session_completion_reason());
  }

  void VerifySessionCompletionReasonNotRecorded() {
    EXPECT_FALSE(fake_tether_session_completion_logger_
                     ->last_session_completion_reason());
  }

  const multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<FakeWifiHotspotDisconnector> fake_wifi_hotspot_disconnector_;
  std::unique_ptr<FakeDisconnectTetheringRequestSender>
      fake_disconnect_tethering_request_sender_;
  std::unique_ptr<FakeTetherConnector> fake_tether_connector_;
  // TODO(hansberry): Use a fake for this when a real mapping scheme is created.
  std::unique_ptr<DeviceIdTetherNetworkGuidMap>
      device_id_tether_network_guid_map_;
  std::unique_ptr<FakeTetherSessionCompletionLogger>
      fake_tether_session_completion_logger_;

  std::string disconnection_result_;

  std::unique_ptr<TetherDisconnectorImpl> tether_disconnector_;
};

TEST_F(TetherDisconnectorImplTest, DisconnectWhenAlreadyDisconnected) {
  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
                 TetherSessionCompletionLogger::SessionCompletionReason::
                     USER_DISCONNECTED);
  EXPECT_EQ(NetworkConnectionHandler::kErrorNotConnected, GetResultAndReset());

  VerifyNoDisconnectionOccurred();
  VerifySessionCompletionReasonNotRecorded();

  // Should still be disconnected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

TEST_F(TetherDisconnectorImplTest, DisconnectWhenOtherDeviceConnected) {
  // Set device 1 as connected.
  fake_active_host_->SetActiveHostConnected(
      test_devices_[1].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[1].GetDeviceId()),
      "someWifiNetworkGuid");

  // Try to disconnect device 0; this should fail since the device is not
  // connected.
  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
                 TetherSessionCompletionLogger::SessionCompletionReason::
                     USER_DISCONNECTED);
  EXPECT_EQ(NetworkConnectionHandler::kErrorNotConnected, GetResultAndReset());

  VerifyNoDisconnectionOccurred();
  VerifySessionCompletionReasonNotRecorded();

  // Should still be connected to the other host.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTED,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[1].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
}

TEST_F(TetherDisconnectorImplTest, DisconnectWhenConnecting_CancelFails) {
  fake_active_host_->SetActiveHostConnecting(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  fake_tether_connector_->set_should_cancel_successfully(false);

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
                 TetherSessionCompletionLogger::SessionCompletionReason::
                     USER_DISCONNECTED);
  EXPECT_EQ(NetworkConnectionHandler::kErrorDisconnectFailed,
            GetResultAndReset());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
            fake_tether_connector_->last_canceled_tether_network_guid());

  VerifyNoDisconnectionOccurred();
  VerifySessionCompletionReasonNotRecorded();

  // Note: This test does not check the active host's status because it will be
  // changed by TetherConnector.
}

TEST_F(TetherDisconnectorImplTest, DisconnectWhenConnecting_CancelSucceeds) {
  fake_active_host_->SetActiveHostConnecting(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  fake_tether_connector_->set_should_cancel_successfully(true);

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
                 TetherSessionCompletionLogger::SessionCompletionReason::
                     USER_DISCONNECTED);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
            fake_tether_connector_->last_canceled_tether_network_guid());

  VerifyNoDisconnectionOccurred();
  VerifySessionCompletionReasonNotRecorded();

  // Note: This test does not check the active host's status because it will be
  // changed by TetherConnector.
}

TEST_F(TetherDisconnectorImplTest, DisconnectWhenConnected_Failure) {
  fake_active_host_->SetActiveHostConnected(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()), kWifiNetworkGuid);
  fake_wifi_hotspot_disconnector_->set_disconnection_error_name("failureName");

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
                 TetherSessionCompletionLogger::SessionCompletionReason::
                     USER_DISCONNECTED);
  EXPECT_EQ("failureName", GetResultAndReset());

  EXPECT_EQ(
      kWifiNetworkGuid,
      fake_wifi_hotspot_disconnector_->last_disconnected_wifi_network_guid());
  EXPECT_EQ(
      std::vector<std::string>{test_devices_[0].GetDeviceId()},
      fake_disconnect_tethering_request_sender_->device_ids_sent_requests());
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());

  VerifySessionCompletionReasonRecorded(
      TetherSessionCompletionLogger::SessionCompletionReason::
          USER_DISCONNECTED);
}

TEST_F(TetherDisconnectorImplTest, DisconnectWhenConnected_Success) {
  fake_active_host_->SetActiveHostConnected(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()), kWifiNetworkGuid);

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
                 TetherSessionCompletionLogger::SessionCompletionReason::
                     USER_DISCONNECTED);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());

  EXPECT_EQ(
      kWifiNetworkGuid,
      fake_wifi_hotspot_disconnector_->last_disconnected_wifi_network_guid());
  EXPECT_EQ(
      std::vector<std::string>{test_devices_[0].GetDeviceId()},
      fake_disconnect_tethering_request_sender_->device_ids_sent_requests());
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());

  VerifySessionCompletionReasonRecorded(
      TetherSessionCompletionLogger::SessionCompletionReason::
          USER_DISCONNECTED);
}

}  // namespace tether

}  // namespace ash
