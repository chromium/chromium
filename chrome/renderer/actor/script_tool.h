// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_SCRIPT_TOOL_H_
#define CHROME_RENDERER_ACTOR_SCRIPT_TOOL_H_

#include <cstdint>

#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/renderer/actor/tool_base.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace actor {

// A tool that can be invoked to invoke a script tool in the `frame`'s Document.
class ScriptTool : public ToolBase {
 public:
  ScriptTool(content::RenderFrame& frame,
             TaskId task_id,
             Journal& journal,
             mojom::ToolTargetPtr target,
             mojom::ObservedToolTargetPtr observed_target,
             mojom::ScriptToolActionPtr action);

  ~ScriptTool() override;

  // actor::ToolBase
  void Execute(ToolFinishedCallback callback) override;
  std::string DebugString() const override;

 private:
  mojom::ScriptToolActionPtr action_;
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_SCRIPT_TOOL_H_
