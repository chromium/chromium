// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_TRACING_MANAGER_H_
#define COMPONENTS_FEEDBACK_TRACING_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"

namespace base {

class RefCountedString;

}
// Callback used for getting the output of a trace.
typedef base::Callback<void(scoped_refptr<base::RefCountedString> trace_data)>
    TraceDataCallback;

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
class TracingManager {
 public:
  virtual ~TracingManager();

  // Create a TracingManager.  Can only be called when none exists.
  static std::unique_ptr<TracingManager> Create();

  // Get the current TracingManager.  Returns NULL if one doesn't exist.
  static TracingManager* Get();

  // Request a trace ending at the current time.  If a trace is already being
  // collected, the id for that trace is returned.
  int RequestTrace();

  // Get the trace data for |id|.  On success, true is returned, and the data is
  // returned via |callback|.  Returns false on failure.
  bool GetTraceData(int id, const TraceDataCallback& callback);

  // Discard the data for trace |id|.
  void DiscardTraceData(int id);

 private:
  TracingManager();

  void StartTracing();
  void OnTraceDataCollected(std::unique_ptr<std::string> data);

  // ID of the trace that is being collected.
  int current_trace_id_;

  // Mapping of trace ID to trace data.
  std::map<int, scoped_refptr<base::RefCountedString> > trace_data_;

  // Callback for the current trace request.
  TraceDataCallback trace_callback_;

  base::WeakPtrFactory<TracingManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TracingManager);
};

#endif  // COMPONENTS_FEEDBACK_TRACING_MANAGER_H_

