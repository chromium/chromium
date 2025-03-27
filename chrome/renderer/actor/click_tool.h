// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_CLICK_TOOL_H_
#define CHROME_RENDERER_ACTOR_CLICK_TOOL_H_

#include <cstdint>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/renderer/actor/tool_base.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace blink {
class WebNode;
class WebMouseEvent;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace gfx {
class PointF;
}  // namespace gfx

namespace actor {

// A tool that can be invoked to perform a click on a target.
class ClickTool : public ToolBase {
 public:
  ClickTool(mojom::ClickActionPtr action,
            base::raw_ref<content::RenderFrame> frame);
  ~ClickTool() override;

  // Performs a click on the specified node. Invoke callback with true if
  // success and false otherwise.
  void Execute(ToolFinishedCallback callback) override;

 private:
  blink::WebMouseEvent CreateClickMouseEvent(
      const blink::WebNode& node,
      const mojom::ClickAction::Type type,
      const mojom::ClickAction::Count count,
      blink::WebInputEvent::Type event_type,
      const gfx::PointF& click_point);

  void SendMouseUp(blink::WebMouseEvent mouse_event,
                   ToolFinishedCallback callback);

  // Raw ref since this is owned by ToolExecutor whose lifetime is tied to
  // RenderFrame.
  base::raw_ref<content::RenderFrame> frame_;
  mojom::ClickActionPtr action_;

  base::WeakPtrFactory<ClickTool> weak_factory_{this};
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_CLICK_TOOL_H_
