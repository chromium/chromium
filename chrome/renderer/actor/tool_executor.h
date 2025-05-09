// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_TOOL_EXECUTOR_H_
#define CHROME_RENDERER_ACTOR_TOOL_EXECUTOR_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/stack_allocated.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/renderer/actor/tool_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace actor {

// Renderer-side tool executor.
//
// This class is responsible for receiving tool request messages and invoking
// the requested tool in the renderer.
//
// WARNING: This class is stack allocated but is written in a way that implies
// that tools can be asynchronously executed. In practice the tools are
// synchronous, and there's a lot of re-entrancy.
class ToolExecutor {
  STACK_ALLOCATED();

 public:
  using ToolExecutorCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;
  explicit ToolExecutor(content::RenderFrame* frame);
  ~ToolExecutor();

  ToolExecutor(const ToolExecutor&) = delete;
  ToolExecutor& operator=(const ToolExecutor&) = delete;

  void InvokeTool(mojom::ToolInvocationPtr request,
                  ToolExecutorCallback callback);

 private:
  void ToolFinished(ToolExecutorCallback callback,
                    mojom::ActionResultPtr result);

  // Raw ref since the executor is currently only stack allocated by the
  // render frame so it must be outlived.
  base::raw_ref<content::RenderFrame> frame_;
  std::unique_ptr<ToolBase> tool_;
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_TOOL_EXECUTOR_H_
