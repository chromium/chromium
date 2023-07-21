// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DEBUGGER_VIZ_DEBUGGER_UNITTESTS_VIZ_DEBUGGER_INTERNAL_H_
#define COMPONENTS_VIZ_SERVICE_DEBUGGER_VIZ_DEBUGGER_UNITTESTS_VIZ_DEBUGGER_INTERNAL_H_

#include <stdint.h>

#include <vector>

#include "components/viz/service/debugger/viz_debugger.h"

#if BUILDFLAG(USE_VIZ_DEBUGGER)

namespace viz {

// The VizDebuggerInternal class is used for creating a
// VizDebugger instance for VizDebugger unit tests.
class VizDebuggerInternal : public VizDebugger {
 public:
  VizDebuggerInternal();
  ~VizDebuggerInternal();

  void ForceEnabled();
  int GetSubmissionCount();
  void SetBufferCapacities(uint32_t bufferSize);
  // Resets and clears all the VizDebugger instance variables
  // and vectors.
  bool Reset();

  // Returns copies of corresponding buffers/vectors in the
  // VizDebugger instance.
  std::vector<DrawCall> GetDrawRectCalls();
  std::vector<LogCall> GetLogs();

  // These functions return the tail index of each type of buffers.
  int GetRectCallsTailIdx();
  int GetLogsTailIdx();

  // These functions get the size of each buffer.
  int GetRectCallsSize();
  int GetLogsSize();

  // This function returns a pointer to the Read-Write lock used
  // for VizDebugger's thread-safety.
  rwlock::RWLock* GetRWLock();

  int GetSourceCount();

  using VizDebugger::CallSubmitCommon;
  using VizDebugger::DrawCall;
  using VizDebugger::FrameAsJson;
  using VizDebugger::LogCall;
  using VizDebugger::UpdateFilters;
};
}  // namespace viz

#endif  // BUILDFLAG(USE_VIZ_DEBUGGER)

#endif  // COMPONENTS_VIZ_SERVICE_DEBUGGER_VIZ_DEBUGGER_UNITTESTS_VIZ_DEBUGGER_INTERNAL_H_
