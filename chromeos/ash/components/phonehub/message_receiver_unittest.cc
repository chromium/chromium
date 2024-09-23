// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <netinet/in.h>

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/phonehub/message_receiver_impl.h"
#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::phonehub {

namespace {

class FakeObserver : public MessageReceiver::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t snapshot_num_calls() const {
    return phone_status_snapshot_updated_num_calls_;
  }

  size_t status_updated_num_calls() const {
    return phone_status_updated_num_calls_;
  }

  size_t feature_setup_response_num_calls() const {
    return feature_setup_response_num_calls_;
  }

  size_t fetch_camera_roll_items_response_calls() const {
    return fetch_camera_roll_items_response_calls_;
  }

  size_t fetch_camera_roll_item_data_response_calls() const {
    return fetch_camera_roll_item_data_response_calls_;
  }

  size_t ping_response_num_calls() const { return ping_response_num_calls_; }

  size_t app_stream_update_calls() const { return app_stream_update_calls_; }

  size_t app_list_update_calls() const { return app_list_update_calls_; }

  size_t app_list_incremental_update_calls() const {
    return app_list_incremental_update_calls_;
  }

  proto::PhoneStatusSnapshot last_snapshot() const { return last_snapshot_; }

  proto::PhoneStatusUpdate last_status_update() const {
    return last_status_update_;
  }

  proto::FeatureSetupResponse last_feature_setup_response() const {
    return last_feature_setup_response_;
  }

  proto::AppStreamUpdate last_app_stream_update() const {
    return last_app_stream_update_;
  }

  proto::AppListUpdate last_app_list_update() const {
    return last_app_list_update_;
  }

  proto::AppListIncrementalUpdate last_app_list_incremental_update() const {
    return last_app_list_incremental_update_;
  }

  proto::FetchCameraRollItemsResponse last_fetch_camera_roll_items_response()
      const {
    return last_fetch_camera_roll_items_response_;
  }

  proto::FetchCameraRollItemDataResponse
  last_fetch_camera_roll_item_data_response() const {
    return last_fetch_camera_roll_item_data_response_;
  }

  // MessageReceiver::Observer:
  void OnPhoneStatusSnapshotReceived(
      proto::PhoneStatusSnapshot phone_status_snapshot) override {
    last_snapshot_ = phone_status_snapshot;
    ++phone_status_snapshot_updated_num_calls_;
  }

  void OnPhoneStatusUpdateReceived(
      proto::PhoneStatusUpdate phone_status_update) override {
    last_status_update_ = phone_status_update;
    ++phone_status_updated_num_calls_;
  }

  void OnFeatureSetupResponseReceived(
      proto::FeatureSetupResponse feature_setup_response) override {
    last_feature_setup_response_ = feature_setup_response;
    ++feature_setup_response_num_calls_;
  }

  void OnFetchCameraRollItemsResponseReceived(
      const proto::FetchCameraRollItemsResponse& response) override {
    last_fetch_camera_roll_items_response_ = response;
    ++fetch_camera_roll_items_response_calls_;
  }

  void OnFetchCameraRollItemDataResponseReceived(
      const proto::FetchCameraRollItemDataResponse& response) override {
    last_fetch_camera_roll_item_data_response_ = response;
    ++fetch_camera_roll_item_data_response_calls_;
  }

  void OnAppStreamUpdateReceived(
      proto::AppStreamUpdate app_stream_update) override {
    last_app_stream_update_ = app_stream_update;
    ++app_stream_update_calls_;
  }

  void OnPingResponseReceived() override { ++ping_response_num_calls_; }

  void OnAppListUpdateReceived(proto::AppListUpdate app_list_update) override {
    last_app_list_update_ = app_list_update;
    ++app_list_update_calls_;
  }

  void OnAppListIncrementalUpdateReceived(
      proto::AppListIncrementalUpdate app_list_incremental_update) override {
    last_app_list_incremental_update_ = app_list_incremental_update;
    ++app_list_incremental_update_calls_;
  }

