// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_TOOL_BASE_H_
#define CHROME_RENDERER_ACTOR_TOOL_BASE_H_

#include <cstdint>

#include "base/functional/callback_forward.h"

namespace actor {
class ToolBase {
 public:
  using ToolFinishedCallback = base::OnceCallback<void(bool)>;
  virtual void Execute(ToolFinishedCallback done_cb) = 0;
  virtual ~ToolBase() = default;
};
}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_TOOL_BASE_H_
