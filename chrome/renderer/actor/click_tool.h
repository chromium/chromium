// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_CLICK_TOOL_H_
#define CHROME_RENDERER_ACTOR_CLICK_TOOL_H_

#include <optional>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/renderer/actor/click_dispatcher.h"
#include "chrome/renderer/actor/tool_base.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace actor {

// A tool that can be invoked to perform a click on a target.
class ClickTool : public ToolBase {
 public:
  ClickTool(content::RenderFrame& frame,
            TaskId task_id,
            Journal& journal,
            mojom::ClickActionPtr action,
            mojom::ToolTargetPtr target,
            mojom::ObservedToolTargetPtr observed_target);
  ~ClickTool() override;

  // actor::ToolBase
  ValidationResult Validate() override;
  void Execute(ToolFinishedCallback callback) override;
  std::string DebugString() const override;
  bool SupportsPaintStability() const override;
  void Cancel() override;

 private:
  mojom::ClickActionPtr action_;
  std::optional<ClickDispatcher> click_dispatcher_;
  std::optional<ResolvedTarget> validated_target_;

  base::WeakPtrFactory<ClickTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_CLICK_TOOL_H_