 private:
  size_t phone_status_snapshot_updated_num_calls_ = 0;
  size_t phone_status_updated_num_calls_ = 0;
  size_t feature_setup_response_num_calls_ = 0;
  size_t fetch_camera_roll_items_response_calls_ = 0;
  size_t fetch_camera_roll_item_data_response_calls_ = 0;
  size_t ping_response_num_calls_ = 0;
  size_t app_stream_update_calls_ = 0;
  size_t app_list_update_calls_ = 0;
  size_t app_list_incremental_update_calls_ = 0;
  proto::PhoneStatusSnapshot last_snapshot_;
  proto::PhoneStatusUpdate last_status_update_;
  proto::FeatureSetupResponse last_feature_setup_response_;
  proto::AppStreamUpdate last_app_stream_update_;
  proto::AppListUpdate last_app_list_update_;
  proto::AppListIncrementalUpdate last_app_list_incremental_update_;
  proto::FetchCameraRollItemsResponse last_fetch_camera_roll_items_response_;
  proto::FetchCameraRollItemDataResponse
      last_fetch_camera_roll_item_data_response_;
};

std::string SerializeMessage(proto::MessageType message_type,
                             const google::protobuf::MessageLite* request) {
  // Add two space characters, followed by the serialized proto.
  std::string message = base::StrCat({"  ", request->SerializeAsString()});

  // Replace the first two characters with |message_type| as a 16-bit int.
  uint16_t* ptr =
      reinterpret_cast<uint16_t*>(const_cast<char*>(message.data()));
  *ptr = htons(static_cast<uint16_t>(message_type));
  return message;
}

std::string SerializeMessageIncorrectly(
    proto::MessageType message_type,
    const google::protobuf::MessageLite* request) {
  // Add two space characters, followed by the serialized proto with junk.
  std::string message =
      base::StrCat({"  ", request->SerializeAsString(), "junk"});

  // Replace the first two characters with |message_type| as a 16-bit int.
  uint16_t* ptr =
      reinterpret_cast<uint16_t*>(const_cast<char*>(message.data()));
  *ptr = htons(static_cast<uint16_t>(message_type));

  // Overwrite the last byte to 0xFF results in malformed wire.
  message[message.size() - 1] = 0xFF;
  return message;
}

}  // namespace

class MessageReceiverImplTest : public testing::Test {
 protected:
  MessageReceiverImplTest()
      : fake_connection_manager_(
            std::make_unique<secure_channel::FakeConnectionManager>()) {}
  MessageReceiverImplTest(const MessageReceiverImplTest&) = delete;
  MessageReceiverImplTest& operator=(const MessageReceiverImplTest&) = delete;
  ~MessageReceiverImplTest() override = default;

  void SetUp() override {
    PhoneHubStructuredMetricsLogger::RegisterPrefs(pref_service_.registry());
    phone_hub_structured_metrics_logger_ =
        std::make_unique<PhoneHubStructuredMetricsLogger>(&pref_service_);
    message_receiver_ = std::make_unique<MessageReceiverImpl>(
        fake_connection_manager_.get(),
        phone_hub_structured_metrics_logger_.get());
    message_receiver_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    message_receiver_->RemoveObserver(&fake_observer_);
  }

  size_t GetNumPhoneStatusSnapshotCalls() const {
    return fake_observer_.snapshot_num_calls();
  }

  size_t GetNumPhoneStatusUpdatedCalls() const {
    return fake_observer_.status_updated_num_calls();
  }

  size_t GetNumFeatureSetupResponseCalls() const {
    return fake_observer_.feature_setup_response_num_calls();
  }

  size_t GetNumFetchCameraRollItemsResponseCalls() const {
    return fake_observer_.fetch_camera_roll_items_response_calls();
  }

  size_t GetNumFetchCameraRollItemDataResponseCalls() const {
    return fake_observer_.fetch_camera_roll_item_data_response_calls();
  }

