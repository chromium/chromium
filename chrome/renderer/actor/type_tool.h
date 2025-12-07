// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_TYPE_TOOL_H_
#define CHROME_RENDERER_ACTOR_TYPE_TOOL_H_

#include <cstdint>
#include <string>
#include <variant>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/renderer/actor/tool_base.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/web/web_element.h"

namespace blink {
class WebWidget;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace actor {

class Journal;

// A tool that simulates typing text into a target DOM node.
class TypeTool : public ToolBase {
 public:
  TypeTool(content::RenderFrame& frame,
           TaskId task_id,
           Journal& journal,
           mojom::TypeActionPtr action,
           mojom::ToolTargetPtr target,
           mojom::ObservedToolTargetPtr observed_target);
  ~TypeTool() override;

  // actor::ToolBase
  void Execute(ToolFinishedCallback callback) override;
  std::string DebugString() const override;
  base::TimeDelta ExecutionObservationDelay() const override;
  bool SupportsPaintStability() const override;

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
    char16_t text = u'\0';
    // Text without modifiers
    char16_t unmodified_text = u'\0';
  };

  using ValidatedResult =
      base::expected<ResolvedTarget, mojom::ActionResultPtr>;
  ValidatedResult Validate() const;

  // Return true if input text can be procssed into a series of keypresses.
  bool ProcessInputText(std::vector<KeyParams>& key_sequence) const;
  KeyParams GetBackspaceKeyParams() const;
  KeyParams GetEnterKeyParams() const;
  std::optional<KeyParams> GetKeyParamsForChar(char16_t c) const;
  blink::WebInputEventResult CreateAndDispatchKeyEvent(
      blink::WebWidget& widget,
      blink::WebInputEvent::Type type,
      KeyParams key_params);
  mojom::ActionResultPtr SimulateKeyPress(TypeTool::KeyParams params);

  void OnFocusingClickComplete(ToolFinishedCallback callback,
                               mojom::ActionResultPtr click_result);
  void ContinueIncrementalTyping(ToolFinishedCallback callback);

  mojom::TypeActionPtr action_;

  // Null until validation is completed.
  std::optional<ResolvedTarget> resolved_target_;

  // Used when typing incrementally.
  std::vector<KeyParams> key_sequence_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  bool is_key_down_ = false;
  size_t current_key_ = 0;

  base::WeakPtrFactory<TypeTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_TYPE_TOOL_H_
