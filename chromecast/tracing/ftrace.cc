// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/tracing/ftrace.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string_view>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/trace_event/common/trace_event_common.h"
#include "chromecast/tracing/system_tracing_common.h"

namespace chromecast {
namespace tracing {
namespace {

const char kPathTracefs[] = "/sys/kernel/tracing";
const char kPathDebugfsTracing[] = "/sys/kernel/debug/tracing";
const char kTraceFileTracingOn[] = "tracing_on";
const char kTraceFileTraceMarker[] = "trace_marker";
const char kTraceFileSetEvent[] = "set_event";
const char kTraceFileTraceClock[] = "trace_clock";
const char kTraceFileTrace[] = "trace";
const char kTraceFileBufferSizeKb[] = "buffer_size_kb";

const char kBufferSizeKbRunning[] = "7040";
const char kBufferSizeKbIdle[] = "1408";

// TODO(spang): Load these lists from a configuration file.
const char* const kGfxEvents[] = {
    "i915:i915_flip_request",        "i915:i915_flip_complete",
    "i915:i915_gem_object_pwrite",   "i915:intel_gpu_freq_change",
    "exynos:exynos_flip_request",    "exynos:exynos_flip_complete",
    "exynos:exynos_page_flip_state", "drm:drm_vblank_event",
};

const char* const kInputEvents[] = {
    "irq:irq_threaded_handler_entry", "irq:irq_threaded_handler_exit",
};

const char* const kIrqEvents[] = {
    "irq:irq_handler_exit", "irq:irq_handler_entry",
};

const char* const kPowerEvents[] = {
    "power:cpu_idle",
    "power:cpu_frequency",
    "mali:mali_dvfs_set_clock",
    "mali:mali_dvfs_set_voltage",
    "cpufreq_interactive:cpufreq_interactive_boost",
    "cpufreq_interactive:cpufreq_interactive_unboost",
    "exynos_busfreq:exynos_busfreq_target_int",
    "exynos_busfreq:exynos_busfreq_target_mif",
};

const char* const kSchedEvents[] = {
    "sched:sched_switch", "sched:sched_wakeup",
};

const char* const kWorkqEvents[] = {
    "workqueue:workqueue_execute_start", "workqueue:workqueue_execute_end",
};

void AddCategoryEvents(const std::string& category,
                       std::vector<std::string>* events) {
  if (category == "gfx") {
    base::ranges::copy(kGfxEvents, std::back_inserter(*events));
    return;
  }
  if (category == "input") {
    base::ranges::copy(kInputEvents, std::back_inserter(*events));
    return;
  }
  if (category == TRACE_DISABLED_BY_DEFAULT("irq")) {
    base::ranges::copy(kIrqEvents, std::back_inserter(*events));
    return;
  }
  if (category == "power") {
    base::ranges::copy(kPowerEvents, std::back_inserter(*events));
    return;
  }
  if (category == "sched") {
    base::ranges::copy(kSchedEvents, std::back_inserter(*events));
    return;
  }
  if (category == "workq") {
    base::ranges::copy(kWorkqEvents, std::back_inserter(*events));
    return;
  }

  LOG(WARNING) << "Unrecognized category: " << category;
}

bool WriteTracingFile(const char* tracing_dir,
                      const char* trace_file,
                      std::string_view contents) {
  base::FilePath path = base::FilePath(tracing_dir).Append(trace_file);

  if (!base::WriteFile(path, contents)) {
    PLOG(ERROR) << "write: " << path;
    return false;
  }

  return true;
}

bool EnableTraceEvent(const char* tracing_dir, std::string_view event) {
  base::FilePath path = base::FilePath(tracing_dir).Append(kTraceFileSetEvent);

  // Enabling events returns EINVAL if the event does not exist. It is normal
  // for driver specific events to be missing when the driver is not built in.
  if (!base::AppendToFile(path, event) && errno != EINVAL) {
    PLOG(ERROR) << "write: " << path;
    return false;
  }

  return true;
}

const char* FindTracingDir() {
  base::FilePath path = base::FilePath(kPathTracefs).Append(kTraceFileSetEvent);
  if (!access(path.value().c_str(), W_OK))
    return kPathTracefs;
  return kPathDebugfsTracing;
}

}  // namespace

bool IsValidCategory(std::string_view str) {
  for (std::string_view category : kCategories) {
    if (category == str) {
      return true;
    }
  }

  return false;
}

bool StartFtrace(const std::vector<std::string>& categories) {
  if (categories.size() == 0) {
    LOG(ERROR) << "No categories to enable";
    return false;
  }

  std::vector<std::string> events;
  for (const auto& category : categories)
    AddCategoryEvents(category, &events);

  if (events.size() == 0) {
    LOG(ERROR) << "No events to enable";
    return false;
  }

  const char* tracing_dir = FindTracingDir();

  // Disable tracing and clear events.
  if (!WriteTracingFile(tracing_dir, kTraceFileTracingOn, "0"))
    return false;
  if (!WriteTracingFile(tracing_dir, kTraceFileSetEvent, "\n"))
    return false;

  // Use CLOCK_MONOTONIC so that kernel timestamps align with std::steady_clock
  // and base::TimeTicks.
  if (!WriteTracingFile(tracing_dir, kTraceFileTraceClock, "mono"))
    return false;

  for (const auto& event : events)
    EnableTraceEvent(tracing_dir, event);

  if (!WriteTracingFile(tracing_dir, kTraceFileBufferSizeKb,
                        kBufferSizeKbRunning))
    return false;
  if (!WriteTracingFile(tracing_dir, kTraceFileTracingOn, "1"))
    return false;

  return true;
}

bool WriteFtraceTimeSyncMarker() {
  return WriteTracingFile(FindTracingDir(), kTraceFileTraceMarker,
                          "trace_event_clock_sync: parent_ts=0");
}

bool StopFtrace() {
  if (!WriteTracingFile(FindTracingDir(), kTraceFileTracingOn, "0"))
    return false;
  return true;
}

base::ScopedFD GetFtraceData() {
  base::FilePath path =
      base::FilePath(FindTracingDir()).Append(kTraceFileTrace);
  base::ScopedFD trace_data(HANDLE_EINTR(
      open(path.value().c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK)));
  if (!trace_data.is_valid())
    PLOG(ERROR) << "open: " << path.value();
  return trace_data;
}

bool ClearFtrace() {
  const char* tracing_dir = FindTracingDir();
  if (!WriteTracingFile(tracing_dir, kTraceFileBufferSizeKb, kBufferSizeKbIdle))
    return false;
  if (!WriteTracingFile(tracing_dir, kTraceFileTrace, "0"))
    return false;
  return true;
}

}  // namespace tracing
}  // namespace chromecast