  size_t GetNumPingResponseCalls() const {
    return fake_observer_.ping_response_num_calls();
  }

  size_t GetNumAppStreamUpdateCalls() const {
    return fake_observer_.app_stream_update_calls();
  }

  size_t GetNumAppListUpdateCalls() const {
    return fake_observer_.app_list_update_calls();
  }

  size_t GetNumAppListIncrementalUpdateCalls() const {
    return fake_observer_.app_list_incremental_update_calls();
  }

  proto::PhoneStatusSnapshot GetLastSnapshot() const {
    return fake_observer_.last_snapshot();
  }

  proto::PhoneStatusUpdate GetLastStatusUpdate() const {
    return fake_observer_.last_status_update();
  }

  proto::FeatureSetupResponse GetLastFeatureSetupResponse() const {
    return fake_observer_.last_feature_setup_response();
  }

  proto::FetchCameraRollItemsResponse GetLastFetchCameraRollItemsResponse()
      const {
    return fake_observer_.last_fetch_camera_roll_items_response();
  }

  proto::FetchCameraRollItemDataResponse
  GetLastFetchCameraRollItemDataResponse() const {
    return fake_observer_.last_fetch_camera_roll_item_data_response();
  }

  proto::AppStreamUpdate GetLastAppStreamUpdate() const {
    return fake_observer_.last_app_stream_update();
  }

  proto::AppListUpdate GetLastAppListUpdate() const {
    return fake_observer_.last_app_list_update();
  }

  proto::AppListIncrementalUpdate GetLastAppListIncrementalUpdate() const {
    return fake_observer_.last_app_list_incremental_update();
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  FakeObserver fake_observer_;
  std::unique_ptr<secure_channel::FakeConnectionManager>
      fake_connection_manager_;
  std::unique_ptr<PhoneHubStructuredMetricsLogger>
      phone_hub_structured_metrics_logger_;
  std::unique_ptr<MessageReceiverImpl> message_receiver_;
};

TEST_F(MessageReceiverImplTest, OnPhoneStatusSnapshotReceived) {
  const int32_t expected_battery_percentage = 15;
  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_battery_percentage(
      expected_battery_percentage);

  proto::PhoneStatusSnapshot expected_snapshot;
  expected_snapshot.set_allocated_properties(
      expected_phone_properties.release());
  expected_snapshot.add_notifications();

  // Simulate receiving a message.
  const std::string expected_message =
      SerializeMessage(proto::PHONE_STATUS_SNAPSHOT, &expected_snapshot);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  proto::PhoneStatusSnapshot actual_snapshot = GetLastSnapshot();

  EXPECT_EQ(1u, GetNumPhoneStatusSnapshotCalls());
  EXPECT_EQ(0u, GetNumPhoneStatusUpdatedCalls());
  EXPECT_EQ(0u, GetNumFetchCameraRollItemsResponseCalls());
  EXPECT_EQ(expected_battery_percentage,
            actual_snapshot.properties().battery_percentage());
  EXPECT_EQ(1, actual_snapshot.notifications_size());
}

TEST_F(MessageReceiverImplTest, OnPhoneStatusUpdated) {
  const int32_t expected_battery_percentage = 15u;
  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_battery_percentage(
      expected_battery_percentage);

  proto::PhoneStatusUpdate expected_update;
  expected_update.set_allocated_properties(expected_phone_properties.release());
  expected_update.add_updated_notifications();

  const int64_t expected_removed_id = 24u;
  expected_update.add_removed_notification_ids(expected_removed_id);

  // Simulate receiving a message.
  const std::string expected_message =
      SerializeMessage(proto::PHONE_STATUS_UPDATE, &expected_update);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  proto::PhoneStatusUpdate actual_update = GetLastStatusUpdate();

  EXPECT_EQ(0u, GetNumPhoneStatusSnapshotCalls());
  EXPECT_EQ(1u, GetNumPhoneStatusUpdatedCalls());
  EXPECT_EQ(0u, GetNumFetchCameraRollItemsResponseCalls());
  EXPECT_EQ(expected_battery_percentage,
            actual_update.properties().battery_percentage());
  EXPECT_EQ(1, actual_update.updated_notifications_size());
  EXPECT_EQ(expected_removed_id, actual_update.removed_notification_ids()[0]);
}

TEST_F(MessageReceiverImplTest,
       OnFeatrueSetupResponseReceivedWithFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPhoneHubFeatureSetupErrorHandling);

