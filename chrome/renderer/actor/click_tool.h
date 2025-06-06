// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_CLICK_TOOL_H_
#define CHROME_RENDERER_ACTOR_CLICK_TOOL_H_

#include <cstdint>

#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/renderer/actor/tool_base.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace gfx {
class PointF;
}  // namespace gfx

namespace actor {

// A tool that can be invoked to perform a click on a target.
class ClickTool : public ToolBase {
 public:
  ClickTool(content::RenderFrame& frame,
            Journal::TaskId task_id,
            Journal& journal,
            mojom::ClickActionPtr action);
  ~ClickTool() override;

  // actor::ToolBase
  mojom::ActionResultPtr Execute() override;
  std::string DebugString() const override;

 private:
  using ValidatedResult = base::expected<gfx::PointF, mojom::ActionResultPtr>;
  ValidatedResult Validate() const;

  mojom::ClickActionPtr action_;
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_CLICK_TOOL_H_
