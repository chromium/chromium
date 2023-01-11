// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/icon_decoder_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "chromeos/ash/components/phonehub/icon_decoder.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace phonehub {

namespace {

// Verifies the bitmaps in the decoded items match the id of the requests.
// As the id % 10 is used as the width of the bitmaps, it checks to see if
// the ids and bitmap sizes match ort not.
void VerifyDecodedItem(IconDecoder::DecodingData& item) {
  EXPECT_EQ((int)(item.id % 10), item.result.Width());
}

void VerifyDecodedItems(std::vector<IconDecoder::DecodingData>* items) {
  for (IconDecoder::DecodingData& item : *items)
    VerifyDecodedItem(item);
}

}  // namespace

class TestDecoderDelegate : public IconDecoderImpl::DecoderDelegate {
 public:
  TestDecoderDelegate() = default;
  ~TestDecoderDelegate() override = default;

  void Decode(const IconDecoder::DecodingData& data,
              data_decoder::DecodeImageCallback callback) override {
    pending_callbacks_[data.id] = std::move(callback);
  }

  void CompleteRequest(const unsigned long id) {
    SkBitmap test_bitmap;
    test_bitmap.allocN32Pixels(id % 10, 1);
    std::move(pending_callbacks_.at(id)).Run(test_bitmap);
    pending_callbacks_.erase(id);
  }

  void FailRequest(const unsigned long id) {
    SkBitmap test_bitmap;
    std::move(pending_callbacks_.at(id)).Run(test_bitmap);
    pending_callbacks_.erase(id);
  }

  void CompleteAllRequests() {
    for (auto& it : pending_callbacks_)
      CompleteRequest(it.first);
    pending_callbacks_.clear();
  }

 private:
  base::flat_map<unsigned long, data_decoder::DecodeImageCallback>
      pending_callbacks_;
};

class IconDecoderImplTest : public testing::Test {
 protected:
  IconDecoderImplTest() = default;
  IconDecoderImplTest(const IconDecoderImplTest&) = delete;
  IconDecoderImplTest& operator=(const IconDecoderImplTest&) = delete;
  ~IconDecoderImplTest() override = default;

  void SetUp() override {
    decoder_.decoder_delegate_ = std::make_unique<TestDecoderDelegate>();
  }

  void BatchDecode(
      std::unique_ptr<std::vector<IconDecoder::DecodingData>> decode_items) {
    decoder_.BatchDecode(std::move(decode_items),
                         base::BindOnce(&IconDecoderImplTest::OnItemsDecoded,
                                        weak_ptr_factory_.GetWeakPtr()));
  }

  void CompleteDecodeRequest(const unsigned long id) {
    static_cast<TestDecoderDelegate*>(decoder_.decoder_delegate_.get())
        ->CompleteRequest(id);
  }

  void FailDecodeRequest(const unsigned long id) {
    static_cast<TestDecoderDelegate*>(decoder_.decoder_delegate_.get())
        ->FailRequest(id);
  }

  void CompleteAllDecodeRequests() {
    static_cast<TestDecoderDelegate*>(decoder_.decoder_delegate_.get())
        ->CompleteAllRequests();
  }

  int completed_batch_count() const { return completed_batch_count_; }
  std::vector<IconDecoder::DecodingData>* last_decoded_batch() const {
    return last_decoded_batch_.get();
  }

  void OnItemsDecoded(
      std::unique_ptr<std::vector<IconDecoder::DecodingData>> decode_items) {
    completed_batch_count_++;
    last_decoded_batch_ = std::move(decode_items);
  }

 private:
  IconDecoderImpl decoder_;
  int completed_batch_count_ = 0;
  std::unique_ptr<std::vector<IconDecoder::DecodingData>> last_decoded_batch_;

  base::WeakPtrFactory<IconDecoderImplTest> weak_ptr_factory_{this};
};

TEST_F(IconDecoderImplTest, BatchDecodeWithNoExistingItems) {
  auto decode_items =
      std::make_unique<std::vector<IconDecoder::DecodingData>>();
  decode_items->push_back(IconDecoder::DecodingData(1, "input1"));
  decode_items->push_back(IconDecoder::DecodingData(2, "input2"));
  decode_items->push_back(IconDecoder::DecodingData(3, "input3"));

  BatchDecode(std::move(decode_items));

  CompleteDecodeRequest(3);
  CompleteDecodeRequest(2);
  EXPECT_EQ(0, completed_batch_count());

  // The batch of items won't be added until decode request are completed for
  // all items.
  CompleteDecodeRequest(1);
  EXPECT_EQ(1, completed_batch_count());
  auto* result = last_decoded_batch();
  EXPECT_EQ(3UL, result->size());
}

TEST_F(IconDecoderImplTest, BatchDecodeWithOutOfOrderCompletions) {
  auto decode_items =
      std::make_unique<std::vector<IconDecoder::DecodingData>>();
  decode_items->push_back(IconDecoder::DecodingData(1, "input1"));
  decode_items->push_back(IconDecoder::DecodingData(2, "input2"));
  decode_items->push_back(IconDecoder::DecodingData(3, "input3"));

  BatchDecode(std::move(decode_items));

  CompleteDecodeRequest(2);
  CompleteDecodeRequest(1);
  CompleteDecodeRequest(3);

  EXPECT_EQ(1, completed_batch_count());
  auto* result = last_decoded_batch();
  EXPECT_EQ(3UL, result->size());
  VerifyDecodedItems(result);
}

TEST_F(IconDecoderImplTest, BatchDecodeWithErrors) {
  auto decode_items =
      std::make_unique<std::vector<IconDecoder::DecodingData>>();
  decode_items->push_back(IconDecoder::DecodingData(1, "input1"));
  decode_items->push_back(IconDecoder::DecodingData(2, "input2"));
  decode_items->push_back(IconDecoder::DecodingData(3, "input3"));

  BatchDecode(std::move(decode_items));

  FailDecodeRequest(2);
  CompleteDecodeRequest(1);
  CompleteDecodeRequest(3);

  EXPECT_EQ(1, completed_batch_count());
  auto* result = last_decoded_batch();
  EXPECT_EQ(3UL, result->size());
  VerifyDecodedItem((*result)[0]);
  VerifyDecodedItem((*result)[2]);
  EXPECT_EQ(0, (*result)[1].result.Width());
}

TEST_F(IconDecoderImplTest, BatchDecodeWithInProgresRequests) {
  auto decode_items1 =
      std::make_unique<std::vector<IconDecoder::DecodingData>>();
  decode_items1->push_back(IconDecoder::DecodingData(2, "input2"));
  decode_items1->push_back(IconDecoder::DecodingData(1, "input1"));

  BatchDecode(std::move(decode_items1));
  CompleteDecodeRequest(1);

  auto decode_items2 =
      std::make_unique<std::vector<IconDecoder::DecodingData>>();
  decode_items2->push_back(IconDecoder::DecodingData(3, "input3"));
  decode_items2->push_back(IconDecoder::DecodingData(2, "input2"));

  BatchDecode(std::move(decode_items2));  // This will cancel the previous call.
  CompleteDecodeRequest(2);
  CompleteDecodeRequest(3);

  EXPECT_EQ(1, completed_batch_count());
  auto* result = last_decoded_batch();
  EXPECT_EQ(2UL, result->size());
  VerifyDecodedItems(result);
}

}  // namespace phonehub
}  // namespace ash
