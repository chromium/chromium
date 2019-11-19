// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/crash_memory_metrics_collector_android.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/crash/crash_memory_metrics_reporter.mojom.h"

// Keys used to attach handler to the RenderProcessHost
const void* const CrashMemoryMetricsCollector::kCrashMemoryMetricsCollectorKey =
    &CrashMemoryMetricsCollector::kCrashMemoryMetricsCollectorKey;

CrashMemoryMetricsCollector*
CrashMemoryMetricsCollector::GetFromRenderProcessHost(
    content::RenderProcessHost* rph) {
  return static_cast<CrashMemoryMetricsCollector*>(rph->GetUserData(
      CrashMemoryMetricsCollector::kCrashMemoryMetricsCollectorKey));
}

CrashMemoryMetricsCollector::CrashMemoryMetricsCollector(
    content::RenderProcessHost* rph) {
  // Create shared memory and pass it to the CrashMemoryMetricsReporter.
  base::UnsafeSharedMemoryRegion shared_metrics_buffer =
      base::UnsafeSharedMemoryRegion::Create(
          sizeof(blink::OomInterventionMetrics));
  metrics_mapping_ = shared_metrics_buffer.Map();
  memset(metrics_mapping_.memory(), 0, sizeof(blink::OomInterventionMetrics));

  mojo::Remote<blink::mojom::CrashMemoryMetricsReporter> reporter;
  rph->BindReceiver(reporter.BindNewPipeAndPassReceiver());
  reporter->SetSharedMemory(shared_metrics_buffer.Duplicate());
}

CrashMemoryMetricsCollector::~CrashMemoryMetricsCollector() = default;

const blink::OomInterventionMetrics*
CrashMemoryMetricsCollector::MemoryMetrics() {
  // This should be called after SetSharedMemory.
  DCHECK(metrics_mapping_.IsValid());
  return metrics_mapping_.GetMemoryAs<blink::OomInterventionMetrics>();
}
