// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_UI_READINESS_RECORDER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_UI_READINESS_RECORDER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/feature_status_provider.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"

namespace ash::phonehub {

// Keep in sync with the PhoneHubUiResult enum defined in
// //tools/metrics/histograms/metadata/phonehub/enums.xml.
enum class ConnectionFlowState {
  kSecureChannelNotConnected = 0,
  kSecureChannelConnected = 1,
  kCrosStateMessageSent = 2,
  kPhoneSnapShotReceivedButNoPhoneModelSet = 3,
  kUiConnected = 4,
  kMaxValue = kUiConnected,
};

// To record Phone Hub UI readiness state, i.e. secure channel connected,
// cros_state message sent, PhoneSanpShot message receivced and Phone Hub UI
// state is updated to connected.
class PhoneHubUiReadinessRecorder
    : FeatureStatusProvider::Observer,
      secure_channel::ConnectionManager::Observer {
 public:
  PhoneHubUiReadinessRecorder(
      FeatureStatusProvider* feature_status_provider,
      secure_channel::ConnectionManager* connection_manager);
  PhoneHubUiReadinessRecorder(const PhoneHubUiReadinessRecorder&) = delete;
  PhoneHubUiReadinessRecorder& operator=(const PhoneHubUiReadinessRecorder&) =
      delete;
  ~PhoneHubUiReadinessRecorder() override;

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // secure_channel::ConnectionManager::Observer:
  void OnConnectionStatusChanged() override;

  void RecordCrosStateMessageSent();
  void RecordPhoneStatusSnapShotReceived();
  void RecordPhoneHubUiConnected();

 private:
  FRIEND_TEST_ALL_PREFIXES(PhoneHubUiReadinessRecorderTest,
                           RecordMessageLatency);
  FRIEND_TEST_ALL_PREFIXES(PhoneHubUiReadinessRecorderTest,
                           RecordMessageSuccess);
  FRIEND_TEST_ALL_PREFIXES(PhoneHubUiReadinessRecorderTest,
                           RecordUiReadyLatency);
  FRIEND_TEST_ALL_PREFIXES(PhoneHubUiReadinessRecorderTest,
                           RecordUiReainessFailedAfterSecureChannelConnected);
  FRIEND_TEST_ALL_PREFIXES(PhoneHubUiReadinessRecorderTest,
                           RecordUiReainessFailedAfterCrosStateMessageSent);
  FRIEND_TEST_ALL_PREFIXES(PhoneHubUiReadinessRecorderTest,
                           RecordUiReainessFailedAfterPhoneSnapShotReceived);
  FRIEND_TEST_ALL_PREFIXES(PhoneHubUiReadinessRecorderTest,
                           RecordUiReainessDisconnectAfterSuccessfulConnection);

  void OnDisconnected();

  raw_ptr<FeatureStatusProvider> feature_status_provider_;
  raw_ptr<secure_channel::ConnectionManager> connection_manager_;
  bool is_cros_state_message_sent_ = false;
  bool is_phone_status_snapshot_processed_ = false;

  base::Time message_sent_timestamp_;

  base::Time secure_channel_connected_time_ = base::Time();

  ConnectionFlowState connection_flow_state_ =
      ConnectionFlowState::kSecureChannelNotConnected;
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_UI_READINESS_RECORDER_H_
