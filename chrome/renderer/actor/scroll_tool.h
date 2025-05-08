// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_SCROLL_TOOL_H_
#define CHROME_RENDERER_ACTOR_SCROLL_TOOL_H_

#include <cstdint>

#include "base/memory/raw_ref.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/renderer/actor/tool_base.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace actor {

// A tool that can be invoked to perform a scroll over a target.
class ScrollTool : public ToolBase {
 public:
  ScrollTool(mojom::ScrollActionPtr action, content::RenderFrame& frame);

  ~ScrollTool() override;

  // actor::ToolBase
  void Execute(ToolFinishedCallback callback) override;
  std::string DebugString() const override;

 private:
  // Raw ref since this is owned by ToolExecutor whose lifetime is tied to
  // RenderFrame.
  base::raw_ref<content::RenderFrame> frame_;
  mojom::ScrollActionPtr action_;
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_SCROLL_TOOL_H_