  proto::FeatureSetupResponse expected_response;
  expected_response.set_camera_roll_setup_result(
      proto::FeatureSetupResult::RESULT_PERMISSION_GRANTED);
  expected_response.set_notification_setup_result(
      proto::FeatureSetupResult::RESULT_PERMISSION_GRANTED);

  const std::string expected_message =
      SerializeMessage(proto::FEATURE_SETUP_RESPONSE, &expected_response);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  proto::FeatureSetupResponse actual_response = GetLastFeatureSetupResponse();

  EXPECT_EQ(0u, GetNumPhoneStatusSnapshotCalls());
  EXPECT_EQ(0u, GetNumPhoneStatusUpdatedCalls());
  EXPECT_EQ(1u, GetNumFeatureSetupResponseCalls());
  EXPECT_EQ(proto::FeatureSetupResult::RESULT_PERMISSION_GRANTED,
            actual_response.camera_roll_setup_result());
  EXPECT_EQ(proto::FeatureSetupResult::RESULT_PERMISSION_GRANTED,
            actual_response.notification_setup_result());
}

TEST_F(MessageReceiverImplTest,
       OnFeatrueSetupResponseReceivedWithFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPhoneHubFeatureSetupErrorHandling);

  proto::FeatureSetupResponse expected_response;
  expected_response.set_camera_roll_setup_result(
      proto::FeatureSetupResult::RESULT_PERMISSION_GRANTED);
  expected_response.set_notification_setup_result(
      proto::FeatureSetupResult::RESULT_PERMISSION_GRANTED);

  const std::string expected_message =
      SerializeMessage(proto::FEATURE_SETUP_RESPONSE, &expected_response);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  proto::FeatureSetupResponse actual_response = GetLastFeatureSetupResponse();

  EXPECT_EQ(0u, GetNumPhoneStatusSnapshotCalls());
  EXPECT_EQ(0u, GetNumPhoneStatusUpdatedCalls());
  EXPECT_EQ(0u, GetNumFeatureSetupResponseCalls());
}

TEST_F(MessageReceiverImplTest,
       OnFetchCameraRollItemsResponseReceivedWthFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPhoneHubCameraRoll);

  proto::FetchCameraRollItemsResponse expected_response;
  proto::CameraRollItem* item_proto = expected_response.add_items();
  proto::CameraRollItemMetadata* metadata = item_proto->mutable_metadata();
  metadata->set_key("key");
  proto::CameraRollItemThumbnail* thumbnail = item_proto->mutable_thumbnail();
  thumbnail->set_data("encoded_thumbnail_data");

  // Simulate receiving a message.
  const std::string expected_message = SerializeMessage(
      proto::FETCH_CAMERA_ROLL_ITEMS_RESPONSE, &expected_response);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  proto::FetchCameraRollItemsResponse actual_response =
      GetLastFetchCameraRollItemsResponse();

  EXPECT_EQ(0u, GetNumPhoneStatusSnapshotCalls());
  EXPECT_EQ(0u, GetNumPhoneStatusUpdatedCalls());
  EXPECT_EQ(0u, GetNumFeatureSetupResponseCalls());
  EXPECT_EQ(1u, GetNumFetchCameraRollItemsResponseCalls());
  EXPECT_EQ(1, actual_response.items_size());
  EXPECT_EQ("key", actual_response.items(0).metadata().key());
  EXPECT_EQ("encoded_thumbnail_data",
            actual_response.items(0).thumbnail().data());
}

