// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/camera_roll_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/phonehub/camera_roll_item.h"
#include "chromeos/components/phonehub/fake_message_receiver.h"
#include "chromeos/components/phonehub/fake_message_sender.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {
namespace {

class FakeObserver : public CameraRollManager::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  // CameraRollManager::Observer
  void OnCameraRollItemsChanged() override {
    on_camera_roll_items_changed_call_count_++;
  }

  int GetOnCameraRollItemChangedCallCount() const {
    return on_camera_roll_items_changed_call_count_;
  }

 private:
  int on_camera_roll_items_changed_call_count_ = 0;
};

void PopulateItemProto(proto::CameraRollItem* item_proto, std::string key) {
  proto::CameraRollItemMetadata* metadata = item_proto->mutable_metadata();
  metadata->set_key(key);
  metadata->set_mime_type("image/png");
  metadata->set_last_modified_millis(123456789L);
  metadata->set_file_size_bytes(123456789L);

  proto::CameraRollItemThumbnail* thumbnail = item_proto->mutable_thumbnail();
  thumbnail->set_format(proto::CameraRollItemThumbnail_Format_JPEG);
  thumbnail->set_data("encoded_thumbnail_data");
}

// Verifies that the metadata of a generated item matches the corresponding
// proto input.
// TODO(http://crbug.com/1221297): verify thumbnail data when implemented
void VerifyMetadataEqual(const proto::CameraRollItemMetadata& expected,
                         const proto::CameraRollItemMetadata& actual) {
  EXPECT_EQ(expected.key(), actual.key());
  EXPECT_EQ(expected.mime_type(), actual.mime_type());
  EXPECT_EQ(expected.last_modified_millis(), actual.last_modified_millis());
  EXPECT_EQ(expected.file_size_bytes(), actual.file_size_bytes());
}

}  // namespace

class CameraRollManagerTest : public testing::Test {
 protected:
  CameraRollManagerTest() = default;
  CameraRollManagerTest(const CameraRollManagerTest&) = delete;
  CameraRollManagerTest& operator=(const CameraRollManagerTest&) = delete;
  ~CameraRollManagerTest() override = default;

  void SetUp() override {
    camera_roll_manager_ = std::make_unique<CameraRollManager>(
        &fake_message_receiver_, &fake_message_sender_);
    camera_roll_manager_.get()->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    camera_roll_manager_.get()->RemoveObserver(&fake_observer_);
  }

  int GetOnCameraRollItemChangedCallCount() const {
    return fake_observer_.GetOnCameraRollItemChangedCallCount();
  }

  const CameraRollItem* GetReceivedItemAtIndex(int index) const {
    return camera_roll_manager_.get()->GetCurrentItems()[index];
  }

  size_t GetCurrentItemsCount() const {
    return camera_roll_manager_.get()->GetCurrentItems().size();
  }

  size_t GetSentFetchCameraRollItemsRequestCount() const {
    return fake_message_sender_.GetFetchCameraRollItemsRequestCallCount();
  }

  const proto::FetchCameraRollItemsRequest& GetSentFetchCameraRollItemsRequest()
      const {
    return fake_message_sender_.GetRecentFetchCameraRollItemsRequest();
  }

  FakeMessageReceiver fake_message_receiver_;

 private:
  FakeMessageSender fake_message_sender_;
  std::unique_ptr<CameraRollManager> camera_roll_manager_;
  FakeObserver fake_observer_;
};

TEST_F(CameraRollManagerTest, OnCameraRollItemsReceived) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key3");
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");

  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);

  EXPECT_EQ(1, GetOnCameraRollItemChangedCallCount());
  EXPECT_EQ(3UL, GetCurrentItemsCount());
  VerifyMetadataEqual(response.items(0).metadata(),
                      GetReceivedItemAtIndex(0)->metadata());
  VerifyMetadataEqual(response.items(1).metadata(),
                      GetReceivedItemAtIndex(1)->metadata());
  VerifyMetadataEqual(response.items(2).metadata(),
                      GetReceivedItemAtIndex(2)->metadata());
}

TEST_F(CameraRollManagerTest, OnCameraRollItemsReceivedWithExistingItems) {
  proto::FetchCameraRollItemsResponse first_response;
  PopulateItemProto(first_response.add_items(), "key3");
  PopulateItemProto(first_response.add_items(), "key2");
  PopulateItemProto(first_response.add_items(), "key1");

  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(
      first_response);

  EXPECT_EQ(3UL, GetCurrentItemsCount());

  proto::FetchCameraRollItemsResponse second_response;
  PopulateItemProto(second_response.add_items(), "key4");
  // Thumbnails won't be sent with the proto if an item's data is already
  // available and up-to-date.
  PopulateItemProto(second_response.add_items(), "key3");
  second_response.mutable_items(1)->clear_thumbnail();
  PopulateItemProto(second_response.add_items(), "key2");
  second_response.mutable_items(2)->clear_thumbnail();

  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(
      second_response);

  EXPECT_EQ(2, GetOnCameraRollItemChangedCallCount());
  EXPECT_EQ(3UL, GetCurrentItemsCount());
  VerifyMetadataEqual(second_response.items(0).metadata(),
                      GetReceivedItemAtIndex(0)->metadata());
  VerifyMetadataEqual(second_response.items(1).metadata(),
                      GetReceivedItemAtIndex(1)->metadata());
  VerifyMetadataEqual(second_response.items(2).metadata(),
                      GetReceivedItemAtIndex(2)->metadata());
}

