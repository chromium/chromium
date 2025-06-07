// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_PAGE_STABILITY_MONITOR_H_
#define CHROME_RENDERER_ACTOR_PAGE_STABILITY_MONITOR_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace actor {

// Helper class for monitoring page stability after tool usage.
class PageStabilityMonitor {
 public:
  // Constructs the monitor and takes a baseline observation of the document in
  // the given RenderFrame.
  explicit PageStabilityMonitor(content::RenderFrame& frame);
  ~PageStabilityMonitor();

  // Invokes the given callback when the page is deemed stable enough for an
  // observation to take place.
  void WaitForStable(base::OnceClosure callback);

 private:
  // RawRef since this object is ultimately tied to a RenderFrame so can't
  // outlive it.
  base::raw_ref<content::RenderFrame> frame_;
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_PAGE_STABILITY_MONITOR_H_
