// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_CONTENT_CONTENT_TRACING_MANAGER_H_
#define COMPONENTS_FEEDBACK_CONTENT_CONTENT_TRACING_MANAGER_H_

#include "components/feedback/tracing_manager.h"

#include <map>
#include <memory>
#include <string>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

// This class is used to manage performance metrics that can be attached to
// feedback reports.  This class is a Singleton that is owned by the preference
// system.  It should only be created when it is enabled, and should only be
// accessed elsewhere via Get().
//
// When a performance trace is desired, TracingManager::Get()->RequestTrace()
// should be invoked.  The TracingManager will then start preparing a zipped
// version of the performance data.  That data can then be requested via
// GetTraceData().  When the data is no longer needed, it should be discarded
// via DiscardTraceData().
class ContentTracingManager final : public TracingManager {
 public:
  ~ContentTracingManager() override;

  // Create a ContentTracingManager.  Can only be called when none exists.
  static std::unique_ptr<ContentTracingManager> Create();

  // Get the current ContentTracingManager.  Returns NULL if one doesn't exist.
  static ContentTracingManager* Get();

  // Request a trace ending at the current time.  If a trace is already being
  // collected, the id for that trace is returned.
  int RequestTrace() override;

  // Get the trace data for |id|.  On success, true is returned, and the data is
  // returned via |callback|.  Returns false on failure.
  bool GetTraceData(int id, TraceDataCallback callback) override;

  // Discard the data for trace |id|.
  void DiscardTraceData(int id) override;

  base::WeakPtr<TracingManager> AsWeakPtr() override;

 private:
  ContentTracingManager();

  void StartTracing();
  void OnTraceDataCollected(std::unique_ptr<std::string> data);
  void OnTraceDataCompressed(scoped_refptr<base::RefCountedString> data);

  // ID of the trace that is being collected.
  int current_trace_id_ = 0;

  // Mapping of trace ID to trace data.
  std::map<int, scoped_refptr<base::RefCountedString>> trace_data_;

  // Callback for the current trace request.
  TraceDataCallback trace_callback_;

  base::WeakPtrFactory<ContentTracingManager> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_FEEDBACK_CONTENT_CONTENT_TRACING_MANAGER_H_
