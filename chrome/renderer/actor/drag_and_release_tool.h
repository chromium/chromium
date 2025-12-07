// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_DRAG_AND_RELEASE_TOOL_H_
#define CHROME_RENDERER_ACTOR_DRAG_AND_RELEASE_TOOL_H_

#include <cstdint>

#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/renderer/actor/tool_base.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace blink {
class WebWidget;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace gfx {
class PointF;
}  // namespace gfx

namespace actor {

// A tool that can be invoked to perform a drag and release over a target.
class DragAndReleaseTool : public ToolBase {
 public:
  DragAndReleaseTool(content::RenderFrame& frame,
                     TaskId task_id,
                     Journal& journal,
                     mojom::DragAndReleaseActionPtr action,
                     mojom::ToolTargetPtr target,
                     mojom::ObservedToolTargetPtr observed_target);

  ~DragAndReleaseTool() override;

  // actor::ToolBase
  void Execute(ToolFinishedCallback callback) override;
  std::string DebugString() const override;

 private:
  struct DragParams {
    ResolvedTarget from;
    ResolvedTarget to;
  };
  using ValidatedResult = base::expected<DragParams, mojom::ActionResultPtr>;
  ValidatedResult Validate() const;

  void ProcessDrag(ResolvedTarget from,
                   ResolvedTarget to,
                   ToolFinishedCallback callback);
  void ProcessRelease(ResolvedTarget to, ToolFinishedCallback callback);

  bool InjectMouseEvent(blink::WebWidget& widget,
                        gfx::PointF& position_in_widget,
                        blink::WebInputEvent::Type type,
                        blink::WebMouseEvent::Button button);

  mojom::DragAndReleaseActionPtr action_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<DragAndReleaseTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_DRAG_AND_RELEASE_TOOL_H_
