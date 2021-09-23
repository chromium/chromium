// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/camera_roll_manager_impl.h"

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/phonehub/camera_roll_item.h"
#include "chromeos/components/phonehub/camera_roll_thumbnail_decoder_impl.h"
#include "chromeos/components/phonehub/fake_message_receiver.h"
#include "chromeos/components/phonehub/fake_message_sender.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {
namespace phonehub {
namespace {

using BatchDecodeResult = CameraRollThumbnailDecoder::BatchDecodeResult;
using BatchDecodeCallback =
    base::OnceCallback<void(BatchDecodeResult,
                            const std::vector<CameraRollItem>&)>;

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
void VerifyMetadataEqual(const proto::CameraRollItemMetadata& expected,
                         const proto::CameraRollItemMetadata& actual) {
  EXPECT_EQ(expected.key(), actual.key());
  EXPECT_EQ(expected.mime_type(), actual.mime_type());
  EXPECT_EQ(expected.last_modified_millis(), actual.last_modified_millis());
  EXPECT_EQ(expected.file_size_bytes(), actual.file_size_bytes());
}

}  // namespace

class FakeThumbnailDecoder : public CameraRollThumbnailDecoder {
 public:
  FakeThumbnailDecoder() = default;
  ~FakeThumbnailDecoder() override = default;

  void BatchDecode(const proto::FetchCameraRollItemsResponse& response,
                   const std::vector<CameraRollItem>& current_items,
                   BatchDecodeCallback callback) override {
    if (!pending_callback_.is_null()) {
      CompletePendingCallback(BatchDecodeResult::kCancelled);
    }
    last_response_ = response;
    pending_callback_ = std::move(callback);
  }

  void CompletePendingCallback(BatchDecodeResult result) {
    std::vector<CameraRollItem> items;
    if (result == BatchDecodeResult::kSuccess) {
      for (const proto::CameraRollItem& item_proto : last_response_.items()) {
        SkBitmap test_bitmap;
        test_bitmap.allocN32Pixels(1, 1);
        gfx::ImageSkia image_skia =
            gfx::ImageSkia::CreateFrom1xBitmap(test_bitmap);
        image_skia.MakeThreadSafe();
        gfx::Image thumbnail(image_skia);
        items.emplace_back(item_proto.metadata(), thumbnail);
      }
    }
    std::move(pending_callback_).Run(result, items);
  }

 private:
  proto::FetchCameraRollItemsResponse last_response_;
  BatchDecodeCallback pending_callback_;
};

class CameraRollManagerImplTest : public testing::Test {
 protected:
  CameraRollManagerImplTest() = default;
  CameraRollManagerImplTest(const CameraRollManagerImplTest&) = delete;
  CameraRollManagerImplTest& operator=(const CameraRollManagerImplTest&) =
      delete;
  ~CameraRollManagerImplTest() override = default;

  void SetUp() override {
    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
    SetCameraRollFeatureSettings(true);
    camera_roll_manager_ = std::make_unique<CameraRollManagerImpl>(
        &fake_message_receiver_, &fake_message_sender_,
        fake_multidevice_setup_client_.get());
    camera_roll_manager_->thumbnail_decoder_ =
        std::make_unique<FakeThumbnailDecoder>();
    camera_roll_manager_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    camera_roll_manager_->RemoveObserver(&fake_observer_);
  }

  int GetOnCameraRollItemChangedCallCount() const {
    return fake_observer_.GetOnCameraRollItemChangedCallCount();
  }

  int GetCurrentItemsCount() const {
    return camera_roll_manager_->current_items().size();
  }

  size_t GetSentFetchCameraRollItemsRequestCount() const {
    return fake_message_sender_.GetFetchCameraRollItemsRequestCallCount();
  }

  const proto::FetchCameraRollItemsRequest& GetSentFetchCameraRollItemsRequest()
      const {
    return fake_message_sender_.GetRecentFetchCameraRollItemsRequest();
  }

  // Verifies current items match the list of items in the last received
  // |FetchCameraRollItemsResponse|, and their thumbnails have been properly
  // decoded.
  void VerifyCurrentItemsMatchResponse(
      const proto::FetchCameraRollItemsResponse& response) const {
    EXPECT_EQ(response.items_size(), GetCurrentItemsCount());
    for (int i = 0; i < GetCurrentItemsCount(); i++) {
      const CameraRollItem& current_item =
          camera_roll_manager_->current_items()[i];
      VerifyMetadataEqual(response.items(i).metadata(),
                          current_item.metadata());
      EXPECT_FALSE(current_item.thumbnail().IsEmpty());
    }
  }

