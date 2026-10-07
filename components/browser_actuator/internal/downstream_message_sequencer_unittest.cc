// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/downstream_message_sequencer.h"

#include <cstdint>
#include <limits>
#include <vector>

#include "base/test/gmock_expected_support.h"
#include "base/test/protobuf_matchers.h"
#include "base/types/expected.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

using Error = DownstreamMessageSequencer::Error;
using ::base::test::EqualsProto;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr char kSessionId[] = "session";

ActuatorDownstreamMessage MakeMessage(int64_t sequence_number) {
  ActuatorDownstreamMessage message;
  message.set_session_id(kSessionId);
  message.set_sequence_number(sequence_number);
  return message;
}

std::vector<int64_t> SequenceNumbers(
    const std::vector<ActuatorDownstreamMessage>& messages) {
  std::vector<int64_t> sequence_numbers;
  for (const ActuatorDownstreamMessage& message : messages) {
    sequence_numbers.push_back(message.sequence_number());
  }
  return sequence_numbers;
}

// Adds `sequence_number` via `AddMessage()` and transforms the delivered
// messages into their sequence numbers.
base::expected<std::vector<int64_t>, Error> AddAndGetDelivered(
    DownstreamMessageSequencer& sequencer,
    int64_t sequence_number) {
  return sequencer.AddMessage(MakeMessage(sequence_number))
      .transform(&SequenceNumbers);
}

TEST(DownstreamMessageSequencerTest, StartsEmpty) {
  DownstreamMessageSequencer sequencer;
  EXPECT_EQ(sequencer.last_seen_sequence_number(), 0);
  EXPECT_EQ(sequencer.buffered_message_count(), 0u);
}

TEST(DownstreamMessageSequencerTest, DeliversInOrderMessages) {
  DownstreamMessageSequencer sequencer;
  EXPECT_THAT(AddAndGetDelivered(sequencer, 1), ValueIs(ElementsAre(1)));
  EXPECT_THAT(AddAndGetDelivered(sequencer, 2), ValueIs(ElementsAre(2)));
  EXPECT_THAT(AddAndGetDelivered(sequencer, 3), ValueIs(ElementsAre(3)));
  EXPECT_EQ(sequencer.last_seen_sequence_number(), 3);
  EXPECT_EQ(sequencer.buffered_message_count(), 0u);
}

TEST(DownstreamMessageSequencerTest, PreservesMessageContents) {
  DownstreamMessageSequencer sequencer;
  ActuatorDownstreamMessage message = MakeMessage(1);
  message.add_typed_payloads()->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);

  EXPECT_THAT(sequencer.AddMessage(message),
              ValueIs(ElementsAre(EqualsProto(message))));
}

TEST(DownstreamMessageSequencerTest, BuffersEarlyMessageUntilGapFills) {
  DownstreamMessageSequencer sequencer;
  EXPECT_THAT(AddAndGetDelivered(sequencer, 1), ValueIs(ElementsAre(1)));

  // 3 is early: held, and the cursor does not move past the gap.
  EXPECT_THAT(AddAndGetDelivered(sequencer, 3), ValueIs(IsEmpty()));
  EXPECT_EQ(sequencer.last_seen_sequence_number(), 1);
  EXPECT_EQ(sequencer.buffered_message_count(), 1u);

  // 2 fills the gap and releases 3.
  EXPECT_THAT(AddAndGetDelivered(sequencer, 2), ValueIs(ElementsAre(2, 3)));
  EXPECT_EQ(sequencer.last_seen_sequence_number(), 3);
  EXPECT_EQ(sequencer.buffered_message_count(), 0u);
}

TEST(DownstreamMessageSequencerTest, BuffersWhenFirstMessageIsMissing) {
  DownstreamMessageSequencer sequencer;
  EXPECT_THAT(AddAndGetDelivered(sequencer, 2), ValueIs(IsEmpty()));
  EXPECT_EQ(sequencer.last_seen_sequence_number(), 0);
  EXPECT_EQ(sequencer.buffered_message_count(), 1u);

  EXPECT_THAT(AddAndGetDelivered(sequencer, 1), ValueIs(ElementsAre(1, 2)));
}

