// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_TYPE_TOOL_H_
#define CHROME_RENDERER_ACTOR_TYPE_TOOL_H_

#include <cstdint>
#include <string>

#include "base/memory/raw_ref.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/renderer/actor/tool_base.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace actor {

// A tool that simulates typing text into a target DOM node.
class TypeTool : public ToolBase {
 public:
  TypeTool(mojom::TypeActionPtr action, content::RenderFrame& frame);
  ~TypeTool() override;

  // actor::ToolBase
  void Execute(ToolFinishedCallback callback) override;
  std::string DebugString() const override;

 private:
  // Structure to hold all necessary parameters for generating keyboard events
  // for a single character or key press.
  struct KeyParams {
    KeyParams();
    ~KeyParams();
    KeyParams(const KeyParams& other);
    int windows_key_code;
    int native_key_code;
    // Physical key identifier string
    std::string dom_code;
    // Character produced, considering modifiers
    std::string dom_key;
    int modifiers = blink::WebInputEvent::kNoModifiers;
    // Text character for kChar event
    char text = '\0';
    // Text without modifiers
    char unmodified_text = '\0';
  };

  KeyParams GetEnterKeyParams();
  std::optional<KeyParams> GetKeyParamsForChar(char c);
  blink::WebInputEventResult CreateAndDispatchKeyEvent(
      blink::WebInputEvent::Type type,
      KeyParams key_params);
  bool SimulateKeyPress(TypeTool::KeyParams params);

  // Raw ref since this is owned by ToolExecutor whose lifetime is tied to
  // RenderFrame.
  base::raw_ref<content::RenderFrame> frame_;
  mojom::TypeActionPtr action_;
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_TYPE_TOOL_H_
