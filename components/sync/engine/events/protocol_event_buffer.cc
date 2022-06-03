// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/events/protocol_event_buffer.h"

#include "components/sync/engine/events/protocol_event.h"

namespace syncer {

const size_t ProtocolEventBuffer::kBufferSize = 6;

ProtocolEventBuffer::ProtocolEventBuffer() = default;

ProtocolEventBuffer::~ProtocolEventBuffer() = default;

void ProtocolEventBuffer::RecordProtocolEvent(const ProtocolEvent& event) {
  buffer_.push_back(event.Clone());
  if (buffer_.size() > kBufferSize)
    buffer_.pop_front();
}

std::vector<std::unique_ptr<ProtocolEvent>>
ProtocolEventBuffer::GetBufferedProtocolEvents() const {
  std::vector<std::unique_ptr<ProtocolEvent>> ret;
  for (auto& event : buffer_)
    ret.push_back(event->Clone());

  return ret;
}

}  // namespace syncer