TEST(DownstreamMessageSequencerTest, FillsMultipleGapsInOrder) {
  DownstreamMessageSequencer sequencer;
  EXPECT_THAT(AddAndGetDelivered(sequencer, 5), ValueIs(IsEmpty()));
  EXPECT_THAT(AddAndGetDelivered(sequencer, 3), ValueIs(IsEmpty()));
  EXPECT_THAT(AddAndGetDelivered(sequencer, 2), ValueIs(IsEmpty()));
  EXPECT_EQ(sequencer.buffered_message_count(), 3u);

  // 1 releases 2 and 3, but 5 waits behind the missing 4.
  EXPECT_THAT(AddAndGetDelivered(sequencer, 1), ValueIs(ElementsAre(1, 2, 3)));
  EXPECT_EQ(sequencer.last_seen_sequence_number(), 3);
  EXPECT_EQ(sequencer.buffered_message_count(), 1u);

  EXPECT_THAT(AddAndGetDelivered(sequencer, 4), ValueIs(ElementsAre(4, 5)));
  EXPECT_EQ(sequencer.last_seen_sequence_number(), 5);
  EXPECT_EQ(sequencer.buffered_message_count(), 0u);
}

// A lost message followed by too few messages to fill the buffer is never
// reported as an error; callers must detect the stall via buffered count.
TEST(DownstreamMessageSequencerTest, TrailingGapHoldsMessagesWithoutError) {
  DownstreamMessageSequencer sequencer;
  for (int64_t i = 1; i <= 3; ++i) {
    EXPECT_THAT(AddAndGetDelivered(sequencer, i), ValueIs(ElementsAre(i)))
        << "i=" << i;
  }
  // 4 is lost; 5 and 6 are the last messages.
  EXPECT_THAT(AddAndGetDelivered(sequencer, 5), ValueIs(IsEmpty()));
  EXPECT_THAT(AddAndGetDelivered(sequencer, 6), ValueIs(IsEmpty()));

  EXPECT_EQ(sequencer.last_seen_sequence_number(), 3);
  EXPECT_EQ(sequencer.buffered_message_count(), 2u);
}

TEST(DownstreamMessageSequencerTest, DropsDuplicateAtOrBelowCursor) {
  DownstreamMessageSequencer sequencer;
  EXPECT_THAT(AddAndGetDelivered(sequencer, 1), ValueIs(ElementsAre(1)));
  EXPECT_THAT(AddAndGetDelivered(sequencer, 2), ValueIs(ElementsAre(2)));

  EXPECT_THAT(AddAndGetDelivered(sequencer, 2), ValueIs(IsEmpty()));
  EXPECT_THAT(AddAndGetDelivered(sequencer, 1), ValueIs(IsEmpty()));
  EXPECT_EQ(sequencer.last_seen_sequence_number(), 2);
  EXPECT_EQ(sequencer.buffered_message_count(), 0u);
}

TEST(DownstreamMessageSequencerTest, DropsDuplicateBufferedMessage) {
  DownstreamMessageSequencer sequencer;
  ActuatorDownstreamMessage first = MakeMessage(2);
  first.add_typed_payloads()->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  ASSERT_THAT(sequencer.AddMessage(first), ValueIs(IsEmpty()));

  // A retransmission of 2 does not replace the original.
  EXPECT_THAT(AddAndGetDelivered(sequencer, 2), ValueIs(IsEmpty()));
  EXPECT_EQ(sequencer.buffered_message_count(), 1u);

  EXPECT_THAT(
      sequencer.AddMessage(MakeMessage(1)),
      ValueIs(ElementsAre(EqualsProto(MakeMessage(1)), EqualsProto(first))));
}

TEST(DownstreamMessageSequencerTest, RejectsInvalidSequenceNumber) {
  DownstreamMessageSequencer sequencer;
  EXPECT_THAT(sequencer.AddMessage(MakeMessage(0)),
              ErrorIs(Error::kInvalidSequenceNumber));
  EXPECT_THAT(sequencer.AddMessage(MakeMessage(-1)),
              ErrorIs(Error::kInvalidSequenceNumber));
  EXPECT_THAT(
      sequencer.AddMessage(MakeMessage(std::numeric_limits<int64_t>::min())),
      ErrorIs(Error::kInvalidSequenceNumber));
  EXPECT_THAT(
      sequencer.AddMessage(MakeMessage(std::numeric_limits<int64_t>::max())),
      ErrorIs(Error::kInvalidSequenceNumber));
  EXPECT_EQ(sequencer.last_seen_sequence_number(), 0);
  EXPECT_EQ(sequencer.buffered_message_count(), 0u);

  // Non-positive sequence numbers are still rejected as invalid rather than
  // dropped as duplicates after the cursor advances.
  EXPECT_THAT(AddAndGetDelivered(sequencer, 1), ValueIs(ElementsAre(1)));
  EXPECT_THAT(sequencer.AddMessage(MakeMessage(0)),
              ErrorIs(Error::kInvalidSequenceNumber));
  EXPECT_THAT(sequencer.AddMessage(MakeMessage(-1)),
              ErrorIs(Error::kInvalidSequenceNumber));
  EXPECT_EQ(sequencer.last_seen_sequence_number(), 1);
}

