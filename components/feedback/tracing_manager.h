// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_TRACING_MANAGER_H_
#define COMPONENTS_FEEDBACK_TRACING_MANAGER_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

namespace base {
class RefCountedString;
}  // namespace base

// Callback used for getting the output of a trace.
using TraceDataCallback =
    base::OnceCallback<void(scoped_refptr<base::RefCountedString> trace_data)>;

// This class is used to manage performance metrics that can be attached to
// feedback reports.  This class is a pure interface.
//
// When a performance trace is desired, TracingManager::RequestTrace() should
// be invoked.  The TracingManager will then start preparing a zipped version
// of the performance data.  That data can then be requested via GetTraceData().
// When the data is no longer needed, it should be discarded via
// DiscardTraceData().
class TracingManager {
 public:
  virtual ~TracingManager();

  TracingManager(const TracingManager&) = delete;
  TracingManager& operator=(const TracingManager&) = delete;

  // Request a trace ending at the current time.  If a trace is already being
  // collected, the id for that trace is returned.
  virtual int RequestTrace() = 0;

  // Get the trace data for |id|.  On success, true is returned, and the data is
  // returned via |callback|.  Returns false on failure.
  virtual bool GetTraceData(int id, TraceDataCallback callback) = 0;

  // Discard the data for trace |id|.
  virtual void DiscardTraceData(int id) = 0;

  // Derived classes must implement this and return pointers from
  // a `base::WeakPtrFactory` data member.
  virtual base::WeakPtr<TracingManager> AsWeakPtr() = 0;

 protected:
  TracingManager();
};

#endif  // COMPONENTS_FEEDBACK_TRACING_MANAGER_H_
