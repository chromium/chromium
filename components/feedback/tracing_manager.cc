// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/tracing_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/feedback/feedback_util.h"
#include "content/public/browser/tracing_controller.h"

namespace {

// Only once trace manager can exist at a time.
TracingManager* g_tracing_manager = nullptr;
// Trace IDs start at 1 and increase.
int g_next_trace_id = 1;
// Name of the file to store the tracing data as.
const base::FilePath::CharType kTracingFilename[] =
    FILE_PATH_LITERAL("tracing.json");
}

TracingManager::TracingManager()
    : current_trace_id_(0),
      weak_ptr_factory_(this) {
  DCHECK(!g_tracing_manager);
  g_tracing_manager = this;
  StartTracing();
}

TracingManager::~TracingManager() {
  DCHECK(g_tracing_manager == this);
  g_tracing_manager = nullptr;
}

int TracingManager::RequestTrace() {
  // Return the current trace if one is being collected.
  if (current_trace_id_)
    return current_trace_id_;

  current_trace_id_ = g_next_trace_id;
  ++g_next_trace_id;
  content::TracingController::GetInstance()->StopTracing(
      content::TracingController::CreateStringEndpoint(
          base::Bind(&TracingManager::OnTraceDataCollected,
                     weak_ptr_factory_.GetWeakPtr())));
  return current_trace_id_;
}

bool TracingManager::GetTraceData(int id, const TraceDataCallback& callback) {
  // If a trace is being collected currently, send it via callback when
  // complete.
  if (current_trace_id_) {
    // Only allow one trace data request at a time.
    if (trace_callback_.is_null()) {
      trace_callback_ = callback;
      return true;
    } else {
      return false;
    }
  } else {
    auto data = trace_data_.find(id);
    if (data == trace_data_.end())
      return false;

    // Always return the data asychronously, so the behavior is consistant.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback, data->second));
    return true;
  }
}

void TracingManager::DiscardTraceData(int id) {
  trace_data_.erase(id);

  // If the trace is discarded before it is complete, clean up the accumulators.
  if (id == current_trace_id_) {
    current_trace_id_ = 0;

    // If the trace has already been requested, provide an empty string.
    if (!trace_callback_.is_null()) {
      trace_callback_.Run(scoped_refptr<base::RefCountedString>());
      trace_callback_.Reset();
    }
  }
}

void TracingManager::StartTracing() {
  content::TracingController::GetInstance()->StartTracing(
      base::trace_event::TraceConfig(),
      content::TracingController::StartTracingDoneCallback());
}

void TracingManager::OnTraceDataCollected(
    std::unique_ptr<const base::DictionaryValue> metadata,
    base::RefCountedString* trace_data) {
  if (!current_trace_id_)
    return;

  std::string output_val;
  feedback_util::ZipString(
      base::FilePath(kTracingFilename), trace_data->data(), &output_val);

  scoped_refptr<base::RefCountedString> output(
      base::RefCountedString::TakeString(&output_val));

  trace_data_[current_trace_id_] = output;

  if (!trace_callback_.is_null()) {
    trace_callback_.Run(output);
    trace_callback_.Reset();
  }

  current_trace_id_ = 0;

  // Tracing has to be restarted asynchronous, so the TracingController can
  // clean up.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&TracingManager::StartTracing,
                                weak_ptr_factory_.GetWeakPtr()));
}

// static
std::unique_ptr<TracingManager> TracingManager::Create() {
  if (g_tracing_manager)
    return std::unique_ptr<TracingManager>();
  return std::unique_ptr<TracingManager>(new TracingManager());
}

TracingManager* TracingManager::Get() {
  return g_tracing_manager;
}
