// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/tool_executor.h"

#include <cstdint>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/renderer/actor/click_tool.h"
#include "chrome/renderer/actor/drag_and_release_tool.h"
#include "chrome/renderer/actor/journal.h"
#include "chrome/renderer/actor/mouse_move_tool.h"
#include "chrome/renderer/actor/scroll_tool.h"
#include "chrome/renderer/actor/select_tool.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "chrome/renderer/actor/type_tool.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"

using blink::WebLocalFrame;
using content::RenderFrame;

namespace actor {

ToolExecutor::ToolExecutor(RenderFrame* frame, Journal& journal)
    : frame_(*frame), journal_(journal) {}

ToolExecutor::~ToolExecutor() = default;

void ToolExecutor::InvokeTool(mojom::ToolInvocationPtr request,
                              ToolExecutorCallback callback) {
  CHECK(!completion_callback_);
  completion_callback_ = std::move(callback);

  // TODO(bokan): Each individual has this condition. Replace in each tool with
  // a CHECK.
  WebLocalFrame* web_frame = frame_->GetWebFrame();
  if (!web_frame || !web_frame->FrameWidget()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ToolExecutor::ToolFinished,
                       weak_ptr_factory_.GetWeakPtr(),
                       MakeResult(mojom::ActionResultCode::kFrameWentAway)));
    return;
  }

  std::unique_ptr<ToolBase> tool;
  switch (request->action->which()) {
    case actor::mojom::ToolAction::Tag::kClick: {
      // Check the mojom we received is in good shape.
      CHECK(request->action->get_click());
      tool = std::make_unique<ClickTool>(
          frame_.get(), request->task_id, journal_.get(),
          std::move(request->action->get_click()));
      break;
    }
    case actor::mojom::ToolAction::Tag::kMouseMove: {
      CHECK(request->action->get_mouse_move());
      tool = std::make_unique<MouseMoveTool>(
          frame_.get(), request->task_id, journal_.get(),
          std::move(request->action->get_mouse_move()));
      break;
    }
    case actor::mojom::ToolAction::Tag::kType: {
      CHECK(request->action->get_type());
      tool = std::make_unique<TypeTool>(frame_.get(), request->task_id,
                                        journal_.get(),
                                        std::move(request->action->get_type()));
      break;
    }
    case actor::mojom::ToolAction::Tag::kScroll: {
      CHECK(request->action->get_scroll());
      tool = std::make_unique<ScrollTool>(
          frame_.get(), request->task_id, journal_.get(),
          std::move(request->action->get_scroll()));
      break;
    }
    case actor::mojom::ToolAction::Tag::kSelect: {
      CHECK(request->action->get_select());
      tool = std::make_unique<SelectTool>(
          frame_.get(), request->task_id, journal_.get(),
          std::move(request->action->get_select()));
      break;
    }
    case actor::mojom::ToolAction::Tag::kDragAndRelease: {
      CHECK(request->action->get_drag_and_release());
      tool = std::make_unique<DragAndReleaseTool>(
          frame_.get(), request->task_id, journal_.get(),
          std::move(request->action->get_drag_and_release()));
      break;
    }
    default:
      NOTREACHED();
  }

  journal_->Log(request->task_id, "Renderer InvokeTool", tool->DebugString());

  page_stability_monitor_ = std::make_unique<PageStabilityMonitor>(*frame_);

  mojom::ActionResultPtr result = tool->Execute();

  page_stability_monitor_->WaitForStable(base::BindOnce(
      &ToolExecutor::ToolFinished, base::Unretained(this), std::move(result)));
}

void ToolExecutor::ToolFinished(mojom::ActionResultPtr result) {
  CHECK(completion_callback_);
  std::move(completion_callback_).Run(std::move(result));
}

}  // namespace actor
