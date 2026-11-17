// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_EXECUTE_JS_TOOL_H_
#define CHROME_RENDERER_ACTOR_EXECUTE_JS_TOOL_H_

#include <cstdint>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/renderer/actor/tool_base.h"

namespace content {
class RenderFrame;
}

namespace actor {

// A tool that executes a dynamic JavaScript code string in the target frame,
// in the actor's isolated world.
class ExecuteJsTool : public ToolBase {
 public:
  ExecuteJsTool(content::RenderFrame& frame,
                TaskId task_id,
                Journal& journal,
                mojom::ToolTargetPtr target,
                mojom::ObservedToolTargetPtr observed_target,
                mojom::ExecuteJsActionPtr action);

  ~ExecuteJsTool() override;

  // ToolBase
  ValidationResult Validate() override;
  void Execute(ToolFinishedCallback callback) override;
  void Cancel() override;
  std::string DebugString() const override;

 private:
  void OnPromiseTimeout();

  mojom::ExecuteJsActionPtr action_;

  // While waiting for a promise the script returned, the key of the pending
  // wait in the registry the promise reactions look it up in. 0 otherwise.
  uint32_t pending_promise_id_ = 0;
  base::OneShotTimer promise_timeout_timer_;

  base::WeakPtrFactory<ExecuteJsTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_EXECUTE_JS_TOOL_H_
