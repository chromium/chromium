// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_connections_stream_buffer_manager.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/src/connections/payload.h"
#include "third_party/nearby/src/internal/platform/byte_array.h"
#include "third_party/nearby/src/internal/platform/exception.h"
#include "third_party/nearby/src/internal/platform/input_stream.h"

namespace nearby {
namespace connections {
namespace {

class FakeStream : public InputStream {
 public:
  FakeStream() = default;
  ~FakeStream() override = default;

  ExceptionOr<ByteArray> Read(std::int64_t size) override {
    if (should_throw_exception)
      return ExceptionOr<ByteArray>(Exception::kIo);
    return ExceptionOr<ByteArray>(ByteArray(std::string(size, '\0')));
  }

  Exception Close() override {
    if (should_throw_exception)
      return {.value = Exception::kIo};
    return {.value = Exception::kSuccess};
  }

  bool should_throw_exception = false;
};

}  // namespace

class NearbyConnectionsStreamBufferManagerTest : public testing::Test {
 protected:
  NearbyConnectionsStreamBufferManagerTest() = default;
  ~NearbyConnectionsStreamBufferManagerTest() override = default;

  Payload CreatePayload(int64_t payload_id, FakeStream** fake_stream) {
    auto stream = std::make_unique<FakeStream>();
    FakeStream* stream_ptr = stream.get();

    *fake_stream = stream_ptr;

    Payload payload(payload_id,
                    [stream_ptr]() -> InputStream& { return *stream_ptr; });

    fake_streams_.emplace_back(std::move(stream));
    return payload;
  }

  NearbyConnectionsStreamBufferManager buffer_manager_;

 private:
  std::vector<std::unique_ptr<FakeStream>> fake_streams_;
};

TEST_F(NearbyConnectionsStreamBufferManagerTest, Success) {
  FakeStream* stream;
  Payload payload = CreatePayload(/*payload_id=*/1, &stream);

  buffer_manager_.StartTrackingPayload(std::move(payload));
  EXPECT_TRUE(buffer_manager_.IsTrackingPayload(/*payload_id=*/1));

  buffer_manager_.HandleBytesTransferred(
      /*payload_id=*/1,
      /*cumulative_bytes_transferred_so_far=*/1980);
  buffer_manager_.HandleBytesTransferred(
      /*payload_id=*/1,
      /*cumulative_bytes_transferred_so_far=*/2500);

  ByteArray array =
      buffer_manager_.GetCompletePayloadAndStopTracking(/*payload_id=*/1);
  EXPECT_FALSE(buffer_manager_.IsTrackingPayload(/*payload_id=*/1));
  EXPECT_EQ(2500u, array.size());
}

TEST_F(NearbyConnectionsStreamBufferManagerTest, Success_MultipleStreams) {
  FakeStream* stream1;
  Payload payload1 = CreatePayload(/*payload_id=*/1, &stream1);

  FakeStream* stream2;
  Payload payload2 = CreatePayload(/*payload_id=*/2, &stream2);

  buffer_manager_.StartTrackingPayload(std::move(payload1));
  EXPECT_TRUE(buffer_manager_.IsTrackingPayload(/*payload_id=*/1));

  buffer_manager_.StartTrackingPayload(std::move(payload2));
  EXPECT_TRUE(buffer_manager_.IsTrackingPayload(/*payload_id=*/2));

  buffer_manager_.HandleBytesTransferred(
      /*payload_id=*/1,
      /*cumulative_bytes_transferred_so_far=*/1980);
  buffer_manager_.HandleBytesTransferred(
      /*payload_id=*/2,
      /*cumulative_bytes_transferred_so_far=*/1980);
  buffer_manager_.HandleBytesTransferred(
      /*payload_id=*/1,
      /*cumulative_bytes_transferred_so_far=*/2500);
  buffer_manager_.HandleBytesTransferred(
      /*payload_id=*/2,
      /*cumulative_bytes_transferred_so_far=*/3000);

  ByteArray array1 =
      buffer_manager_.GetCompletePayloadAndStopTracking(/*payload_id=*/1);
  EXPECT_FALSE(buffer_manager_.IsTrackingPayload(/*payload_id=*/1));
  EXPECT_EQ(2500u, array1.size());

  ByteArray array2 =
      buffer_manager_.GetCompletePayloadAndStopTracking(/*payload_id=*/2);
  EXPECT_FALSE(buffer_manager_.IsTrackingPayload(/*payload_id=*/2));
  EXPECT_EQ(3000u, array2.size());
}

TEST_F(NearbyConnectionsStreamBufferManagerTest, Failure) {
  FakeStream* stream;
  Payload payload = CreatePayload(/*payload_id=*/1, &stream);

  buffer_manager_.StartTrackingPayload(std::move(payload));
  EXPECT_TRUE(buffer_manager_.IsTrackingPayload(/*payload_id=*/1));

  buffer_manager_.HandleBytesTransferred(
      /*payload_id=*/1,
      /*cumulative_bytes_transferred_so_far=*/1980);
  buffer_manager_.StopTrackingFailedPayload(/*payload_id=*/1);
  EXPECT_FALSE(buffer_manager_.IsTrackingPayload(/*payload_id=*/1));
}

TEST_F(NearbyConnectionsStreamBufferManagerTest, Exception) {
  FakeStream* stream;
  Payload payload = CreatePayload(/*payload_id=*/1, &stream);

  buffer_manager_.StartTrackingPayload(std::move(payload));
  EXPECT_TRUE(buffer_manager_.IsTrackingPayload(/*payload_id=*/1));

  stream->should_throw_exception = true;
  buffer_manager_.HandleBytesTransferred(
      /*payload_id=*/1,
      /*cumulative_bytes_transferred_so_far=*/1980);

  EXPECT_FALSE(buffer_manager_.IsTrackingPayload(/*payload_id=*/1));
}

}  // namespace connections
}  // namespace nearby
