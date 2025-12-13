// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_NETWORK_AND_MAIN_THREAD_STABILITY_MONITOR_H_
#define CHROME_RENDERER_ACTOR_NETWORK_AND_MAIN_THREAD_STABILITY_MONITOR_H_

#include <stddef.h>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/actor/task_id.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace content {
class RenderFrame;
}  // namespace content

namespace actor {

class Journal;

// Helper class for monitoring network and main thread stability after tool
// usage. This is owned by `PageStabilityMonitor`.
class NetworkAndMainThreadStabilityMonitor {
 public:
  NetworkAndMainThreadStabilityMonitor(content::RenderFrame& frame,
                                       TaskId task_id,
                                       Journal& journal);
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

  TaskId task_id_;

  raw_ref<Journal> journal_;

  base::WeakPtrFactory<NetworkAndMainThreadStabilityMonitor> weak_ptr_factory_{
      this};
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_NETWORK_AND_MAIN_THREAD_STABILITY_MONITOR_H_
