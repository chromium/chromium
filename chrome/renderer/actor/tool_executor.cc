// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/tool_executor.h"

#include <cstdint>
#include <memory>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "chrome/renderer/actor/click_tool.h"
#include "chrome/renderer/actor/drag_and_release_tool.h"
#include "chrome/renderer/actor/journal.h"
#include "chrome/renderer/actor/mouse_move_tool.h"
#include "chrome/renderer/actor/no_op_tool.h"
#include "chrome/renderer/actor/script_tool.h"
#include "chrome/renderer/actor/scroll_tool.h"
#include "chrome/renderer/actor/select_tool.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "chrome/renderer/actor/type_tool.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "ui/gfx/geometry/point_conversions.h"

using blink::WebLocalFrame;
using content::RenderFrame;

namespace actor {

ToolExecutor::ToolExecutor(RenderFrame* frame, Journal& journal)
    : frame_(*frame), journal_(journal) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
}

ToolExecutor::~ToolExecutor() {
  if (completion_callback_) {
    std::move(completion_callback_)
        .Run(MakeResult(mojom::ActionResultCode::kExecutorDestroyed,
                        /*requires_page_stabilization=*/false,
                        "The tool executor was destroyed before invocation "
                        "could complete."));
  }
}

mojom::InitializeToolResultPtr ToolExecutor::InitializeTool(
    mojom::ToolInvocationPtr invocation) {
  is_split_execution_ = true;
  return InitializeToolImpl(std::move(invocation));
}

mojom::InitializeToolResultPtr ToolExecutor::InitializeToolImpl(
    mojom::ToolInvocationPtr invocation) {
  auto init_entry = journal_->CreatePendingAsyncEntry(invocation->task_id,
                                                      "InitializeTool", {});

  // Send the buffer now so the journal shows we received the message. This
  // helps when debugging unresponsive renderers.
  journal_->SendLogBuffer();

  if (tool_) {
    return mojom::InitializeToolResult::NewErrorResult(
        MakeResult(mojom::ActionResultCode::kExecutorBusy));
  }

  CHECK_EQ(phase_, ExecutionPhase::kStart)
      << "InitializeTool called from invalid phase.";

  WebLocalFrame* web_frame = frame_->GetWebFrame();

  // Tool calls should only be routed to local root frames.
  CHECK(!web_frame || web_frame->LocalRoot() == web_frame);

  // Check LocalRoot in case the frame is a subframe.
  if (!web_frame || !web_frame->FrameWidget()) {
    return mojom::InitializeToolResult::NewErrorResult(
        MakeResult(mojom::ActionResultCode::kFrameWentAway));
  }

  switch (invocation->action->which()) {
    case actor::mojom::ToolAction::Tag::kClick: {
      tool_ = std::make_unique<ClickTool>(
          frame_.get(), invocation->task_id, journal_.get(),
          std::move(invocation->action->get_click()),
          std::move(invocation->target),
          std::move(invocation->observed_target));
      break;
    }
    case actor::mojom::ToolAction::Tag::kMouseMove: {
      tool_ = std::make_unique<MouseMoveTool>(
          frame_.get(), invocation->task_id, journal_.get(),
          std::move(invocation->action->get_mouse_move()),
          std::move(invocation->target),
          std::move(invocation->observed_target));
      break;
    }
    case actor::mojom::ToolAction::Tag::kType: {
      tool_ = std::make_unique<TypeTool>(
          frame_.get(), invocation->task_id, journal_.get(),
          std::move(invocation->action->get_type()),
          std::move(invocation->target),
          std::move(invocation->observed_target));
      break;
    }
    case actor::mojom::ToolAction::Tag::kScroll: {
      tool_ = std::make_unique<ScrollTool>(
          frame_.get(), invocation->task_id, journal_.get(),
          std::move(invocation->action->get_scroll()),
          std::move(invocation->target),
          std::move(invocation->observed_target));
      break;
    }
    case actor::mojom::ToolAction::Tag::kSelect: {
      tool_ = std::make_unique<SelectTool>(
          frame_.get(), invocation->task_id, journal_.get(),
          std::move(invocation->action->get_select()),
          std::move(invocation->target),
          std::move(invocation->observed_target));
      break;
    }
    case actor::mojom::ToolAction::Tag::kDragAndRelease: {
      tool_ = std::make_unique<DragAndReleaseTool>(
          frame_.get(), invocation->task_id, journal_.get(),
          std::move(invocation->action->get_drag_and_release()),
          std::move(invocation->target),
          std::move(invocation->observed_target));
      break;
    }
    case actor::mojom::ToolAction::Tag::kScriptTool: {
      // We could consider not waiting for stabilization since the API has an
      // explicit async hook to know when the tool is done. Or having the
      // stabilization only delay until a new frame is produced.
      tool_ = std::make_unique<ScriptTool>(
          frame_.get(), invocation->task_id, journal_.get(),
          std::move(invocation->target), std::move(invocation->observed_target),
          std::move(invocation->action->get_script_tool()));
      break;
    }
    case actor::mojom::ToolAction::Tag::kScrollTo: {
      // This is only used to call `EnsureTargetInView()`.
      tool_ = std::make_unique<NoOpTool>(
          frame_.get(), invocation->task_id, journal_.get(),
          std::move(invocation->target),
          std::move(invocation->observed_target));
      break;
    }
    default:
      NOTREACHED();
  }

  if (tool_->EnsureTargetInView()) {
    performed_scroll_into_view_ = true;
  }

  ValidationResult validation = tool_->Validate();

  if (!IsOk(*validation.result)) {
    // Add the error result with the current state before cleaning up.
    validation.result->execution_end_time = base::TimeTicks::Now();
    validation.result->requires_page_stabilization |=
        performed_scroll_into_view_;
    // Reset tool so that the ToolExecutor can receive new ToolInvocations if we
    // are erroring after validation.
    tool_.reset();
    performed_scroll_into_view_ = false;
    return mojom::InitializeToolResult::NewErrorResult(
        std::move(validation.result));
  }
  std::optional<gfx::Point> point;
  if (validation.target_point.has_value()) {
    point = gfx::ToRoundedPoint(validation.target_point.value());
  }

  phase_ = ExecutionPhase::kInitialized;
  return mojom::InitializeToolResult::NewSuccessPoint(point);
}

