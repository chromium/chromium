// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULING_METRICS_THREAD_TYPE_H_
#define COMPONENTS_SCHEDULING_METRICS_THREAD_TYPE_H_

namespace scheduling_metrics {

// The list of all threads in the Chrome we support scheduling metrics for.
// This enum is used as a key in histograms and should not be renumbered.
// Please update SchedulerThreadType enum in tools/metrics/histograms/enums.xml
// when adding new values.
enum class ThreadType {
  kBrowserUIThread = 0,
  kBrowserIOThread = 1,
  kRendererMainThread = 2,
  kRendererCompositorThread = 3,
  kRendererDedicatedWorkerThread = 4,
  kRendererServiceWorkerThread = 5,
  // Blink has ~10 other named threads, however they run just a few tasks.
  // Aggregate them into a single item for clarity and split out if necessary.
  kRendererOtherBlinkThread = 6,

  kMaxValue = kRendererOtherBlinkThread,
};

}  // namespace scheduling_metrics

#endif  // COMPONENTS_SCHEDULING_METRICS_THREAD_TYPE_H_
