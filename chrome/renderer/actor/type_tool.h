// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_TYPE_TOOL_H_
#define CHROME_RENDERER_ACTOR_TYPE_TOOL_H_

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/renderer/actor/click_dispatcher.h"
#include "chrome/renderer/actor/key_dispatcher.h"
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
  void Cancel() override;
  std::string DebugString() const override;
  base::TimeDelta ExecutionObservationDelay() const override;
  bool SupportsPaintStability() const override;

 private:
  using ValidatedResult =
      base::expected<ResolvedTarget, mojom::ActionResultPtr>;
  ValidatedResult Validate() const;

  // Return true if input text can be procssed into a series of keypresses.
  bool ProcessInputText(
      std::vector<KeyDispatcher::KeyParams>& key_sequence) const;
  KeyDispatcher::KeyParams GetBackspaceKeyParams() const;
  KeyDispatcher::KeyParams GetEnterKeyParams() const;
  std::optional<KeyDispatcher::KeyParams> GetKeyParamsForChar(char16_t c) const;
  blink::WebInputEventResult CreateAndDispatchKeyEvent(
      blink::WebWidget& widget,
      blink::WebInputEvent::Type type,
      KeyDispatcher::KeyParams key_params);
  mojom::ActionResultPtr SimulateKeyPress(KeyDispatcher::KeyParams params);

  void OnFocusingClickComplete(ToolFinishedCallback callback,
                               mojom::ActionResultPtr click_result);

  mojom::TypeActionPtr action_;

  // Null until validation is completed.
  std::optional<ResolvedTarget> resolved_target_;

  // Used when typing incrementally.
  std::vector<KeyDispatcher::KeyParams> key_sequence_;

  std::optional<ClickDispatcher> click_dispatcher_;
  std::optional<KeyDispatcher> key_dispatcher_;

  base::WeakPtrFactory<TypeTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_TYPE_TOOL_H_
