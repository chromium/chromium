// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_RENDER_INPUT_ROUTER_CLIENT_H_
#define COMPONENTS_INPUT_RENDER_INPUT_ROUTER_CLIENT_H_

#include <optional>

#include "base/component_export.h"
#include "components/input/input_router_client.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/widget/platform_widget.mojom.h"
#include "ui/latency/latency_info.h"

namespace input {

class StylusInterface;

class COMPONENT_EXPORT(INPUT) RenderInputRouterClient {
 public:
  // TODO(b/331420891): Move these methods into RenderInputRouter.
  virtual void IncrementInFlightEventCount() = 0;
  virtual void DecrementInFlightEventCount(
      blink::mojom::InputEventResultSource ack_source) = 0;

  virtual void ForwardWheelEventWithLatencyInfo(
      const blink::WebMouseWheelEvent& wheel_event,
      const ui::LatencyInfo& latency_info) = 0;

  virtual bool IsAutoscrollInProgress() = 0;

  virtual void SetMouseCapture(bool capture) = 0;
  virtual void SetAutoscrollSelectionActiveInMainFrame(
      bool autoscroll_selection) = 0;
  virtual void RequestMouseLock(
      bool from_user_gesture,
      bool unadjusted_movement,
      blink::mojom::WidgetInputHandlerHost::RequestMouseLockCallback
          response) = 0;

  virtual void OnImeCancelComposition() = 0;
  virtual void OnImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::optional<std::vector<gfx::Rect>>& character_bounds,
      const std::optional<std::vector<gfx::Rect>>& line_bounds) = 0;
  virtual StylusInterface* GetStylusInterface() = 0;
  // Initiate stylus handwriting.
  virtual void OnStartStylusWriting() = 0;
  // Update which editable element has focus for stylus writing.
  virtual void UpdateElementFocusForStylusWriting() = 0;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_RENDER_INPUT_ROUTER_CLIENT_H_
