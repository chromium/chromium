// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_NO_OP_TOOL_H_
#define CHROME_RENDERER_ACTOR_NO_OP_TOOL_H_

#include "base/types/expected.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/renderer/actor/tool_base.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace actor {

// A tool that does nothing, useful for validation and tool side-effects (e.g.
// `EnsureToolInView()`).
class NoOpTool : public ToolBase {
 public:
  NoOpTool(content::RenderFrame& frame,
           TaskId task_id,
           Journal& journal,
           mojom::ToolTargetPtr target,
           mojom::ObservedToolTargetPtr observed_target);

  ~NoOpTool() override;

  // actor::ToolBase
  void Execute(ToolFinishedCallback callback) override;
  std::string DebugString() const override;

 private:
  using ValidatedResult = base::expected<void, mojom::ActionResultPtr>;
  ValidatedResult Validate() const;
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_NO_OP_TOOL_H_