TEST_F(MessageReceiverImplTest,
       OnFetchCameraRollItemsResponseReceivedWithFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kPhoneHubCameraRoll);

  proto::FetchCameraRollItemsResponse expected_response;
  proto::CameraRollItem* item_proto = expected_response.add_items();
  proto::CameraRollItemMetadata* metadata = item_proto->mutable_metadata();
  metadata->set_key("key");
  proto::CameraRollItemThumbnail* thumbnail = item_proto->mutable_thumbnail();
  thumbnail->set_data("encoded_thumbnail_data");

  // Simulate receiving a message.
  const std::string expected_message = SerializeMessage(
      proto::FETCH_CAMERA_ROLL_ITEMS_RESPONSE, &expected_response);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  EXPECT_EQ(0u, GetNumPhoneStatusSnapshotCalls());
  EXPECT_EQ(0u, GetNumPhoneStatusUpdatedCalls());
  EXPECT_EQ(0u, GetNumFeatureSetupResponseCalls());
  EXPECT_EQ(0u, GetNumFetchCameraRollItemsResponseCalls());
}

TEST_F(MessageReceiverImplTest,
       OnFetchCameraRollItemDataResponseReceivedWthFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPhoneHubCameraRoll);

  proto::FetchCameraRollItemDataResponse expected_response;
  expected_response.mutable_metadata()->set_key("key");
  expected_response.set_file_availability(
      proto::FetchCameraRollItemDataResponse::AVAILABLE);
  expected_response.set_payload_id(1234);

  // Simulate receiving a message.
  const std::string expected_message = SerializeMessage(
      proto::FETCH_CAMERA_ROLL_ITEM_DATA_RESPONSE, &expected_response);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  proto::FetchCameraRollItemDataResponse actual_response =
      GetLastFetchCameraRollItemDataResponse();

  EXPECT_EQ(0u, GetNumPhoneStatusSnapshotCalls());
  EXPECT_EQ(0u, GetNumPhoneStatusUpdatedCalls());
  EXPECT_EQ(0u, GetNumFeatureSetupResponseCalls());
  EXPECT_EQ(0u, GetNumFetchCameraRollItemsResponseCalls());
  EXPECT_EQ(1u, GetNumFetchCameraRollItemDataResponseCalls());
  EXPECT_EQ("key", actual_response.metadata().key());
  EXPECT_EQ(proto::FetchCameraRollItemDataResponse::AVAILABLE,
            actual_response.file_availability());
  EXPECT_EQ(1234, actual_response.payload_id());
}

TEST_F(MessageReceiverImplTest,
       OnFetchCameraRollItemDataResponseReceivedWithFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kPhoneHubCameraRoll);

  proto::FetchCameraRollItemDataResponse expected_response;
  expected_response.mutable_metadata()->set_key("key");
  expected_response.set_file_availability(
      proto::FetchCameraRollItemDataResponse::AVAILABLE);
  expected_response.set_payload_id(1234);

  // Simulate receiving a message.
  const std::string expected_message = SerializeMessage(
      proto::FETCH_CAMERA_ROLL_ITEM_DATA_RESPONSE, &expected_response);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  EXPECT_EQ(0u, GetNumPhoneStatusSnapshotCalls());
  EXPECT_EQ(0u, GetNumPhoneStatusUpdatedCalls());
  EXPECT_EQ(0u, GetNumFeatureSetupResponseCalls());
  EXPECT_EQ(0u, GetNumFetchCameraRollItemsResponseCalls());
  EXPECT_EQ(0u, GetNumFetchCameraRollItemDataResponseCalls());
}

TEST_F(MessageReceiverImplTest, OnPingResponseReceivedFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPhoneHubPingOnBubbleOpen);

  proto::PingResponse expected_response;

  // Simulate receiving a message
  const std::string expected_message =
      SerializeMessage(proto::PING_RESPONSE, &expected_response);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  EXPECT_EQ(1u, GetNumPingResponseCalls());
}

