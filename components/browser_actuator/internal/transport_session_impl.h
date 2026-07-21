// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_SESSION_IMPL_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_SESSION_IMPL_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/browser_actuator/public/transport_session.h"

namespace browser_actuator {

class TransportChannel;

// Implements TransportSession. Represents an active session instance between
// the server and Chrome instance.
class TransportSessionImpl : public TransportSession {
 public:
  TransportSessionImpl(std::string_view session_id,
                       base::WeakPtr<TransportChannel> channel);
  ~TransportSessionImpl() override;

  TransportSessionImpl(const TransportSessionImpl&) = delete;
  TransportSessionImpl& operator=(const TransportSessionImpl&) = delete;

  // TransportSession implementation.
  std::string_view GetSessionId() const override;
  base::expected<void, SendMessageError> SendMessage(
      PayloadType payload_type,
      std::string_view payload) override;

  // Last sequence number for the session received from the server, used for
  // message ordering
  int64_t last_seen_sequence_number() const;
  bool has_last_seen_sequence_number() const;

  // Updates the last seen sequence number if the new sequence number is
  // positive and strictly greater than the current sequence number.
  void RecordServerSequenceNumber(int64_t seq);

  int64_t client_sequence_number() const;
  // Increments the sequence number for the next outgoing message.
  int64_t IncrementClientSequenceNumber();

  base::WeakPtr<TransportSessionImpl> GetWeakPtr();

  // TODO(crbug.com/525542155): Add downstream capabilities and support setting
  // server sequence numbers.

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  const std::string session_id_;
  base::WeakPtr<TransportChannel> channel_;

  int64_t last_seen_sequence_number_ = 0;
  int64_t client_sequence_number_ = 0;

  base::WeakPtrFactory<TransportSessionImpl> weak_ptr_factory_{this};
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_SESSION_IMPL_H_
