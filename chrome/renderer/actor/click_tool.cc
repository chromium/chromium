// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/click_tool.h"

#include <cstdint>
#include <optional>

#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/latency/latency_info.h"

namespace actor {

using ::blink::WebCoalescedInputEvent;
using ::blink::WebFormControlElement;
using ::blink::WebFrameWidget;
using ::blink::WebInputEvent;
using ::blink::WebInputEventResult;
using ::blink::WebMouseEvent;
using ::blink::WebNode;

ClickTool::ClickTool(mojom::ClickActionPtr action, content::RenderFrame& frame)
    : frame_(frame), action_(std::move(action)) {}

ClickTool::~ClickTool() = default;

void ClickTool::Execute(ToolFinishedCallback callback) {
  ValidatedResult validated_result = Validate();
  if (!validated_result.has_value()) {
    std::move(callback).Run(std::move(validated_result.error()));
    return;
  }

  gfx::PointF click_point = validated_result.value();

  WebMouseEvent::Button button;
  switch (action_->type) {
    case mojom::ClickAction::Type::kLeft: {
      button = WebMouseEvent::Button::kLeft;
      break;
    }
    case mojom::ClickAction::Type::kRight: {
      button = WebMouseEvent::Button::kRight;
      break;
    }
  }
  int click_count;
  switch (action_->count) {
    case mojom::ClickAction::Count::kSingle: {
      click_count = 1;
      break;
    }
    case mojom::ClickAction::Count::kDouble: {
      click_count = 2;
      break;
    }
  }

  mojom::ActionResultPtr result = CreateAndDispatchClick(
      button, click_count, click_point, frame_->GetWebFrame()->FrameWidget());
  std::move(callback).Run(std::move(result));
}

std::string ClickTool::DebugString() const {
  return absl::StrFormat(
      "ClickTool[%s;type(%s);count(%s)]", ToDebugString(action_->target),
      base::ToString(action_->type), base::ToString(action_->count));
}

ClickTool::ValidatedResult ClickTool::Validate() const {
  if (!frame_->GetWebFrame()->FrameWidget()) {
    return base::unexpected(
        MakeResult(mojom::ActionResultCode::kFrameWentAway));
  }

  if (action_->target->is_coordinate()) {
    gfx::PointF click_point(action_->target->get_coordinate());

    if (!IsPointWithinViewport(click_point, frame_.get())) {
      return base::unexpected(
          MakeResult(mojom::ActionResultCode::kCoordinatesOutOfBounds));
    }

    return click_point;
  }

  int32_t dom_node_id = action_->target->get_dom_node_id();
  WebNode node = GetNodeFromId(frame_.get(), dom_node_id);
  if (node.IsNull()) {
    return base::unexpected(
        MakeResult(mojom::ActionResultCode::kInvalidDomNodeId));
  }

  WebFormControlElement form_element = node.DynamicTo<WebFormControlElement>();
  if (!form_element.IsNull() && !form_element.IsEnabled()) {
    return base::unexpected(MakeResult(
        mojom::ActionResultCode::kElementDisabled,
        absl::StrFormat("[Element %s]", base::ToString(form_element))));
  }

  std::optional<gfx::PointF> click_point = InteractionPointFromWebNode(node);
  if (!click_point.has_value()) {
    return base::unexpected(
        MakeResult(mojom::ActionResultCode::kElementOffscreen,
                   absl::StrFormat("[Element %s]", base::ToString(node))));
  }

  return *click_point;
}

}  // namespace actor
