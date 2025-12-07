// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_MOUSE_MOVE_TOOL_H_
#define CHROME_RENDERER_ACTOR_MOUSE_MOVE_TOOL_H_

#include <cstdint>

#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/renderer/actor/tool_base.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace actor {

// A tool that can be invoked to perform a mouse move over a target.
class MouseMoveTool : public ToolBase {
 public:
  MouseMoveTool(content::RenderFrame& frame,
                TaskId task_id,
                Journal& journal,
                mojom::MouseMoveActionPtr action,
                mojom::ToolTargetPtr target,
                mojom::ObservedToolTargetPtr observed_target);

  ~MouseMoveTool() override;

  // actor::ToolBase
  void Execute(ToolFinishedCallback callback) override;
  std::string DebugString() const override;

 private:
  using ValidatedResult =
      base::expected<ResolvedTarget, mojom::ActionResultPtr>;
  ValidatedResult Validate() const;

  mojom::MouseMoveActionPtr action_;
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_MOUSE_MOVE_TOOL_H_
