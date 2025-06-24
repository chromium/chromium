// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_TYPE_TOOL_H_
#define CHROME_RENDERER_ACTOR_TYPE_TOOL_H_

#include <cstdint>
#include <string>
#include <variant>

#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/renderer/actor/tool_base.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/web/web_element.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace actor {

class Journal;

// A tool that simulates typing text into a target DOM node.
class TypeTool : public ToolBase {
 public:
  TypeTool(content::RenderFrame& frame,
           Journal::TaskId task_id,
           Journal& journal,
           mojom::TypeActionPtr action);
  ~TypeTool() override;

  // actor::ToolBase
  mojom::ActionResultPtr Execute() override;
  std::string DebugString() const override;
  base::TimeDelta ExecutionObservationDelay() const override;

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

  struct TargetAndKeys {
    TargetAndKeys(const gfx::PointF& coordinate,
                  std::vector<KeyParams> key_sequence);
    TargetAndKeys(const blink::WebElement& element,
                  std::vector<KeyParams> key_sequence);
    ~TargetAndKeys();
    TargetAndKeys(const TargetAndKeys&);
    TargetAndKeys& operator=(const TargetAndKeys&);
    TargetAndKeys(TargetAndKeys&&);
    TargetAndKeys& operator=(TargetAndKeys&&);

    std::variant<gfx::PointF, blink::WebElement> target;
    std::vector<KeyParams> key_sequence;
  };
  using ValidatedResult = base::expected<TargetAndKeys, mojom::ActionResultPtr>;
  ValidatedResult Validate() const;

  KeyParams GetEnterKeyParams() const;
  std::optional<KeyParams> GetKeyParamsForChar(char c) const;
  blink::WebInputEventResult CreateAndDispatchKeyEvent(
      blink::WebInputEvent::Type type,
      KeyParams key_params);
  mojom::ActionResultPtr SimulateKeyPress(TypeTool::KeyParams params);

  mojom::TypeActionPtr action_;
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_TYPE_TOOL_H_
