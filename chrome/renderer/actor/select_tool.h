// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_SELECT_TOOL_H_
#define CHROME_RENDERER_ACTOR_SELECT_TOOL_H_

#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/renderer/actor/tool_base.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_select_element.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace actor {

// A tool that can be invoked to choose an option from a <select> element.
class SelectTool : public ToolBase {
 public:
  SelectTool(mojom::SelectActionPtr action, content::RenderFrame& frame);
  ~SelectTool() override;

  // actor::ToolBase
  void Execute(ToolFinishedCallback callback) override;
  std::string DebugString() const override;

 private:
  struct TargetAndValue {
    blink::WebSelectElement select;
    blink::WebString option_value;
  };
  using ValidatedResult =
      base::expected<TargetAndValue, mojom::ActionResultPtr>;
  ValidatedResult Validate() const;

  // Raw ref since this is owned by ToolExecutor whose lifetime is tied to
  // RenderFrame.
  base::raw_ref<content::RenderFrame> frame_;
  mojom::SelectActionPtr action_;
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_SELECT_TOOL_H_
