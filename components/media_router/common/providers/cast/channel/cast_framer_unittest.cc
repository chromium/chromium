// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/cast_framer.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

namespace cast_channel {

using ::cast_channel::ChannelError;

class CastFramerTest : public testing::Test {
 public:
  CastFramerTest() = default;
  ~CastFramerTest() override = default;

  void SetUp() override {
    cast_message_.set_protocol_version(CastMessage::CASTV2_1_0);
    cast_message_.set_source_id("source");
    cast_message_.set_destination_id("destination");
    cast_message_.set_namespace_("namespace");
    cast_message_.set_payload_type(CastMessage::STRING);
    cast_message_.set_payload_utf8("payload");
    ASSERT_TRUE(MessageFramer::Serialize(cast_message_, &cast_message_str_));

    buffer_ = base::MakeRefCounted<net::GrowableIOBuffer>();
    buffer_->SetCapacity(MessageFramer::MessageHeader::max_message_size());
    framer_ = std::make_unique<MessageFramer>(buffer_.get());
  }

  void WriteToBuffer(const std::string& data) {
    buffer_->everything().copy_prefix_from(base::as_byte_span(data));
  }

 protected:
  CastMessage cast_message_;
  std::string cast_message_str_;
  scoped_refptr<net::GrowableIOBuffer> buffer_;
  std::unique_ptr<MessageFramer> framer_;
};

TEST_F(CastFramerTest, TestMessageFramerCompleteMessage) {
  ChannelError error;
  size_t message_length;

  WriteToBuffer(cast_message_str_);

  // Receive 1 byte of the header, framer demands 3 more bytes.
  EXPECT_EQ(4u, framer_->BytesRequested());
  EXPECT_EQ(nullptr, framer_->Ingest(1, &message_length, &error).get());
  EXPECT_EQ(ChannelError::NONE, error);
  EXPECT_EQ(3u, framer_->BytesRequested());

  // Ingest remaining 3, expect that the framer has moved on to requesting the
  // body contents.
  EXPECT_EQ(nullptr, framer_->Ingest(3, &message_length, &error).get());
  EXPECT_EQ(ChannelError::NONE, error);
  EXPECT_EQ(cast_message_str_.size() - sizeof(MessageFramer::MessageHeader),
            framer_->BytesRequested());

  // Remainder of packet sent over the wire.
  std::unique_ptr<CastMessage> message;
  message = framer_->Ingest(framer_->BytesRequested(), &message_length, &error);
  EXPECT_NE(static_cast<CastMessage*>(nullptr), message.get());
  EXPECT_EQ(ChannelError::NONE, error);
  EXPECT_EQ(message->SerializeAsString(), cast_message_.SerializeAsString());
  EXPECT_EQ(4u, framer_->BytesRequested());
  EXPECT_EQ(message->SerializeAsString().size(), message_length);
}

TEST_F(CastFramerTest, TestSerializeErrorMessageTooLarge) {
  std::string serialized;
  CastMessage big_message;
  big_message.CopyFrom(cast_message_);
  std::string payload;
  payload.append(MessageFramer::MessageHeader::max_body_size() + 1, 'x');
  big_message.set_payload_utf8(payload);
  EXPECT_FALSE(MessageFramer::Serialize(big_message, &serialized));
}

TEST_F(CastFramerTest, TestIngestIllegalLargeMessage) {
  std::string mangled_cast_message = cast_message_str_;
  mangled_cast_message[0] = 88;
  mangled_cast_message[1] = 88;
  mangled_cast_message[2] = 88;
  mangled_cast_message[3] = 88;
  WriteToBuffer(mangled_cast_message);

  size_t bytes_ingested;
  ChannelError error;
  EXPECT_EQ(4u, framer_->BytesRequested());
  EXPECT_EQ(nullptr, framer_->Ingest(4, &bytes_ingested, &error).get());
  EXPECT_EQ(ChannelError::INVALID_MESSAGE, error);
  EXPECT_EQ(0u, framer_->BytesRequested());

  // Test that the parser enters a terminal error state.
  WriteToBuffer(cast_message_str_);
  EXPECT_EQ(0u, framer_->BytesRequested());
  EXPECT_EQ(nullptr, framer_->Ingest(4, &bytes_ingested, &error).get());
  EXPECT_EQ(ChannelError::INVALID_MESSAGE, error);
  EXPECT_EQ(0u, framer_->BytesRequested());
}

TEST_F(CastFramerTest, TestIngestIllegalLargeMessage2) {
  std::string mangled_cast_message = cast_message_str_;
  // Header indicates body size is 0x00010001 = 65537
  mangled_cast_message[0] = 0;
  mangled_cast_message[1] = 0x1;
  mangled_cast_message[2] = 0;
  mangled_cast_message[3] = 0x1;
  WriteToBuffer(mangled_cast_message);

  size_t bytes_ingested;
  ChannelError error;
  EXPECT_EQ(4u, framer_->BytesRequested());
  EXPECT_EQ(nullptr, framer_->Ingest(4, &bytes_ingested, &error).get());
  EXPECT_EQ(ChannelError::INVALID_MESSAGE, error);
  EXPECT_EQ(0u, framer_->BytesRequested());

  // Test that the parser enters a terminal error state.
  WriteToBuffer(cast_message_str_);
  EXPECT_EQ(0u, framer_->BytesRequested());
  EXPECT_EQ(nullptr, framer_->Ingest(4, &bytes_ingested, &error).get());
  EXPECT_EQ(ChannelError::INVALID_MESSAGE, error);
  EXPECT_EQ(0u, framer_->BytesRequested());
}

TEST_F(CastFramerTest, TestUnparsableBodyProto) {
  // Message header is OK, but the body is replaced with "x"en.
  std::string mangled_cast_message = cast_message_str_;
  for (size_t i = sizeof(MessageFramer::MessageHeader);
       i < mangled_cast_message.size(); ++i) {
    std::fill(
        mangled_cast_message.begin() + sizeof(MessageFramer::MessageHeader),
        mangled_cast_message.end(), 'x');
  }
  WriteToBuffer(mangled_cast_message);

  // Send header.
  size_t message_length;
  ChannelError error;
  EXPECT_EQ(4u, framer_->BytesRequested());
  EXPECT_EQ(nullptr, framer_->Ingest(4, &message_length, &error).get());
  EXPECT_EQ(ChannelError::NONE, error);
  EXPECT_EQ(cast_message_str_.size() - 4, framer_->BytesRequested());

  // Send body, expect an error.
  std::unique_ptr<CastMessage> message;
  EXPECT_EQ(nullptr,
            framer_->Ingest(framer_->BytesRequested(), &message_length, &error)
                .get());
  EXPECT_EQ(ChannelError::INVALID_MESSAGE, error);
}
}  // namespace cast_channel