TEST_F(MessageReceiverImplTest, OnPingResponseReceivedFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kPhoneHubPingOnBubbleOpen);

  proto::PingResponse expected_response;

  // Simulate receiving a message
  const std::string expected_message =
      SerializeMessage(proto::PING_RESPONSE, &expected_response);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  EXPECT_EQ(0u, GetNumPingResponseCalls());
}

TEST_F(MessageReceiverImplTest, OnAppStreamUpdateReceived) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kEcheSWA);

  proto::AppStreamUpdate expected_app_stream_update;
  auto* app = expected_app_stream_update.mutable_foreground_app();
  app->set_user_id(12);
  app->set_package_name("package1");
  app->set_visible_name("visible1");

  // Simulate receiving a message.
  const std::string expected_message =
      SerializeMessage(proto::APP_STREAM_UPDATE, &expected_app_stream_update);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  proto::AppStreamUpdate actual_app_stream_update = GetLastAppStreamUpdate();

  EXPECT_EQ(1u, GetNumAppStreamUpdateCalls());
  EXPECT_EQ(12, actual_app_stream_update.foreground_app().user_id());
  EXPECT_EQ("package1",
            actual_app_stream_update.foreground_app().package_name());
  EXPECT_EQ("visible1",
            actual_app_stream_update.foreground_app().visible_name());
}

TEST_F(MessageReceiverImplTest, OnAppListUpdateReceived) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kEcheSWA);

  proto::AppListUpdate expected_app_list_update;
  auto* streamable_apps = expected_app_list_update.mutable_all_apps();
  streamable_apps->add_apps();

  // Simulate receiving a message.
  const std::string expected_message =
      SerializeMessage(proto::APP_LIST_UPDATE, &expected_app_list_update);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  proto::AppListUpdate actual_app_list_update = GetLastAppListUpdate();

  EXPECT_EQ(1u, GetNumAppListUpdateCalls());
  EXPECT_TRUE(actual_app_list_update.has_all_apps());
  EXPECT_EQ(1, actual_app_list_update.all_apps().apps_size());
}

TEST_F(MessageReceiverImplTest, OnAppListUpdateReceivedNoStreamableApps) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kEcheSWA);

  proto::AppListUpdate expected_app_list_update;

  // Simulate receiving a message.
  const std::string expected_message =
      SerializeMessage(proto::APP_LIST_UPDATE, &expected_app_list_update);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  proto::AppListUpdate actual_app_list_update = GetLastAppListUpdate();

  EXPECT_EQ(1u, GetNumAppListUpdateCalls());
  EXPECT_FALSE(actual_app_list_update.has_all_apps());
}

TEST_F(MessageReceiverImplTest, OnAppListUpdateReceivedFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kEcheSWA);

  proto::AppListUpdate expected_app_list_update;
  auto* streamable_apps = expected_app_list_update.mutable_all_apps();
  streamable_apps->add_apps();

  // Simulate receiving a message.
  const std::string expected_message =
      SerializeMessage(proto::APP_LIST_UPDATE, &expected_app_list_update);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  EXPECT_EQ(0u, GetNumAppListUpdateCalls());
}

TEST_F(MessageReceiverImplTest, OnAppListIncremenatlUpdateReceived) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kEcheSWA);

  proto::AppListIncrementalUpdate expected_app_list_incremental_update;
  auto* installed_app =
      expected_app_list_incremental_update.mutable_installed_apps();
  installed_app->add_apps();

  // Simulate receiving a message.
  const std::string expected_message =
      SerializeMessage(proto::APP_LIST_INCREMENTAL_UPDATE,
                       &expected_app_list_incremental_update);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  proto::AppListIncrementalUpdate actual_app_list_incremental_update =
      GetLastAppListIncrementalUpdate();

  EXPECT_EQ(1u, GetNumAppListIncrementalUpdateCalls());
  EXPECT_TRUE(expected_app_list_incremental_update.has_installed_apps());
  EXPECT_FALSE(expected_app_list_incremental_update.has_removed_apps());
  EXPECT_EQ(1,
            expected_app_list_incremental_update.installed_apps().apps_size());
}

