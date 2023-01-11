// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/camera_roll_thumbnail_decoder_impl.h"

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/phonehub/camera_roll_item.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace phonehub {

namespace {

using BatchDecodeResult = CameraRollThumbnailDecoder::BatchDecodeResult;

void PopulateItemProto(proto::CameraRollItem* item_proto,
                       const std::string& key) {
  proto::CameraRollItemMetadata* metadata = item_proto->mutable_metadata();
  metadata->set_key(key);
  metadata->set_mime_type("image/png");
  metadata->set_last_modified_millis(123456789L);
  metadata->set_file_size_bytes(123456789L);

  proto::CameraRollItemThumbnail* thumbnail = item_proto->mutable_thumbnail();
  thumbnail->set_format(proto::CameraRollItemThumbnail_Format_JPEG);
  thumbnail->set_data("encoded_thumbnail_data");
}

const CameraRollItem CreateItem(const std::string& key) {
  proto::CameraRollItemMetadata metadata;
  metadata.set_key(key);
  metadata.set_mime_type("image/png");
  metadata.set_last_modified_millis(123456789L);
  metadata.set_file_size_bytes(123456789L);

  SkBitmap test_bitmap;
  test_bitmap.allocN32Pixels(1, 1);
  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(test_bitmap);
  image_skia.MakeThreadSafe();
  gfx::Image thumbnail(image_skia);

  return CameraRollItem(metadata, thumbnail);
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

class FakeDecoderDelegate
    : public CameraRollThumbnailDecoderImpl::DecoderDelegate {
 public:
  FakeDecoderDelegate() = default;
  ~FakeDecoderDelegate() override = default;

  void DecodeThumbnail(
      const CameraRollThumbnailDecoderImpl::DecodeRequest& request,
      data_decoder::DecodeImageCallback callback) override {
    pending_callbacks_[request.GetMetadata().key()] = std::move(callback);
  }

  void CompleteRequest(const std::string& key) {
    SkBitmap test_bitmap;
    test_bitmap.allocN32Pixels(1, 1);
    std::move(pending_callbacks_.at(key)).Run(test_bitmap);
    pending_callbacks_.erase(key);
  }

  void FailRequest(const std::string& key) {
    SkBitmap test_bitmap;
    std::move(pending_callbacks_.at(key)).Run(test_bitmap);
    pending_callbacks_.erase(key);
  }

  void CompleteAllRequests() {
    SkBitmap test_bitmap;
    test_bitmap.allocN32Pixels(1, 1);
    for (auto& it : pending_callbacks_)
      std::move(it.second).Run(test_bitmap);
    pending_callbacks_.clear();
  }

 private:
  base::flat_map<std::string, data_decoder::DecodeImageCallback>
      pending_callbacks_;
};

class CameraRollThumbnailDecoderImplTest : public testing::Test {
 protected:
  CameraRollThumbnailDecoderImplTest() = default;
  CameraRollThumbnailDecoderImplTest(
      const CameraRollThumbnailDecoderImplTest&) = delete;
  CameraRollThumbnailDecoderImplTest& operator=(
      const CameraRollThumbnailDecoderImplTest&) = delete;
  ~CameraRollThumbnailDecoderImplTest() override = default;

  void SetUp() override {
    decoder_.decoder_delegate_ = std::make_unique<FakeDecoderDelegate>();
  }

  void BatchDecode(const proto::FetchCameraRollItemsResponse& response,
                   const std::vector<CameraRollItem>& current_items) {
    decoder_.BatchDecode(
        response, current_items,
        base::BindOnce(&CameraRollThumbnailDecoderImplTest::OnItemsDecoded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void CompleteDecodeRequest(const std::string& key) {
    static_cast<FakeDecoderDelegate*>(decoder_.decoder_delegate_.get())
        ->CompleteRequest(key);
  }

  void FailDecodeRequest(const std::string& key) {
    static_cast<FakeDecoderDelegate*>(decoder_.decoder_delegate_.get())
        ->FailRequest(key);
  }

  void CompleteAllDecodeRequests() {
    static_cast<FakeDecoderDelegate*>(decoder_.decoder_delegate_.get())
        ->CompleteAllRequests();
  }

  int completed_batch_count() const { return completed_batch_count_; }

  BatchDecodeResult last_decode_result() const { return last_decode_result_; }

  // Verifies decoded items match the list of items in the provided
  // |FetchCameraRollItemsResponse|, and their thumbnails have been properly
  // decoded.
  void VerifyDecodedItemsMatchResponse(
      const proto::FetchCameraRollItemsResponse& response) const {
    int decoded_item_count = last_decoded_items_.size();
    EXPECT_EQ(response.items_size(), decoded_item_count);
    for (int i = 0; i < decoded_item_count; i++) {
      VerifyDecodedItem(/*decoded_item_index=*/i, response.items(i));
    }
  }

  void VerifyDecodedItem(int decoded_item_index,
                         const proto::CameraRollItem& input_item) const {
    const CameraRollItem& decoded_item =
        last_decoded_items_.at(decoded_item_index);
    VerifyMetadataEqual(input_item.metadata(), decoded_item.metadata());
    EXPECT_FALSE(decoded_item.thumbnail().IsEmpty());
  }

  void OnItemsDecoded(BatchDecodeResult result,
                      const std::vector<CameraRollItem>& items) {
    completed_batch_count_++;
    last_decode_result_ = result;
    last_decoded_items_ = items;
  }

 private:
  CameraRollThumbnailDecoderImpl decoder_;
  int completed_batch_count_ = 0;
  BatchDecodeResult last_decode_result_;
  std::vector<CameraRollItem> last_decoded_items_;
  base::WeakPtrFactory<CameraRollThumbnailDecoderImplTest> weak_ptr_factory_{
      this};
};

TEST_F(CameraRollThumbnailDecoderImplTest, BatchDecodeWithNoExistingItems) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key3");
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");

  BatchDecode(response, {});

  CompleteDecodeRequest("key3");
  CompleteDecodeRequest("key2");
  EXPECT_EQ(0, completed_batch_count());

  // The batch of items won't be added until decode request are completed for
  // all items.
  CompleteDecodeRequest("key1");
  EXPECT_EQ(1, completed_batch_count());
  EXPECT_EQ(BatchDecodeResult::kCompleted, last_decode_result());
  VerifyDecodedItemsMatchResponse(response);
}

TEST_F(CameraRollThumbnailDecoderImplTest,
       BatchDecodeWithOutOfOrderCompletions) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key3");
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");

  BatchDecode(response, {});

  CompleteDecodeRequest("key2");
  CompleteDecodeRequest("key1");
  CompleteDecodeRequest("key3");
  EXPECT_EQ(1, completed_batch_count());
  EXPECT_EQ(BatchDecodeResult::kCompleted, last_decode_result());
  // Verify that the order in which the thumbnails are decoded does not affect
  // the final display order of the items.
  VerifyDecodedItemsMatchResponse(response);
}

TEST_F(CameraRollThumbnailDecoderImplTest, BatchDecodeWithErrors) {
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key3");
  PopulateItemProto(response.add_items(), "key2");
  PopulateItemProto(response.add_items(), "key1");

  BatchDecode(response, {});

  CompleteDecodeRequest("key3");
  CompleteDecodeRequest("key1");
  FailDecodeRequest("key2");
  EXPECT_EQ(1, completed_batch_count());
  EXPECT_EQ(BatchDecodeResult::kCompleted, last_decode_result());
  VerifyDecodedItem(/*decoded_item_index=*/0, response.items(0));
  VerifyDecodedItem(/*decoded_item_index=*/1, response.items(2));
}

TEST_F(CameraRollThumbnailDecoderImplTest,
       BatchDecodeWithMissingThumbnailData) {
  std::vector<CameraRollItem> existing_items;
  existing_items.push_back(CreateItem("key2"));
  existing_items.push_back(CreateItem("key1"));
  proto::FetchCameraRollItemsResponse response;
  // This is a new item but its thumbnail data is missing.
  PopulateItemProto(response.add_items(), "key3");
  response.mutable_items(0)->clear_thumbnail();
  PopulateItemProto(response.add_items(), "key2");

  BatchDecode(response, existing_items);
  CompleteAllDecodeRequests();

  EXPECT_EQ(1, completed_batch_count());
  EXPECT_EQ(BatchDecodeResult::kCancelled, last_decode_result());
}

TEST_F(CameraRollThumbnailDecoderImplTest, BatchDecodeWithExistingItems) {
  std::vector<CameraRollItem> existing_items;
  existing_items.push_back(CreateItem("key3"));
  existing_items.push_back(CreateItem("key2"));
  existing_items.push_back(CreateItem("key1"));
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key4");
  // Thumbnails won't be sent with the proto if an item's data is already
  // available and up-to-date.
  PopulateItemProto(response.add_items(), "key3");
  response.mutable_items(1)->clear_thumbnail();
  PopulateItemProto(response.add_items(), "key2");
  response.mutable_items(2)->clear_thumbnail();

  BatchDecode(response, existing_items);
  EXPECT_EQ(0, completed_batch_count());

  // Only need to decode thumbnail of this new item.
  CompleteDecodeRequest("key4");
  EXPECT_EQ(1, completed_batch_count());
  EXPECT_EQ(BatchDecodeResult::kCompleted, last_decode_result());
  VerifyDecodedItemsMatchResponse(response);
}

TEST_F(CameraRollThumbnailDecoderImplTest,
       BatchDecodeWithExistingItemsInDifferentOrder) {
  std::vector<CameraRollItem> existing_items;
  existing_items.push_back(CreateItem("key3"));
  existing_items.push_back(CreateItem("key2"));
  existing_items.push_back(CreateItem("key1"));
  proto::FetchCameraRollItemsResponse response;
  PopulateItemProto(response.add_items(), "key2");
  response.mutable_items(0)->clear_thumbnail();
  PopulateItemProto(response.add_items(), "key3");
  response.mutable_items(1)->clear_thumbnail();
  PopulateItemProto(response.add_items(), "key1");
  response.mutable_items(2)->clear_thumbnail();

  BatchDecode(response, existing_items);

  // No additional thumbnail decoding is needed because the items are already
  // available.
  EXPECT_EQ(1, completed_batch_count());
  EXPECT_EQ(BatchDecodeResult::kCompleted, last_decode_result());
  VerifyDecodedItemsMatchResponse(response);
}

TEST_F(CameraRollThumbnailDecoderImplTest, BatchDecodeWithInProgresRequests) {
  proto::FetchCameraRollItemsResponse first_response;
  PopulateItemProto(first_response.add_items(), "key2");
  PopulateItemProto(first_response.add_items(), "key1");

  BatchDecode(first_response, {});
  CompleteDecodeRequest("key1");

  proto::FetchCameraRollItemsResponse second_response;
  PopulateItemProto(second_response.add_items(), "key3");
  PopulateItemProto(second_response.add_items(), "key2");
  BatchDecode(second_response, {});
  // The first in-progress request should be cancelled
  EXPECT_EQ(1, completed_batch_count());
  EXPECT_EQ(BatchDecodeResult::kCancelled, last_decode_result());

  CompleteAllDecodeRequests();
  EXPECT_EQ(2, completed_batch_count());
  EXPECT_EQ(BatchDecodeResult::kCompleted, last_decode_result());
  VerifyDecodedItemsMatchResponse(second_response);
}

}  // namespace phonehub
}  // namespace ash
