// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/debugger/viz_debugger_unittests/viz_debugger_internal.h"

#if VIZ_DEBUGGER_IS_ON()

namespace viz {

void VizDebuggerInternal::ForceEnabled() {
  enabled_ = true;
}

std::vector<VizDebuggerInternal::DrawCall>
VizDebuggerInternal::GetDrawRectCalls() {
  return draw_rect_calls_;
}

std::vector<VizDebuggerInternal::LogCall> VizDebuggerInternal::GetLogs() {
  return logs_;
}

int VizDebuggerInternal::GetSourceCount() {
  return static_cast<int>(sources_.size());
}

int VizDebuggerInternal::GetSubmissionCount() {
  return submission_count_;
}

int VizDebuggerInternal::GetRectCallsTailIdx() {
  return draw_calls_tail_idx_;
}

int VizDebuggerInternal::GetLogsTailIdx() {
  return logs_tail_idx_;
}

int VizDebuggerInternal::GetRectCallsSize() {
  return draw_rect_calls_.size();
}

int VizDebuggerInternal::GetLogsSize() {
  return logs_.size();
}

rwlock::RWLock* VizDebuggerInternal::GetRWLock() {
  return &read_write_lock_;
}

void VizDebuggerInternal::SetBufferCapacities(uint32_t bufferSize) {
  draw_rect_calls_.resize(bufferSize);
  logs_.resize(bufferSize);
  sources_.reserve(bufferSize);
}

bool VizDebuggerInternal::Reset() {
  submission_count_ = 0;
  buffer_id = 0;
  draw_rect_calls_.clear();

  logs_.clear();
  buffers_.clear();

  draw_rect_calls_.resize(kDefaultBufferSize);

  logs_.resize(kDefaultBufferSize);

  last_sent_source_count_ = 0;
  sources_.clear();
  // Reset index counters for each buffer.
  draw_calls_tail_idx_ = 0;

  logs_tail_idx_ = 0;
  enabled_ = false;
  new_filters_.clear();
  cached_filters_.clear();
  return true;
}
}  // namespace viz

#endif  // VIZ_DEBUGGER_IS_ON()
