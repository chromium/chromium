// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_RENDERER_NETWORK_AND_MAIN_THREAD_STABILITY_MONITOR_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_RENDERER_NETWORK_AND_MAIN_THREAD_STABILITY_MONITOR_H_

#include <stddef.h>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace content {
class RenderFrame;
}  // namespace content

namespace page_content_annotations {

class PageStabilityMonitorDelegate;

// Helper class for monitoring network and main thread stability. This is owned
// by `PageStabilityMonitor`.
class NetworkAndMainThreadStabilityMonitor {
 public:
  explicit NetworkAndMainThreadStabilityMonitor(
      content::RenderFrame& frame,
      PageStabilityMonitorDelegate* delegate = nullptr);
  ~NetworkAndMainThreadStabilityMonitor();

  void WaitForStable(base::OnceClosure callback);

 private:
  void OnNetworkIdle();

  void WaitForMainThreadIdle();

  void OnMainThreadIdle(base::TimeTicks);

  // The number of active network requests at the time this object was
  // initialized. Used to compare to the number of requests after monitoring
  // begins to determine if new network requests were started in that interval.
  size_t starting_request_count_;

  base::OnceClosure is_stable_callback_;

  raw_ref<content::RenderFrame> render_frame_;

  // The delegate is owned by the PageStabilityMonitor that created this
  // sub-monitor. Both the paint and network/main thread monitors share the
  // same delegate as they represent a single, unified monitoring session
  // with consistent configuration and logging needs.
  raw_ptr<PageStabilityMonitorDelegate> delegate_ = nullptr;

  base::WeakPtrFactory<NetworkAndMainThreadStabilityMonitor> weak_ptr_factory_{
      this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_RENDERER_NETWORK_AND_MAIN_THREAD_STABILITY_MONITOR_H_