  void CompleteThumbnailDecoding(BatchDecodeResult result) {
    static_cast<FakeThumbnailDecoder*>(
        camera_roll_manager_->thumbnail_decoder_.get())
        ->CompletePendingCallback(result);
  }

  void SetCameraRollFeatureSettings(bool enabled) {
    if (enabled) {
      fake_multidevice_setup_client_->SetFeatureState(
          multidevice_setup::mojom::Feature::kPhoneHubCameraRoll,
          multidevice_setup::mojom::FeatureState::kEnabledByUser);
    } else {
      fake_multidevice_setup_client_->SetFeatureState(
          multidevice_setup::mojom::Feature::kPhoneHubCameraRoll,
          multidevice_setup::mojom::FeatureState::kDisabledByUser);
    }
  }

  FakeMessageReceiver fake_message_receiver_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;

 private:
  FakeMessageSender fake_message_sender_;
  std::unique_ptr<CameraRollManagerImpl> camera_roll_manager_;
  FakeObserver fake_observer_;
};

TEST_F(CameraRollManagerImplTest, OnCameraRollItemsReceived) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key3");
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");

  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kSuccess);

  EXPECT_EQ(1, GetOnCameraRollItemChangedCallCount());
  VerifyCurrentItemsMatchResponse(response);
}

TEST_F(CameraRollManagerImplTest,
       OnCameraRollItemsReceivedWithThumbnailDecodingError) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key3");
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");

  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kError);

  EXPECT_EQ(0, GetOnCameraRollItemChangedCallCount());
  EXPECT_EQ(0, GetCurrentItemsCount());
}

TEST_F(CameraRollManagerImplTest,
       OnCameraRollItemsReceivedWithPendingThumbnailDecodedRequest) {
  proto::FetchCameraRollItemsResponse first_response;
  PopulateItemProto(first_response.add_items(), "key2");
  PopulateItemProto(first_response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(
      first_response);

  proto::FetchCameraRollItemsResponse second_response;
  PopulateItemProto(second_response.add_items(), "key4");
  PopulateItemProto(second_response.add_items(), "key3");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(
      second_response);
  CompleteThumbnailDecoding(BatchDecodeResult::kSuccess);

  // The first thumbnail decode request should be cancelled and the current item
  // set should be updated only once after the second request completes.
  EXPECT_EQ(1, GetOnCameraRollItemChangedCallCount());
  VerifyCurrentItemsMatchResponse(second_response);
}

TEST_F(CameraRollManagerImplTest, OnCameraRollItemsReceivedWithExistingItems) {
  proto::FetchCameraRollItemsResponse first_response;
  PopulateItemProto(first_response.add_items(), "key3");
  PopulateItemProto(first_response.add_items(), "key2");
  PopulateItemProto(first_response.add_items(), "key1");

  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(
      first_response);
  CompleteThumbnailDecoding(BatchDecodeResult::kSuccess);
  VerifyCurrentItemsMatchResponse(first_response);

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
  CompleteThumbnailDecoding(BatchDecodeResult::kSuccess);
  EXPECT_EQ(2, GetOnCameraRollItemChangedCallCount());
  VerifyCurrentItemsMatchResponse(second_response);
}

TEST_F(CameraRollManagerImplTest,
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

TEST_F(CameraRollManagerImplTest,
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

TEST_F(CameraRollManagerImplTest,
       OnPhoneStatusUpdateReceivedWithExistingItems) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key3");
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kSuccess);

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

TEST_F(CameraRollManagerImplTest,
       OnPhoneStatusUpdateReceivedWithFeatureDisabled) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kSuccess);

  proto::PhoneStatusUpdate update;
  proto::CameraRollAccessState* access_state =
      update.mutable_properties()->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(false);
  access_state->set_storage_permission_granted(true);
  fake_message_receiver_.NotifyPhoneStatusUpdateReceived(update);

  EXPECT_EQ(0UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(2, GetOnCameraRollItemChangedCallCount());
  EXPECT_EQ(0, GetCurrentItemsCount());
}

TEST_F(CameraRollManagerImplTest,
       OnPhoneStatusUpdateReceivedWithCameraRollSettingsDisabled) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kSuccess);

  proto::PhoneStatusUpdate update;
  update.set_has_camera_roll_updates(true);
  proto::CameraRollAccessState* access_state =
      update.mutable_properties()->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(true);
  access_state->set_storage_permission_granted(true);
  SetCameraRollFeatureSettings(false);
  fake_message_receiver_.NotifyPhoneStatusUpdateReceived(update);

  EXPECT_EQ(0UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(2, GetOnCameraRollItemChangedCallCount());
  EXPECT_EQ(0, GetCurrentItemsCount());
}

