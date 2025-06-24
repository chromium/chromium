// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_TOOL_BASE_H_
#define CHROME_RENDERER_ACTOR_TOOL_BASE_H_

#include <cstdint>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/renderer/actor/journal.h"

namespace content {
class RenderFrame;
}

namespace actor {

class ToolBase {
 public:
  ToolBase(content::RenderFrame& frame,
           Journal::TaskId task_id,
           Journal& journal)
      : frame_(frame), task_id_(task_id), journal_(journal) {}
  virtual ~ToolBase() = default;

  // Executes the tool and returns the result code.
  virtual mojom::ActionResultPtr Execute() = 0;

  // Returns a human readable string representing this tool and its parameters.
  // Used primarily for logging and debugging.
  virtual std::string DebugString() const = 0;

  // The amount of time to wait when observing tool execution before starting to
  // wait for page stability. 0 by default, meaning no delay, but tools can
  // override this on a case-by-case basis when the expected effects of tool use
  // may happen asynchronously outside of the injected events.
  virtual base::TimeDelta ExecutionObservationDelay() const;

 protected:
  // Raw ref since this is owned by ToolExecutor whose lifetime is tied to
  // RenderFrame.
  base::raw_ref<content::RenderFrame> frame_;
  Journal::TaskId task_id_;
  base::raw_ref<Journal> journal_;
};
}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_TOOL_BASE_H_
