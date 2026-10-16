// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_SCROLL_TOOL_H_
#define CHROME_RENDERER_ACTOR_SCROLL_TOOL_H_

#include <cstdint>
#include <optional>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/renderer/actor/tool_base.h"
#include "components/actor/core/task_id.h"
#include "third_party/blink/public/web/web_element.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace actor {

// A tool that can be invoked to perform a scroll over a target.
class ScrollTool : public ToolBase {
 public:
  ScrollTool(content::RenderFrame& frame,
             TaskId task_id,
             Journal& journal,
             mojom::ScrollActionPtr action,
             mojom::ToolTargetPtr target,
             mojom::ObservedToolTargetPtr observed_target);

  ~ScrollTool() override;

  // actor::ToolBase
  ValidationResult Validate() override;
  void Execute(ToolFinishedCallback callback) override;
  std::string DebugString() const override;

 private:
  // Executes a page/document granularity scroll of `scrolling_element`.
  void ExecuteByGranularity(blink::WebElement& scrolling_element,
                            ToolFinishedCallback callback);
  // Called when the scroll started by ExecuteByGranularity() completes.
  void OnScrollByGranularityFinished(
      blink::WebElement scrolling_element,
      gfx::Vector2dF start_offset_css,
      ToolFinishedCallback callback,
      blink::WebElement::ScrollByGranularityResult result);

  struct ScrollerAndDistance {
    blink::WebElement scroller;
    // The offset to scroll by, in physical pixels. Only set for pixel
    // (distance) scrolls; page/document scroll amounts are determined by Blink.
    std::optional<gfx::Vector2dF> scroll_by_offset;
  };

  mojom::ScrollActionPtr action_;
  std::optional<ScrollerAndDistance> validated_scroller_and_distance_;

  // Page/document scrolls may animate and complete asynchronously, after which
  // this tool may have been destroyed.
  base::WeakPtrFactory<ScrollTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_SCROLL_TOOL_H_
