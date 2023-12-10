// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_RENDERER_NO_STATE_PREFETCH_HELPER_H_
#define COMPONENTS_NO_STATE_PREFETCH_RENDERER_NO_STATE_PREFETCH_HELPER_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"

namespace blink {
class URLLoaderThrottle;
}  // namespace blink

namespace prerender {
class NoStatePrefetchURLLoaderThrottle;

// Helper class to track whether its RenderFrame is currently being no-state
// prefetched. Created when prefetching starts and deleted as soon as it stops.
class NoStatePrefetchHelper
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<NoStatePrefetchHelper> {
 public:
  NoStatePrefetchHelper(content::RenderFrame* render_frame,
                        const std::string& histogram_prefix);

  NoStatePrefetchHelper(const NoStatePrefetchHelper&) = delete;
  NoStatePrefetchHelper& operator=(const NoStatePrefetchHelper&) = delete;

  ~NoStatePrefetchHelper() override;

  // Configures and returns a new NoStatePrefetchURLLoaderThrottle instance if
  // the indicated frame has an associated NoStatePrefetchHelper.
  static std::unique_ptr<blink::URLLoaderThrottle> MaybeCreateThrottle(
      const blink::LocalFrameToken& frame_token);

  // Returns true if |render_frame| is currently prefetching.
  static bool IsPrefetching(const content::RenderFrame* render_frame);

  std::string histogram_prefix() const { return histogram_prefix_; }

 private:
  // RenderFrameObserver implementation.
  void DidDispatchDOMContentLoadedEvent() override;
  void OnDestruct() override;

  void AddThrottle(NoStatePrefetchURLLoaderThrottle& throttle);
  void OnThrottleDestroyed();
  void SendPrefetchFinished();

  std::string histogram_prefix_;

  int prefetch_count_ = 0;
  bool prefetch_finished_ = false;
  base::TimeTicks start_time_;

  base::WeakPtrFactory<NoStatePrefetchHelper> weak_factory_{this};
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_RENDERER_NO_STATE_PREFETCH_HELPER_H_