TEST_F(MessageReceiverImplTest,
       OnAppListIncremenatlUpdateReceivedFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kEcheSWA);

  proto::AppListIncrementalUpdate expected_app_list_incremental_update;
  auto* installed_app =
      expected_app_list_incremental_update.mutable_installed_apps();
  installed_app->add_apps();

  // Simulate receiving a message.
  const std::string expected_message =
      SerializeMessage(proto::APP_LIST_INCREMENTAL_UPDATE,
                       &expected_app_list_incremental_update);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  EXPECT_EQ(0u, GetNumAppListIncrementalUpdateCalls());
}

TEST_F(MessageReceiverImplTest, OnMessageReceivedParseFailureStates) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kEcheSWA, features::kPhoneHubCameraRoll,
                            features::kPhoneHubFeatureSetupErrorHandling,
                            features::kPhoneHubPingOnBubbleOpen},
      /*disabled_features=*/{});

  std::string expected_message;

  proto::PhoneStatusSnapshot expected_snapshot;
  // Simulate receiving an incorrect message by malforming the last byte.
  expected_message = SerializeMessageIncorrectly(proto::PHONE_STATUS_SNAPSHOT,
                                                 &expected_snapshot);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  EXPECT_EQ(0u, GetNumPhoneStatusSnapshotCalls());

  proto::PhoneStatusUpdate expected_update;
  // Simulate receiving an incorrect message by malforming the last byte.
  expected_message =
      SerializeMessageIncorrectly(proto::PHONE_STATUS_UPDATE, &expected_update);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  EXPECT_EQ(0u, GetNumPhoneStatusUpdatedCalls());

  proto::FeatureSetupResponse expected_response;
  // Simulate receiving an incorrect message by malforming the last byte.
  expected_message = SerializeMessageIncorrectly(proto::FEATURE_SETUP_RESPONSE,
                                                 &expected_response);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  EXPECT_EQ(0u, GetNumFeatureSetupResponseCalls());

  proto::FetchCameraRollItemsResponse expected_items;
  // Simulate receiving an incorrect message by malforming the last byte.
  expected_message = SerializeMessageIncorrectly(
      proto::FETCH_CAMERA_ROLL_ITEMS_RESPONSE, &expected_items);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  EXPECT_EQ(0u, GetNumFetchCameraRollItemsResponseCalls());

  proto::FetchCameraRollItemDataResponse expected_data;
  // Simulate receiving an incorrect message by malforming the last byte.
  expected_message = SerializeMessageIncorrectly(
      proto::FETCH_CAMERA_ROLL_ITEM_DATA_RESPONSE, &expected_data);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  EXPECT_EQ(0u, GetNumFetchCameraRollItemDataResponseCalls());

  proto::AppStreamUpdate expected_app_stream_update;
  // Simulate receiving an incorrect message by malforming the last byte.
  expected_message = SerializeMessageIncorrectly(proto::APP_STREAM_UPDATE,
                                                 &expected_app_stream_update);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  EXPECT_EQ(0u, GetNumAppStreamUpdateCalls());

  proto::AppListUpdate expected_app_list_update;
  // Simulate receiving an incorrect message by malforming the last byte.
  expected_message = SerializeMessageIncorrectly(proto::APP_LIST_UPDATE,
                                                 &expected_app_list_update);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  EXPECT_EQ(0u, GetNumAppListUpdateCalls());

  proto::AppListIncrementalUpdate expected_incremental_update;
  // Simulate receiving an incorrect message by malforming the last byte.
  expected_message = SerializeMessageIncorrectly(
      proto::APP_LIST_INCREMENTAL_UPDATE, &expected_incremental_update);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  EXPECT_EQ(0u, GetNumAppListIncrementalUpdateCalls());
}

}  // namespace ash::phonehub
