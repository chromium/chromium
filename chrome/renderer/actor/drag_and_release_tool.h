// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_DRAG_AND_RELEASE_TOOL_H_
#define CHROME_RENDERER_ACTOR_DRAG_AND_RELEASE_TOOL_H_

#include <cstdint>

#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/renderer/actor/tool_base.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace gfx {
class PointF;
}  // namespace gfx

namespace actor {

// A tool that can be invoked to perform a drag and release over a target.
class DragAndReleaseTool : public ToolBase {
 public:
  DragAndReleaseTool(mojom::DragAndReleaseActionPtr action,
                     content::RenderFrame& frame);

  ~DragAndReleaseTool() override;

  // actor::ToolBase
  void Execute(ToolFinishedCallback callback) override;
  std::string DebugString() const override;

 private:
  struct DragParams {
    gfx::PointF from;
    gfx::PointF to;
  };
  using ValidatedResult = base::expected<DragParams, mojom::ActionResultPtr>;
  ValidatedResult Validate() const;

  bool InjectMouseEvent(blink::WebInputEvent::Type type,
                        const gfx::PointF& position_in_widget,
                        blink::WebMouseEvent::Button button);

  // Raw ref since this is owned by ToolExecutor whose lifetime is tied to
  // RenderFrame.
  base::raw_ref<content::RenderFrame> frame_;
  mojom::DragAndReleaseActionPtr action_;
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_DRAG_AND_RELEASE_TOOL_H_
