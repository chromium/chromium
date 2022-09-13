// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/events/protocol_event_buffer.h"

#include <string>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/engine/events/protocol_event.h"

namespace syncer {

const size_t ProtocolEventBuffer::kDefaultBufferSize = 6;

namespace {

size_t GetBufferSize() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  std::string buffer_size =
      cmd_line->GetSwitchValueASCII(kSyncProtocolLogBufferSize);
  size_t result = 0;
  if (!base::StringToSizeT(buffer_size, &result)) {
    result = ProtocolEventBuffer::kDefaultBufferSize;
  }
  return result;
}

}  // namespace

ProtocolEventBuffer::ProtocolEventBuffer() : buffer_size_(GetBufferSize()) {}

ProtocolEventBuffer::~ProtocolEventBuffer() = default;

void ProtocolEventBuffer::RecordProtocolEvent(const ProtocolEvent& event) {
  if (buffer_.size() >= buffer_size_) {
    buffer_.pop_front();
  }
  buffer_.push_back(event.Clone());
}

std::vector<std::unique_ptr<ProtocolEvent>>
ProtocolEventBuffer::GetBufferedProtocolEvents() const {
  std::vector<std::unique_ptr<ProtocolEvent>> ret;
  for (const std::unique_ptr<ProtocolEvent>& event : buffer_) {
    ret.push_back(event->Clone());
  }

  return ret;
}

}  // namespace syncer
