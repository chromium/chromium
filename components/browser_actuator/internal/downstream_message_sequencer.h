// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_DOWNSTREAM_MESSAGE_SEQUENCER_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_DOWNSTREAM_MESSAGE_SEQUENCER_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"

namespace browser_actuator {

// Orders the downstream messages of a single session by `sequence_number`,
// which starts at 1.
//
// Relative to `last_seen_sequence_number()`, a message numbered:
// - `last_seen + 1` is delivered, plus any buffered messages now contiguous;
// - `> last_seen + 1` is buffered;
// - `<= last_seen` is dropped as a duplicate.
//
// `last_seen_sequence_number()` therefore only advances over contiguous
// messages, which makes it a safe resume position: everything after it can
// be replayed by the server.
//
// Only `Flush()` skips gaps. This class never requests a replay itself: a
// caller must react to `Error::kBufferFull`, and must also handle a gap that
// is never filled, because a lost message followed by too few messages to
// fill the buffer never produces an error.
//
// Not thread-safe; owned and used on a single sequence by its session.
class DownstreamMessageSequencer {
 public:
  enum class Error {
    // `sequence_number` is <= 0 or `std::numeric_limits<int64_t>::max()`.
    kInvalidSequenceNumber,
    // The message is early and the buffer is full. The message is discarded;
    // the caller should request a replay from `last_seen_sequence_number()`.
    kBufferFull,
  };

  // Bounds per-session memory while absorbing transient reordering.
  static constexpr size_t kDefaultMaxBufferedMessages = 16;

  explicit DownstreamMessageSequencer(
      size_t max_buffered_messages = kDefaultMaxBufferedMessages);
  ~DownstreamMessageSequencer();

  DownstreamMessageSequencer(const DownstreamMessageSequencer&) = delete;
  DownstreamMessageSequencer& operator=(const DownstreamMessageSequencer&) =
      delete;

  // Returns the messages that are now deliverable, in order. Returns an empty
  // vector if `message` was buffered or dropped as a duplicate. On error,
  // `message` is discarded.
  base::expected<std::vector<ActuatorDownstreamMessage>, Error> AddMessage(
      ActuatorDownstreamMessage message);

  // Returns all buffered messages in order, skipping any gaps. Empties the
  // buffer and advances the cursor to the last returned sequence number.
  [[nodiscard]] std::vector<ActuatorDownstreamMessage> Flush();

  int64_t last_seen_sequence_number() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return last_seen_sequence_number_;
  }
  size_t buffered_message_count() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return buffered_messages_.size();
  }

 private:
  int64_t next_expected_sequence_number() const
      VALID_CONTEXT_REQUIRED(sequence_checker_) {
    return last_seen_sequence_number_ + 1;
  }

  SEQUENCE_CHECKER(sequence_checker_);

  int64_t last_seen_sequence_number_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  const size_t max_buffered_messages_;

  // Keyed by sequence number; every key is > `next_expected_sequence_number()`.
  base::flat_map<int64_t, ActuatorDownstreamMessage> buffered_messages_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_DOWNSTREAM_MESSAGE_SEQUENCER_H_
