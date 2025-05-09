// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/tool_executor.h"

#include <cstdint>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/renderer/actor/click_tool.h"
#include "chrome/renderer/actor/drag_and_release_tool.h"
#include "chrome/renderer/actor/mouse_move_tool.h"
#include "chrome/renderer/actor/scroll_tool.h"
#include "chrome/renderer/actor/select_tool.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "chrome/renderer/actor/type_tool.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_node.h"

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
          std::move(request->action->get_click()), frame_.get());
      break;
    }
    case actor::mojom::ToolAction::Tag::kMouseMove: {
      CHECK(request->action->get_mouse_move());
      tool_ = std::make_unique<MouseMoveTool>(
          std::move(request->action->get_mouse_move()), frame_.get());
      break;
    }
    case actor::mojom::ToolAction::Tag::kType: {
      CHECK(request->action->get_type());
      tool_ = std::make_unique<TypeTool>(std::move(request->action->get_type()),
                                         frame_.get());
      break;
    }
    case actor::mojom::ToolAction::Tag::kScroll: {
      CHECK(request->action->get_scroll());
      tool_ = std::make_unique<ScrollTool>(
          std::move(request->action->get_scroll()), frame_.get());
      break;
    }
    case actor::mojom::ToolAction::Tag::kSelect: {
      CHECK(request->action->get_select());
      tool_ = std::make_unique<SelectTool>(
          std::move(request->action->get_select()), frame_.get());
      break;
    }
    case actor::mojom::ToolAction::Tag::kDragAndRelease: {
      CHECK(request->action->get_drag_and_release());
      tool_ = std::make_unique<DragAndReleaseTool>(
          std::move(request->action->get_drag_and_release()), frame_.get());
      break;
    }
  }

  ACTOR_LOG() << "Renderer InvokeTool: " << tool_->DebugString();

  // It's safe to use base::Unretained as tool_ is owned by this object and
  // tool_ has its own weak factory to manage the callback.
  tool_->Execute(base::BindOnce(&ToolExecutor::ToolFinished,
                                base::Unretained(this), std::move(callback)));
}

void ToolExecutor::ToolFinished(ToolExecutorCallback callback,
                                mojom::ActionResultPtr result) {
  CHECK(tool_);
  // Release current tool so we can accept a new tool invocation.
  tool_.reset();
  std::move(callback).Run(std::move(result));
}

}  // namespace actor
