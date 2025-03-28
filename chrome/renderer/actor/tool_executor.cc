// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/tool_executor.h"

#include <cstdint>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/renderer/actor/click_tool.h"
#include "content/public/renderer/render_frame.h"

using content::RenderFrame;

namespace actor {

ToolExecutor::ToolExecutor(RenderFrame* frame) : frame_(*frame) {
  // TODO(crbug.com/398260855): Currently, this is created only for the main
  // frame but eventually this will have to support all local roots in a page.
  CHECK(frame->IsMainFrame());
  CHECK(!frame->IsInFencedFrameTree());
}

ToolExecutor::~ToolExecutor() = default;

void ToolExecutor::InvokeTool(mojom::ToolInvocationPtr request,
                              ToolExecutorCallback callback) {
  CHECK(!tool_);
  switch (request->action->which()) {
    case actor::mojom::ToolAction::Tag::kClick: {
      // Check the mojom we received is in good shape.
      CHECK(request->action->get_click());
      tool_ = std::make_unique<ClickTool>(
          std::move(request->action->get_click()), frame_);
      break;
    }
  }
  // It's safe to use base::Unretained as tool_ is owned by this object and
  // tool_ has its own weak factory to manage the callback.
  tool_->Execute(base::BindOnce(&ToolExecutor::ToolFinished,
                                base::Unretained(this), std::move(callback)));
}

void ToolExecutor::ToolFinished(ToolExecutorCallback callback,
                                bool tool_status) {
  CHECK(tool_);
  // Release current tool so we can accept a new tool invocation.
  tool_.reset();
  std::move(callback).Run(tool_status);
}

}  // namespace actor
