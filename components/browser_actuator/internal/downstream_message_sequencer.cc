// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/downstream_message_sequencer.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"

namespace browser_actuator {

DownstreamMessageSequencer::DownstreamMessageSequencer(
    size_t max_buffered_messages)
    : max_buffered_messages_(max_buffered_messages) {}

DownstreamMessageSequencer::~DownstreamMessageSequencer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::expected<std::vector<ActuatorDownstreamMessage>,
               DownstreamMessageSequencer::Error>
DownstreamMessageSequencer::AddMessage(ActuatorDownstreamMessage message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const int64_t sequence_number = message.sequence_number();
  if (sequence_number <= 0 ||
      sequence_number == std::numeric_limits<int64_t>::max()) {
    return base::unexpected(Error::kInvalidSequenceNumber);
  }
  if (sequence_number <= last_seen_sequence_number_) {
    DVLOG(1) << "Session '" << message.session_id()
             << "': dropping duplicate sequence number " << sequence_number;
    return std::vector<ActuatorDownstreamMessage>();
  }

  if (sequence_number > next_expected_sequence_number()) {
    if (buffered_messages_.contains(sequence_number)) {
      DVLOG(1) << "Session '" << message.session_id()
               << "': dropping duplicate sequence number " << sequence_number;
      return std::vector<ActuatorDownstreamMessage>();
    }
    if (buffered_messages_.size() >= max_buffered_messages_) {
      // TODO(crbug.com/571122418): Record UMA for reorder buffer overflow.
      DVLOG(1) << "Session '" << message.session_id()
               << "': reorder buffer full (" << max_buffered_messages_
               << " messages); cannot buffer sequence number "
               << sequence_number;
      return base::unexpected(Error::kBufferFull);
    }
    buffered_messages_.emplace(sequence_number, std::move(message));
    return std::vector<ActuatorDownstreamMessage>();
  }

  std::vector<ActuatorDownstreamMessage> deliverable;
  deliverable.reserve(1 + buffered_messages_.size());
  deliverable.push_back(std::move(message));
  last_seen_sequence_number_ = sequence_number;

  // Drain buffered messages that are now contiguous. The map is sorted, so
  // they form a prefix of `buffered_messages_`.
  auto it = buffered_messages_.begin();
  while (it != buffered_messages_.end() &&
         it->first == next_expected_sequence_number()) {
    last_seen_sequence_number_ = it->first;
    deliverable.push_back(std::move(it->second));
    ++it;
  }
  buffered_messages_.erase(buffered_messages_.begin(), it);

  CHECK(buffered_messages_.empty() ||
        buffered_messages_.begin()->first > next_expected_sequence_number());
  return deliverable;
}

std::vector<ActuatorDownstreamMessage> DownstreamMessageSequencer::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (buffered_messages_.empty()) {
    return {};
  }

  const int64_t last_flushed = buffered_messages_.rbegin()->first;
  const int64_t skipped_count = (last_flushed - last_seen_sequence_number_) -
                                static_cast<int64_t>(buffered_messages_.size());
  DVLOG(1) << "Session '" << buffered_messages_.begin()->second.session_id()
           << "': flushing " << buffered_messages_.size()
           << " buffered messages through sequence number " << last_flushed
           << " (skipping " << skipped_count << " missing)";

  std::vector<ActuatorDownstreamMessage> flushed;
  flushed.reserve(buffered_messages_.size());
  for (auto& [sequence_number, buffered_message] : buffered_messages_) {
    flushed.push_back(std::move(buffered_message));
  }
  last_seen_sequence_number_ = last_flushed;
  buffered_messages_.clear();
  return flushed;
}

}  // namespace browser_actuator
