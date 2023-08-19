// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_connections_stream_buffer_manager.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/src/connections/payload.h"
#include "third_party/nearby/src/internal/platform/byte_array.h"
#include "third_party/nearby/src/internal/platform/exception.h"
#include "third_party/nearby/src/internal/platform/input_stream.h"

namespace nearby::connections {

namespace {

class FakeStream : public InputStream {
 public:
  FakeStream() = default;
  ~FakeStream() override = default;

  ExceptionOr<ByteArray> Read(std::int64_t size) override {
    if (should_throw_exception) {
      return ExceptionOr<ByteArray>(Exception::kIo);
    }
    return ExceptionOr<ByteArray>(ByteArray(std::string(size, '\0')));
  }

  Exception Close() override {
    if (should_throw_exception) {
      return {.value = Exception::kIo};
    }
    return {.value = Exception::kSuccess};
  }

  bool should_throw_exception = false;
};

}  // namespace

class NearbyConnectionsStreamBufferManagerTest : public testing::Test {
 protected:
  NearbyConnectionsStreamBufferManagerTest() = default;
  ~NearbyConnectionsStreamBufferManagerTest() override = default;

  NearbyConnectionsStreamBufferManager buffer_manager_;
};

TEST_F(NearbyConnectionsStreamBufferManagerTest, Success) {
  Payload payload =
      Payload(/* payload_id= */ 1, std::make_unique<FakeStream>());

  buffer_manager_.StartTrackingPayload(std::move(payload));
  EXPECT_TRUE(buffer_manager_.IsTrackingPayload(/* payload_id= */ 1));

  buffer_manager_.HandleBytesTransferred(
      /* payload_id= */ 1,
      /* cumulative_bytes_transferred_so_far= */ 1980);
  buffer_manager_.HandleBytesTransferred(
      /* payload_id= */ 1,
      /* cumulative_bytes_transferred_so_far= */ 2500);

  ByteArray array =
      buffer_manager_.GetCompletePayloadAndStopTracking(/* payload_id= */ 1);
  EXPECT_FALSE(buffer_manager_.IsTrackingPayload(/* payload_id= */ 1));
  EXPECT_EQ(2500u, array.size());
}

TEST_F(NearbyConnectionsStreamBufferManagerTest, Success_MultipleStreams) {
  Payload payload1 =
      Payload(/* payload_id= */ 1, std::make_unique<FakeStream>());
  Payload payload2 =
      Payload(/* payload_id= */ 2, std::make_unique<FakeStream>());

  buffer_manager_.StartTrackingPayload(std::move(payload1));
  EXPECT_TRUE(buffer_manager_.IsTrackingPayload(/* payload_id= */ 1));

  buffer_manager_.StartTrackingPayload(std::move(payload2));
  EXPECT_TRUE(buffer_manager_.IsTrackingPayload(/* payload_id= */ 2));

  buffer_manager_.HandleBytesTransferred(
      /* payload_id= */ 1,
      /* cumulative_bytes_transferred_so_far= */ 1980);
  buffer_manager_.HandleBytesTransferred(
      /* payload_id= */ 2,
      /* cumulative_bytes_transferred_so_far= */ 1980);
  buffer_manager_.HandleBytesTransferred(
      /* payload_id= */ 1,
      /* cumulative_bytes_transferred_so_far= */ 2500);
  buffer_manager_.HandleBytesTransferred(
      /* payload_id= */ 2,
      /* cumulative_bytes_transferred_so_far= */ 3000);

  ByteArray array1 =
      buffer_manager_.GetCompletePayloadAndStopTracking(/* payload_id= */ 1);
  EXPECT_FALSE(buffer_manager_.IsTrackingPayload(/* payload_id= */ 1));
  EXPECT_EQ(2500u, array1.size());

  ByteArray array2 =
      buffer_manager_.GetCompletePayloadAndStopTracking(/* payload_id= */ 2);
  EXPECT_FALSE(buffer_manager_.IsTrackingPayload(/* payload_id= */ 2));
  EXPECT_EQ(3000u, array2.size());
}

TEST_F(NearbyConnectionsStreamBufferManagerTest, Failure) {
  Payload payload =
      Payload(/* payload_id= */ 1, std::make_unique<FakeStream>());

  buffer_manager_.StartTrackingPayload(std::move(payload));
  EXPECT_TRUE(buffer_manager_.IsTrackingPayload(/* payload_id= */ 1));

  buffer_manager_.HandleBytesTransferred(
      /* payload_id= */ 1,
      /* cumulative_bytes_transferred_so_far= */ 1980);
  buffer_manager_.StopTrackingFailedPayload(/* payload_id= */ 1);
  EXPECT_FALSE(buffer_manager_.IsTrackingPayload(/* payload_id= */ 1));
}

TEST_F(NearbyConnectionsStreamBufferManagerTest, Exception) {
  auto stream = std::make_unique<FakeStream>();
  stream->should_throw_exception = true;
  Payload payload = Payload(/* payload_id= */ 1, std::move(stream));

  buffer_manager_.StartTrackingPayload(std::move(payload));
  EXPECT_TRUE(buffer_manager_.IsTrackingPayload(/* payload_id= */ 1));

  buffer_manager_.HandleBytesTransferred(
      /* payload_id= */ 1,
      /* cumulative_bytes_transferred_so_far= */ 1980);

  EXPECT_FALSE(buffer_manager_.IsTrackingPayload(/* payload_id= */ 1));
}

}  // namespace nearby::connections
