// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/camera_roll_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/phonehub/camera_roll_item.h"
#include "chromeos/components/phonehub/fake_message_receiver.h"
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

}  // namespace

class CameraRollManagerTest : public testing::Test {
 protected:
  CameraRollManagerTest() = default;
  CameraRollManagerTest(const CameraRollManagerTest&) = delete;
  CameraRollManagerTest& operator=(const CameraRollManagerTest&) = delete;
  ~CameraRollManagerTest() override = default;

  void SetUp() override {
    camera_roll_manager_ =
        std::make_unique<CameraRollManager>(&fake_message_receiver_);
    camera_roll_manager_.get()->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    camera_roll_manager_.get()->RemoveObserver(&fake_observer_);
  }

  static void PopulateItemProto(proto::CameraRollItem* item_proto,
                                std::string key) {
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
  static void VerifyItemMatchesProto(const CameraRollItem* item,
                                     proto::CameraRollItem item_proto) {
    EXPECT_EQ(item_proto.metadata().key(), item->metadata().key());
    EXPECT_EQ(item_proto.metadata().mime_type(), item->metadata().mime_type());
    EXPECT_EQ(item_proto.metadata().last_modified_millis(),
              item->metadata().last_modified_millis());
    EXPECT_EQ(item_proto.metadata().file_size_bytes(),
              item->metadata().file_size_bytes());
  }

  void NotifyFetchCameraRollItemsResponseReceived(
      proto::FetchCameraRollItemsResponse response) {
    fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  }

  int GetOnCameraRollItemChangedCallCount() const {
    return fake_observer_.GetOnCameraRollItemChangedCallCount();
  }

  const CameraRollItem* GetReceivedItemAtIndex(int index) const {
    return camera_roll_manager_.get()->GetCurrentItems()[index];
  }

  size_t GetReceivedItemsCount() const {
    return camera_roll_manager_.get()->GetCurrentItems().size();
  }

 private:
  FakeMessageReceiver fake_message_receiver_;
  std::unique_ptr<CameraRollManager> camera_roll_manager_;
  FakeObserver fake_observer_;
};

TEST_F(CameraRollManagerTest, OnCameraRollItemsReceived) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key3");
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");

  NotifyFetchCameraRollItemsResponseReceived(response);

  EXPECT_EQ(1, GetOnCameraRollItemChangedCallCount());
  EXPECT_EQ(3UL, GetReceivedItemsCount());
  VerifyItemMatchesProto(GetReceivedItemAtIndex(0), response.items(0));
  VerifyItemMatchesProto(GetReceivedItemAtIndex(1), response.items(1));
  VerifyItemMatchesProto(GetReceivedItemAtIndex(2), response.items(2));
}

TEST_F(CameraRollManagerTest, OnCameraRollItemsReceivedWithExistingItems) {
  proto::FetchCameraRollItemsResponse first_response;
  PopulateItemProto(first_response.add_items(), "key3");
  PopulateItemProto(first_response.add_items(), "key2");
  PopulateItemProto(first_response.add_items(), "key1");

  NotifyFetchCameraRollItemsResponseReceived(first_response);

  EXPECT_EQ(3UL, GetReceivedItemsCount());

  proto::FetchCameraRollItemsResponse second_response;
  PopulateItemProto(second_response.add_items(), "key4");
  // Thumbnails won't be sent with the proto if an item's data is already
  // available and up-to-date.
  PopulateItemProto(second_response.add_items(), "key3");
  second_response.mutable_items(1)->clear_thumbnail();
  PopulateItemProto(second_response.add_items(), "key2");
  second_response.mutable_items(2)->clear_thumbnail();

  NotifyFetchCameraRollItemsResponseReceived(second_response);

  EXPECT_EQ(2, GetOnCameraRollItemChangedCallCount());
  EXPECT_EQ(3UL, GetReceivedItemsCount());
  VerifyItemMatchesProto(GetReceivedItemAtIndex(0), second_response.items(0));
  VerifyItemMatchesProto(GetReceivedItemAtIndex(1), second_response.items(1));
  VerifyItemMatchesProto(GetReceivedItemAtIndex(2), second_response.items(2));
}

}  // namespace phonehub
}  // namespace chromeos
