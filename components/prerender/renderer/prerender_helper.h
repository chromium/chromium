// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRERENDER_RENDERER_PRERENDER_HELPER_H_
#define COMPONENTS_PRERENDER_RENDERER_PRERENDER_HELPER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/prerender/common/prerender_types.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"

namespace blink {
class URLLoaderThrottle;
}  // namespace blink

namespace prerender {
class PrerenderURLLoaderThrottle;

// Helper class to track whether its RenderFrame is currently being prerendered.
// Created when prerendering starts and deleted as soon as it stops.
class PrerenderHelper
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<PrerenderHelper> {
 public:
  PrerenderHelper(content::RenderFrame* render_frame,
                  prerender::mojom::PrerenderMode prerender_mode,
                  const std::string& histogram_prefix);

  ~PrerenderHelper() override;

  // Configures and returns a new PrerenderURLLoaderThrottle instance if the
  // indicated frame has an associated PrerenderHelper.
  static std::unique_ptr<blink::URLLoaderThrottle> MaybeCreateThrottle(
      int render_frame_id);

  // Returns true if |render_frame| is currently prerendering.
  static bool IsPrerendering(const content::RenderFrame* render_frame);

  static prerender::mojom::PrerenderMode GetPrerenderMode(
      const content::RenderFrame* render_frame);

  prerender::mojom::PrerenderMode prerender_mode() const {
    return prerender_mode_;
  }
  std::string histogram_prefix() const { return histogram_prefix_; }

 private:
  // RenderFrameObserver implementation.
  void DidFinishDocumentLoad() override;
  void OnDestruct() override;

  void AddThrottle(PrerenderURLLoaderThrottle& throttle);
  void OnThrottleDestroyed();
  void SendPrefetchFinished();

  const prerender::mojom::PrerenderMode prerender_mode_;
  std::string histogram_prefix_;

  int prefetch_count_ = 0;
  bool prefetch_finished_ = false;
  base::TimeTicks start_time_;
  base::TimeTicks parsed_time_;

  base::WeakPtrFactory<PrerenderHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrerenderHelper);
};

}  // namespace prerender

#endif  // COMPONENTS_PRERENDER_RENDERER_PRERENDER_HELPER_H_
