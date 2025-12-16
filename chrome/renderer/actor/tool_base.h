// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_TOOL_BASE_H_
#define CHROME_RENDERER_ACTOR_TOOL_BASE_H_

#include <cstdint>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/renderer/actor/journal.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_widget.h"

namespace content {
class RenderFrame;
}

namespace actor {

class ToolBase;

// Struct to return the resolved information about a tool target.
struct ResolvedTarget {
  // The node identified by the target. May be null if the node has been
  // removed from DOM.
  blink::WebNode node;

  // The interaction point of node in Blink (physical) pixels, relative to the
  // resolved widget's origin.
  gfx::PointF widget_point;

  // Set if the target hits a popup. If a popup is hit this handle identifies
  // the popup and is used to confirm the popup widget is the one that was
  // originally hit.
  std::optional<blink::WebPagePopup::Handle> popup_handle;

  // The widget this target resolved to. This can be either the frame's widget
  // or the popup widget. Since a widget can be destroyed by script or during
  // async yields, callers should always access via this getter, rather than
  // holding onto the pointer.
  blink::WebWidget* GetWidget(const ToolBase& tool) const;
};

class ToolBase {
 public:
  using ToolFinishedCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;
  ToolBase(content::RenderFrame& frame,
           TaskId task_id,
           Journal& journal,
           mojom::ToolTargetPtr target,
           mojom::ObservedToolTargetPtr observed_target);
  virtual ~ToolBase();

  // Executes the tool. `callback` is invoked with the tool result.
  virtual void Execute(ToolFinishedCallback callback) = 0;

  // Cancels execution of the tool.
  virtual void Cancel();

  // Returns a human readable string representing this tool and its parameters.
  // Used primarily for logging and debugging.
  virtual std::string DebugString() const = 0;

  // The amount of time to wait when observing tool execution before starting to
  // wait for page stability. 0 by default, meaning no delay, but tools can
  // override this on a case-by-case basis when the expected effects of tool use
  // may happen asynchronously outside of the injected events.
  virtual base::TimeDelta ExecutionObservationDelay() const;

  // Scrolls the target element into view if it's not already. If the target is
  // a coordinate, the coordinate is updated to reflect the new location after
  // scrolling. Returns true if a scroll into view was requested.
  virtual bool EnsureTargetInView();

  // Whether or not the tool supports page stability monitoring via paint
  // stability tracking, which is currently only supported on a subset of
  // interactions.
  virtual bool SupportsPaintStability() const;

  content::RenderFrame* frame() const { return &frame_.get(); }
  const TaskId& task_id() const { return task_id_; }

 protected:
  using ResolveResult = base::expected<ResolvedTarget, mojom::ActionResultPtr>;

  // Resolves the given target into the ResolvedTarget struct which includes
  // both a point to inject input events to and a DOM node to validate against.
  ResolveResult ResolveTarget(const mojom::ToolTarget& target) const;

  // Validate that target_ passes tool-agnostic validation (e.g. within
  // viewport, no change between observation and time of use) and resolve the
  // mojom target to Node and Point, ready for tool use.
  ResolveResult ValidateAndResolveTarget() const;

  // Raw ref since this is owned by ToolExecutor whose lifetime is tied to
  // RenderFrame.
  base::raw_ref<content::RenderFrame> frame_;
  TaskId task_id_;
  base::raw_ref<Journal> journal_;
  mojom::ToolTargetPtr target_;
  mojom::ObservedToolTargetPtr observed_target_;

 private:
  // Validate that resolved target matches the observed target from last
  // observation.
  mojom::ActionResultPtr ValidateTimeOfUse(
      const ResolvedTarget& resolved_target) const;
};
}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_TOOL_BASE_H_
