// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_MOUSE_MOVE_TOOL_H_
#define CHROME_RENDERER_ACTOR_MOUSE_MOVE_TOOL_H_

#include <cstdint>

#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/renderer/actor/tool_base.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace actor {

// A tool that can be invoked to perform a mouse move over a target.
class MouseMoveTool : public ToolBase {
 public:
  MouseMoveTool(mojom::MouseMoveActionPtr action, content::RenderFrame& frame);

  ~MouseMoveTool() override;

  // actor::ToolBase
  void Execute(ToolFinishedCallback callback) override;
  std::string DebugString() const override;

 private:
  using ValidatedResult = base::expected<gfx::PointF, mojom::ActionResultPtr>;
  ValidatedResult Validate() const;

  // Raw ref since this is owned by ToolExecutor whose lifetime is tied to
  // RenderFrame.
  base::raw_ref<content::RenderFrame> frame_;
  mojom::MouseMoveActionPtr action_;
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_MOUSE_MOVE_TOOL_H_
