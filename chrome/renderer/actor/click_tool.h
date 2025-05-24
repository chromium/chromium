// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_CLICK_TOOL_H_
#define CHROME_RENDERER_ACTOR_CLICK_TOOL_H_

#include <cstdint>

#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/renderer/actor/tool_base.h"

namespace blink {
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
  ClickTool(mojom::ClickActionPtr action, content::RenderFrame& frame);
  ~ClickTool() override;

  // actor::ToolBase
  void Execute(ToolFinishedCallback callback) override;
  std::string DebugString() const override;

 private:
  using ValidatedResult = base::expected<gfx::PointF, mojom::ActionResultPtr>;
  ValidatedResult Validate() const;

  void SendMouseUp(blink::WebMouseEvent mouse_event,
                   ToolFinishedCallback callback);

  // Raw ref since this is owned by ToolExecutor whose lifetime is tied to
  // RenderFrame.
  base::raw_ref<content::RenderFrame> frame_;
  mojom::ClickActionPtr action_;
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_CLICK_TOOL_H_