TEST_F(CameraRollManagerImplTest,
       OnPhoneStatusUpdateReceivedWithoutStoragePermission) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kSuccess);

  proto::PhoneStatusUpdate update;
  proto::CameraRollAccessState* access_state =
      update.mutable_properties()->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(true);
  access_state->set_storage_permission_granted(false);
  fake_message_receiver_.NotifyPhoneStatusUpdateReceived(update);

  EXPECT_EQ(0UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(2, GetOnCameraRollItemChangedCallCount());
  EXPECT_EQ(0, GetCurrentItemsCount());
}

TEST_F(CameraRollManagerImplTest, OnPhoneStatusSnapshotReceived) {
  proto::PhoneStatusSnapshot snapshot;
  proto::CameraRollAccessState* access_state =
      snapshot.mutable_properties()->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(true);
  access_state->set_storage_permission_granted(true);
  fake_message_receiver_.NotifyPhoneStatusSnapshotReceived(snapshot);

  EXPECT_EQ(1UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(0, GetOnCameraRollItemChangedCallCount());
}

TEST_F(CameraRollManagerImplTest,
       OnPhoneStatusSnapshotReceivedWithFeatureDisabled) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kSuccess);

  proto::PhoneStatusSnapshot snapshot;
  proto::CameraRollAccessState* access_state =
      snapshot.mutable_properties()->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(false);
  access_state->set_storage_permission_granted(true);
  fake_message_receiver_.NotifyPhoneStatusSnapshotReceived(snapshot);

  EXPECT_EQ(0UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(2, GetOnCameraRollItemChangedCallCount());
  EXPECT_EQ(0, GetCurrentItemsCount());
}

TEST_F(CameraRollManagerImplTest,
       OnPhoneStatusSnapshotReceivedWithCameraRollSettingDisabled) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kSuccess);

  proto::PhoneStatusSnapshot snapshot;
  proto::CameraRollAccessState* access_state =
      snapshot.mutable_properties()->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(false);
  access_state->set_storage_permission_granted(true);
  SetCameraRollFeatureSettings(false);
  fake_message_receiver_.NotifyPhoneStatusSnapshotReceived(snapshot);

  EXPECT_EQ(0UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(2, GetOnCameraRollItemChangedCallCount());
  EXPECT_EQ(0, GetCurrentItemsCount());
}

TEST_F(CameraRollManagerImplTest,
       OnPhoneStatusSnapshotReceivedWithoutStoragePermission) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kSuccess);

  proto::PhoneStatusSnapshot snapshot;
  proto::CameraRollAccessState* access_state =
      snapshot.mutable_properties()->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(true);
  access_state->set_storage_permission_granted(false);
  fake_message_receiver_.NotifyPhoneStatusSnapshotReceived(snapshot);

  EXPECT_EQ(0UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(2, GetOnCameraRollItemChangedCallCount());
  EXPECT_EQ(0, GetCurrentItemsCount());
}

TEST_F(CameraRollManagerImplTest, OnFeatureOnFeatureStatesChangedToDisabled) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");
  fake_message_receiver_.NotifyFetchCameraRollItemsResponseReceived(response);
  CompleteThumbnailDecoding(BatchDecodeResult::kSuccess);

  SetCameraRollFeatureSettings(false);

  EXPECT_EQ(0UL, GetSentFetchCameraRollItemsRequestCount());
  EXPECT_EQ(2, GetOnCameraRollItemChangedCallCount());
  EXPECT_EQ(0, GetCurrentItemsCount());
}

}  // namespace phonehub
}  // namespace chromeos
