// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_TOOL_EXECUTOR_H_
#define CHROME_RENDERER_ACTOR_TOOL_EXECUTOR_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/renderer/actor/tool_base.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace actor {

class Journal;

// Renderer-side tool executor.
//
// This class is responsible for receiving tool request messages and invoking
// the requested tool in the renderer.
class ToolExecutor {
 public:
  using ToolExecutorCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;
  explicit ToolExecutor(content::RenderFrame* frame, Journal& journal);
  ~ToolExecutor();

  ToolExecutor(const ToolExecutor&) = delete;
  ToolExecutor& operator=(const ToolExecutor&) = delete;

  void InvokeTool(mojom::ToolInvocationPtr request,
                  ToolExecutorCallback callback);

 private:
  void ToolFinished(mojom::ActionResultPtr result);
  void OnCompletion(mojom::ActionResultPtr result);

  bool performed_scroll_into_view_ = false;

  // Raw ref since the executor is owned by the RenderFrameObserver which has
  // the same lifetime as RenderFrame.
  base::raw_ref<content::RenderFrame> frame_;
  std::unique_ptr<ToolBase> tool_;
  base::raw_ref<Journal> journal_;
  ToolExecutorCallback completion_callback_;
  std::unique_ptr<Journal::PendingAsyncEntry> invoke_journal_entry_;
  std::unique_ptr<Journal::PendingAsyncEntry> execute_journal_entry_;

  base::WeakPtrFactory<ToolExecutor> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_TOOL_EXECUTOR_H_
