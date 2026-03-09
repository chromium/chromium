// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/control/remoting/remoting_decoder_buffer_factory.h"

#include "base/memory/scoped_refptr.h"
#include "components/cast_streaming/browser/common/decoder_buffer_factory.h"
#include "media/base/decoder_buffer.h"
#include "media/cast/openscreen/remoting_proto_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/public/encoded_frame.h"

namespace cast_streaming {
namespace {

class RemotingDecoderBufferFactoryTest : public testing::Test {
 public:
  RemotingDecoderBufferFactoryTest() = default;
  ~RemotingDecoderBufferFactoryTest() override = default;

 protected:
  RemotingDecoderBufferFactory factory_;
};

TEST_F(RemotingDecoderBufferFactoryTest, DeserializationFailed) {
  // Invalid data to force ByteArrayToDecoderBuffer failure
  uint8_t invalid_data[] = {0xFF, 0x00, 0x11};

  openscreen::cast::EncodedFrame encoded_frame;
  auto result = factory_.ToDecoderBuffer(
      encoded_frame, base::span<const uint8_t>(invalid_data));

  EXPECT_FALSE(result);
}

TEST_F(RemotingDecoderBufferFactoryTest, Success) {
  // Create a valid protobuf message for a DecoderBuffer
  scoped_refptr<media::DecoderBuffer> decoder_buffer =
      base::MakeRefCounted<media::DecoderBuffer>(10);
  auto valid_data = media::cast::DecoderBufferToByteArray(*decoder_buffer);

  openscreen::cast::EncodedFrame encoded_frame;
  auto result = factory_.ToDecoderBuffer(encoded_frame,
                                         base::span<const uint8_t>(valid_data));

  EXPECT_TRUE(result);
  EXPECT_EQ(result->size(), 10u);
}

}  // namespace
}  // namespace cast_streaming
