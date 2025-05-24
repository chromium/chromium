// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_TOOL_BASE_H_
#define CHROME_RENDERER_ACTOR_TOOL_BASE_H_

#include <cstdint>
#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {
class ToolBase {
 public:
  using ToolFinishedCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;
  virtual ~ToolBase() = default;

  // Executes the tool. `callback` is invoked with the tool result.
  virtual void Execute(ToolFinishedCallback callback) = 0;

  // Returns a human readable string representing this tool and its parameters.
  // Used primarily for logging and debugging.
  virtual std::string DebugString() const = 0;
};
}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_TOOL_BASE_H_