TEST_F(CameraRollManagerTest,
       OnPhoneStatusUpdateReceivedWithoutCameraRollUpdates) {
  proto::PhoneStatusUpdate update;
  update.set_has_camera_roll_updates(false);
  proto::CameraRollAccessState* access_state =
      update.mutable_properties()->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(true);
  access_state->set_storage_permission_granted(true);
  fake_message_receiver_.NotifyPhoneStatusUpdateReceived(update);

  EXPECT_EQ(0UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(0, GetOnCameraRollItemChangedCallCount());
}

TEST_F(CameraRollManagerTest,
       OnPhoneStatusUpdateReceivedWithCameraRollUpdates) {
  proto::PhoneStatusUpdate update;
  update.set_has_camera_roll_updates(true);
  proto::CameraRollAccessState* access_state =
      update.mutable_properties()->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(true);
  access_state->set_storage_permission_granted(true);
  fake_message_receiver_.NotifyPhoneStatusUpdateReceived(update);

  EXPECT_EQ(1UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(0,
            GetSentFetchCameraRollItemsRequest().current_item_metadata_size());
  EXPECT_EQ(0, GetOnCameraRollItemChangedCallCount());
}

TEST_F(CameraRollManagerTest, OnPhoneStatusUpdateReceivedWithExistingItems) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key3");
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);

  proto::PhoneStatusUpdate update;
  update.set_has_camera_roll_updates(true);
  proto::CameraRollAccessState* access_state =
      update.mutable_properties()->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(true);
  access_state->set_storage_permission_granted(true);
  fake_message_receiver_.NotifyPhoneStatusUpdateReceived(update);

  EXPECT_EQ(1UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(1, GetOnCameraRollItemChangedCallCount());
  EXPECT_EQ(3,
            GetSentFetchCameraRollItemsRequest().current_item_metadata_size());
  VerifyMetadataEqual(
      response.items(0).metadata(),
      GetSentFetchCameraRollItemsRequest().current_item_metadata(0));
  VerifyMetadataEqual(
      response.items(1).metadata(),
      GetSentFetchCameraRollItemsRequest().current_item_metadata(1));
  VerifyMetadataEqual(
      response.items(2).metadata(),
      GetSentFetchCameraRollItemsRequest().current_item_metadata(2));
}

TEST_F(CameraRollManagerTest, OnPhoneStatusUpdateReceivedWithFeatureDisabled) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);

  proto::PhoneStatusUpdate update;
  proto::CameraRollAccessState* access_state =
      update.mutable_properties()->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(false);
  access_state->set_storage_permission_granted(true);
  fake_message_receiver_.NotifyPhoneStatusUpdateReceived(update);

  EXPECT_EQ(0UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(2, GetOnCameraRollItemChangedCallCount());
  EXPECT_EQ(0UL, GetCurrentItemsCount());
}

TEST_F(CameraRollManagerTest,
       OnPhoneStatusUpdateReceivedWithoutStoragePermission) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);

  proto::PhoneStatusUpdate update;
  proto::CameraRollAccessState* access_state =
      update.mutable_properties()->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(true);
  access_state->set_storage_permission_granted(false);
  fake_message_receiver_.NotifyPhoneStatusUpdateReceived(update);

  EXPECT_EQ(0UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(2, GetOnCameraRollItemChangedCallCount());
  EXPECT_EQ(0UL, GetCurrentItemsCount());
}

TEST_F(CameraRollManagerTest, OnPhoneStatusSnapshotReceived) {
  proto::PhoneStatusSnapshot snapshot;
  proto::CameraRollAccessState* access_state =
      snapshot.mutable_properties()->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(true);
  access_state->set_storage_permission_granted(true);
  fake_message_receiver_.NotifyPhoneStatusSnapshotReceived(snapshot);

  EXPECT_EQ(1UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(0, GetOnCameraRollItemChangedCallCount());
}

TEST_F(CameraRollManagerTest,
       OnPhoneStatusSnapshotReceivedWithFeatureDisabled) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);

  proto::PhoneStatusSnapshot snapshot;
  proto::CameraRollAccessState* access_state =
      snapshot.mutable_properties()->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(false);
  access_state->set_storage_permission_granted(true);
  fake_message_receiver_.NotifyPhoneStatusSnapshotReceived(snapshot);

  EXPECT_EQ(0UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(2, GetOnCameraRollItemChangedCallCount());
  EXPECT_EQ(0UL, GetCurrentItemsCount());
}

TEST_F(CameraRollManagerTest,
       OnPhoneStatusSnapshotReceivedWithoutStoragePermission) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);

  proto::PhoneStatusSnapshot snapshot;
  proto::CameraRollAccessState* access_state =
      snapshot.mutable_properties()->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(true);
  access_state->set_storage_permission_granted(false);
  fake_message_receiver_.NotifyPhoneStatusSnapshotReceived(snapshot);

  EXPECT_EQ(0UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(2, GetOnCameraRollItemChangedCallCount());
  EXPECT_EQ(0UL, GetCurrentItemsCount());
}

}  // namespace phonehub
}  // namespace chromeos
