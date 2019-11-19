// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_MEMORY_METRICS_COLLECTOR_ANDROID_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_MEMORY_METRICS_COLLECTOR_ANDROID_H_

#include "base/memory/shared_memory_mapping.h"
#include "base/supports_user_data.h"
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "content/public/browser/content_browser_client.h"
#include "third_party/blink/public/common/oom_intervention/oom_intervention_types.h"

// This class manages a shared memory that is shared with
// CrashMemoryMetricsReporter on the renderer side. The shared memory contains
// blink memory metrics, and this class manages the metrics so that it can be
// uploaded from the browser when the renderer crashes. The lifetime is the same
// as the renderer process, as |this| is attached to the corresponding
// RenderProcessHost as a UserData.
class CrashMemoryMetricsCollector : public base::SupportsUserData::Data {
 public:
  explicit CrashMemoryMetricsCollector(content::RenderProcessHost* host);
  ~CrashMemoryMetricsCollector() override;

  // Key used to attach the handler to the RenderProcessHost.
  static const void* const kCrashMemoryMetricsCollectorKey;

  static CrashMemoryMetricsCollector* GetFromRenderProcessHost(
      content::RenderProcessHost* rph);

  // Gets the memory metrics that are filled on the renderer side.
  const blink::OomInterventionMetrics* MemoryMetrics();

 private:
  base::WritableSharedMemoryMapping metrics_mapping_;

  DISALLOW_COPY_AND_ASSIGN(CrashMemoryMetricsCollector);
};

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_MEMORY_METRICS_COLLECTOR_ANDROID_H_
