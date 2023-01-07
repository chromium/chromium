// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_EVENTS_PROTOCOL_EVENT_BUFFER_H_
#define COMPONENTS_SYNC_ENGINE_EVENTS_PROTOCOL_EVENT_BUFFER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"

namespace syncer {

class ProtocolEvent;

// A container for ProtocolEvents.
//
// Stores some maximum number of events (kDefaultBufferSize, unless overridden
// by command-line param), then starts dropping the oldest events.
class ProtocolEventBuffer {
 public:
  static const size_t kDefaultBufferSize;

  ProtocolEventBuffer();

  ProtocolEventBuffer(const ProtocolEventBuffer&) = delete;
  ProtocolEventBuffer& operator=(const ProtocolEventBuffer&) = delete;

  ~ProtocolEventBuffer();

  // Records an event.  May cause the oldest event to be dropped.
  void RecordProtocolEvent(const ProtocolEvent& event);

  // Returns copies of the buffered contents.  Will not clear the buffer.
  std::vector<std::unique_ptr<ProtocolEvent>> GetBufferedProtocolEvents() const;

 private:
  const size_t buffer_size_;
  base::circular_deque<std::unique_ptr<ProtocolEvent>> buffer_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_EVENTS_PROTOCOL_EVENT_BUFFER_H_
