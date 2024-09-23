// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_TRACING_SYSTEM_TRACER_H_
#define CHROMECAST_TRACING_SYSTEM_TRACER_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"

namespace chromecast {

class SystemTracer {
 public:
  static std::unique_ptr<SystemTracer> Create();

  SystemTracer(const SystemTracer&) = delete;
  SystemTracer& operator=(const SystemTracer&) = delete;

  virtual ~SystemTracer() = default;

  enum class Status {
    OK,
    KEEP_GOING,
    FAIL,
  };

  using StartTracingCallback = base::OnceCallback<void(Status)>;
  using StopTracingCallback =
      base::RepeatingCallback<void(Status status, std::string trace_data)>;

  // Start system tracing for categories in |categories| (comma separated).
  virtual void StartTracing(std::string_view categories,
                            StartTracingCallback callback) = 0;

  // Stop system tracing.
  //
  // This will call |callback| on the current thread with the trace data. If
  // |status| is Status::KEEP_GOING, another call will be made with additional
  // data.
  virtual void StopTracing(const StopTracingCallback& callback) = 0;

 protected:
  SystemTracer() = default;
};

}  // namespace chromecast

#endif  // CHROMECAST_TRACING_SYSTEM_TRACER_H_
