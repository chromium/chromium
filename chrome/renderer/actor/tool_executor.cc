// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/tool_executor.h"

#include "base/functional/callback.h"
#include "chrome/common/actor.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

using content::RenderFrame;
using mojo::PendingReceiver;

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
  // TODO(crbug.com/402731599): Implement tools.
  std::move(callback).Run(true);
}

}  // namespace actor