void ToolExecutor::ExecuteTool(const actor::TaskId& task_id,
                               ToolExecutorCallback callback) {
  CHECK_EQ(phase_, ExecutionPhase::kInitialized)
      << "ExecuteTool called without successful InitializeTool.";
  phase_ = ExecutionPhase::kExecuting;
  execute_journal_entry_ = journal_->CreatePendingAsyncEntry(
      task_id, "ExecuteTool",
      JournalDetailsBuilder().Add("tool", tool_->DebugString()).Build());
  // Send the buffer now so the journal shows we received the message. This
  // helps when debugging unresponsive renderers.
  journal_->SendLogBuffer();
  CHECK(tool_);
  CHECK_EQ(tool_->task_id(), task_id);
  CHECK(!completion_callback_);
  completion_callback_ = std::move(callback);
  if (is_split_execution_) {
    tool_->MarkAsRevalidation();
    ValidationResult revalidation = tool_->Validate();
    base::UmaHistogramSparse("Actor.Tools.RevalidationResult",
                             std::to_underlying(revalidation.result->code));
    if (!IsOk(*revalidation.result)) {
      ToolFinished(std::move(revalidation.result));
      return;
    }
  }
  tool_->Execute(base::BindOnce(&ToolExecutor::ToolFinished,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ToolExecutor::InvokeTool(mojom::ToolInvocationPtr invocation,
                              ToolExecutorCallback callback) {
  auto invoke_entry =
      journal_->CreatePendingAsyncEntry(invocation->task_id, "InvokeTool", {});
  // Send the buffer now so the journal shows we received the message. This
  // helps when debugging unresponsive renderers.
  journal_->SendLogBuffer();
  actor::TaskId task_id = invocation->task_id;
  mojom::InitializeToolResultPtr result =
      InitializeToolImpl(std::move(invocation));
  if (result->is_error_result()) {
    // The tool failed to initialize because another tool is active. Abort this
    // invocation immediately without disturbing the running tool.
    if (result->get_error_result()->code ==
        mojom::ActionResultCode::kExecutorBusy) {
      std::move(callback).Run(std::move(result->get_error_result()));
      return;
    }
    CHECK(!completion_callback_);
    completion_callback_ = std::move(callback);
    ToolFinished(std::move(result->get_error_result()));
    return;
  }
  // Set after the busy check to avoid corrupting an active split-execution
  // tool.
  is_split_execution_ = false;
  invoke_journal_entry_ = std::move(invoke_entry);
  ExecuteTool(task_id, std::move(callback));
}

void ToolExecutor::CancelTool(const actor::TaskId& task_id) {
  journal_->Log(
      task_id, "ToolExecutor::CancelTool",
      JournalDetailsBuilder().Add("tool_already_finished", !tool_).Build());

  // Send the buffer now so the journal shows we received the message. This
  // helps when debugging unresponsive renderers.
  // TODO(b/467336183): This instance should be removed once this bug is
  // resolved.
  journal_->SendLogBuffer();

  weak_ptr_factory_.InvalidateWeakPtrs();
  if (!tool_) {
    // Benign race condition: the tool has already finished.
    CHECK(!completion_callback_);
    // If the tool is already null, that means it has already finished and we
    // should be at the start phase.
    CHECK_EQ(phase_, ExecutionPhase::kStart);
    return;
  }

  // The browser and renderer should agree on the active tool.
  CHECK_EQ(tool_->task_id(), task_id);

  tool_->Cancel();

  // The result code doesn't matter as it will be ignored by the browser
  // process.
  ToolFinished(MakeResult(mojom::ActionResultCode::kInvokeCanceled));
}

void ToolExecutor::ToolFinished(mojom::ActionResultPtr result) {
  phase_ = ExecutionPhase::kStart;
  execute_journal_entry_.reset();
  invoke_journal_entry_.reset();
  result->execution_end_time = base::TimeTicks::Now();
  result->requires_page_stabilization |= performed_scroll_into_view_;
  // Reset for future ToolInvocations.
  performed_scroll_into_view_ = false;
  tool_.reset();
  // The completion callback could be null if we receive a CancelTool call
  // before ExecuteTool has started.
  if (completion_callback_) {
    std::move(completion_callback_).Run(std::move(result));
  }
}

}  // namespace actor
