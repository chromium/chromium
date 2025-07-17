// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_TOOL_BASE_H_
#define CHROME_RENDERER_ACTOR_TOOL_BASE_H_

#include <cstdint>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/renderer/actor/journal.h"
#include "third_party/blink/public/web/web_node.h"

namespace content {
class RenderFrame;
}

namespace actor {

class ToolBase {
 public:
  using ToolFinishedCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;
  ToolBase(content::RenderFrame& frame,
           Journal::TaskId task_id,
           Journal& journal,
           mojom::ToolTargetPtr target,
           mojom::ObservedToolTargetPtr observed_target);
  virtual ~ToolBase();

  // Executes the tool. `callback` is invoked with the tool result.
  virtual void Execute(ToolFinishedCallback callback) = 0;

  // Struct to hold the resolved target information.
  struct ResolvedTarget {
    // The node identified by the target. May be null if the node has been
    // removed from DOM.
    blink::WebNode node;
    // The interaction point of node in viewport coordinates. Currently defaults
    // to center point of node's bounding rect.
    gfx::PointF point;
  };

  // Validate that target passes tool-agnostic validation (e.g. within
  // viewport, no change between observation and time of use) and resolve the
  // mojom target to Node and Point, ready for tool use.
  base::expected<ResolvedTarget, mojom::ActionResultPtr>
  ValidateAndResolveTarget() const;

  // Returns a human readable string representing this tool and its parameters.
  // Used primarily for logging and debugging.
  virtual std::string DebugString() const = 0;

  // The amount of time to wait when observing tool execution before starting to
  // wait for page stability. 0 by default, meaning no delay, but tools can
  // override this on a case-by-case basis when the expected effects of tool use
  // may happen asynchronously outside of the injected events.
  virtual base::TimeDelta ExecutionObservationDelay() const;

 protected:
  // Raw ref since this is owned by ToolExecutor whose lifetime is tied to
  // RenderFrame.
  base::raw_ref<content::RenderFrame> frame_;
  Journal::TaskId task_id_;
  base::raw_ref<Journal> journal_;
  mojom::ToolTargetPtr target_;
  mojom::ObservedToolTargetPtr observed_target_;

 private:
  // Validate that resolved target matches the observed target from last
  // observation.
  base::expected<ResolvedTarget, mojom::ActionResultPtr> ValidateTimeOfUse(
      const ResolvedTarget& resolved_target) const;
};
}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_TOOL_BASE_H_
