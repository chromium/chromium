// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport_session_impl.h"

#include "base/check.h"
#include "components/browser_actuator/public/transport_channel.h"

namespace browser_actuator {

TransportSessionImpl::TransportSessionImpl(
    std::string_view session_id,
    base::WeakPtr<TransportChannel> channel)
    : session_id_(session_id), channel_(std::move(channel)) {
  DCHECK(channel_);
}

TransportSessionImpl::~TransportSessionImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::string_view TransportSessionImpl::GetSessionId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return session_id_;
}

base::expected<void, SendMessageError> TransportSessionImpl::SendMessage(
    PayloadType payload_type,
    std::string_view payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (channel_) {
    channel_->SendUpstreamMessage(session_id_, payload_type, payload);
    return {};
  }
  return base::unexpected(SendMessageError::kChannelDisconnected);
}

int64_t TransportSessionImpl::last_seen_sequence_number() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return last_seen_sequence_number_;
}

bool TransportSessionImpl::has_last_seen_sequence_number() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return last_seen_sequence_number_ > 0;
}

void TransportSessionImpl::RecordServerSequenceNumber(int64_t seq) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Sequence numbers begin at 1 and can only be strictly increasing.
  if (seq > 0 && seq > last_seen_sequence_number_) {
    last_seen_sequence_number_ = seq;
  }
}

int64_t TransportSessionImpl::client_sequence_number() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return client_sequence_number_;
}

int64_t TransportSessionImpl::IncrementClientSequenceNumber() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ++client_sequence_number_;
}

base::WeakPtr<TransportSessionImpl> TransportSessionImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace browser_actuator