TEST(DownstreamMessageSequencerTest, RejectsEarlyMessageWhenBufferFull) {
  DownstreamMessageSequencer sequencer(/*max_buffered_messages=*/2);
  EXPECT_THAT(AddAndGetDelivered(sequencer, 2), ValueIs(IsEmpty()));
  EXPECT_THAT(AddAndGetDelivered(sequencer, 3), ValueIs(IsEmpty()));

  // The incoming message is discarded; the buffer keeps what it had.
  EXPECT_THAT(sequencer.AddMessage(MakeMessage(4)),
              ErrorIs(Error::kBufferFull));
  EXPECT_EQ(sequencer.buffered_message_count(), 2u);
  EXPECT_EQ(sequencer.last_seen_sequence_number(), 0);

  // The next expected message is still accepted while the buffer is full,
  // and drains it. The rejected 4 must be replayed.
  EXPECT_THAT(AddAndGetDelivered(sequencer, 1), ValueIs(ElementsAre(1, 2, 3)));
  EXPECT_THAT(AddAndGetDelivered(sequencer, 4), ValueIs(ElementsAre(4)));
}

TEST(DownstreamMessageSequencerTest, DropsDuplicateOfBufferedMessageWhenFull) {
  DownstreamMessageSequencer sequencer(/*max_buffered_messages=*/1);
  EXPECT_THAT(AddAndGetDelivered(sequencer, 2), ValueIs(IsEmpty()));
  // A duplicate of a buffered message is dropped, not reported as overflow.
  EXPECT_THAT(AddAndGetDelivered(sequencer, 2), ValueIs(IsEmpty()));
  EXPECT_EQ(sequencer.buffered_message_count(), 1u);
}

TEST(DownstreamMessageSequencerTest, ZeroCapacityRejectsEveryEarlyMessage) {
  DownstreamMessageSequencer sequencer(/*max_buffered_messages=*/0);
  EXPECT_THAT(sequencer.AddMessage(MakeMessage(2)),
              ErrorIs(Error::kBufferFull));
  EXPECT_THAT(AddAndGetDelivered(sequencer, 1), ValueIs(ElementsAre(1)));
  EXPECT_THAT(AddAndGetDelivered(sequencer, 2), ValueIs(ElementsAre(2)));
}

TEST(DownstreamMessageSequencerTest, FlushWithEmptyBuffer) {
  DownstreamMessageSequencer sequencer;
  EXPECT_THAT(sequencer.Flush(), IsEmpty());
  EXPECT_EQ(sequencer.last_seen_sequence_number(), 0);

  EXPECT_THAT(AddAndGetDelivered(sequencer, 1), ValueIs(ElementsAre(1)));
  EXPECT_THAT(sequencer.Flush(), IsEmpty());
  EXPECT_EQ(sequencer.last_seen_sequence_number(), 1);
}

TEST(DownstreamMessageSequencerTest, FlushSkipsGapsAndAdvancesCursor) {
  DownstreamMessageSequencer sequencer;
  EXPECT_THAT(AddAndGetDelivered(sequencer, 1), ValueIs(ElementsAre(1)));

  ActuatorDownstreamMessage message_6 = MakeMessage(6);
  message_6.add_typed_payloads()->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  ASSERT_THAT(sequencer.AddMessage(message_6), ValueIs(IsEmpty()));
  EXPECT_THAT(AddAndGetDelivered(sequencer, 3), ValueIs(IsEmpty()));
  EXPECT_THAT(AddAndGetDelivered(sequencer, 4), ValueIs(IsEmpty()));

  EXPECT_THAT(sequencer.Flush(),
              ElementsAre(EqualsProto(MakeMessage(3)),
                          EqualsProto(MakeMessage(4)), EqualsProto(message_6)));
  EXPECT_EQ(sequencer.last_seen_sequence_number(), 6);
  EXPECT_EQ(sequencer.buffered_message_count(), 0u);
}

}  // namespace
}  // namespace browser_actuator
