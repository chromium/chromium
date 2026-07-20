// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport_session_impl.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/browser_actuator/test_support/mock_transport_channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

TEST(TransportSessionImplTest, GetSessionId) {
  MockTransportChannel channel;
  TransportSessionImpl session("test_session", channel.GetWeakPtr());
  EXPECT_EQ(session.GetSessionId(), "test_session");
}

TEST(TransportSessionImplTest, SendMessage) {
  MockTransportChannel channel;
  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  EXPECT_CALL(channel,
              SendUpstreamMessage("test_session", PayloadType::kUnspecified,
                                  "test_payload"));
  EXPECT_TRUE(session.SendMessage(PayloadType::kUnspecified, "test_payload")
                  .has_value());
}

TEST(TransportSessionImplTest, SendMessageAfterChannelDestruction) {
  auto channel = std::make_unique<MockTransportChannel>();
  TransportSessionImpl session("test_session", channel->GetWeakPtr());

  EXPECT_CALL(*channel, SendUpstreamMessage(testing::_, testing::_, testing::_))
      .Times(0);
  channel.reset();
  base::expected<void, SendMessageError> result =
      session.SendMessage(PayloadType::kUnspecified, "test_payload");
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), SendMessageError::kChannelDisconnected);
}

TEST(TransportSessionImplTest, ClientSequenceNumbers) {
  MockTransportChannel channel;
  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  EXPECT_EQ(session.client_sequence_number(), 0);

  EXPECT_EQ(session.IncrementClientSequenceNumber(), 1);
  EXPECT_EQ(session.client_sequence_number(), 1);
}

TEST(TransportSessionImplTest, ServerSequenceNumbers) {
  MockTransportChannel channel;
  TransportSessionImpl session("test_session", channel.GetWeakPtr());

  EXPECT_EQ(session.last_seen_sequence_number(), 0);

  session.RecordServerSequenceNumber(42);
  EXPECT_EQ(session.last_seen_sequence_number(), 42);

  // Past sequence numbers should be ignored.
  session.RecordServerSequenceNumber(30);
  EXPECT_EQ(session.last_seen_sequence_number(), 42);

  // Non-positive sequence numbers should be ignored.
  session.RecordServerSequenceNumber(0);
  session.RecordServerSequenceNumber(-5);
  EXPECT_EQ(session.last_seen_sequence_number(), 42);
}

TEST(TransportSessionImplTest, GetWeakPtr) {
  MockTransportChannel channel;
  auto session = std::make_unique<TransportSessionImpl>("test_session",
                                                        channel.GetWeakPtr());
  base::WeakPtr<TransportSessionImpl> weak_session = session->GetWeakPtr();

  EXPECT_NE(weak_session.get(), nullptr);
  session.reset();
  EXPECT_EQ(weak_session.get(), nullptr);
}

}  // namespace
}  // namespace browser_actuator
