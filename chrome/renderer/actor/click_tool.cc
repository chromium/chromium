// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/click_tool.h"

#include <cstdint>
#include <optional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/common/actor.mojom-shared.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/renderer/actor/click_dispatcher.h"
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

ClickTool::ClickTool(content::RenderFrame& frame,
                     TaskId task_id,
                     Journal& journal,
                     mojom::ClickActionPtr action,
                     mojom::ToolTargetPtr target,
                     mojom::ObservedToolTargetPtr observed_target)
    : ToolBase(frame,
               task_id,
               journal,
               std::move(target),
               std::move(observed_target)),
      action_(std::move(action)) {}

ClickTool::~ClickTool() = default;

void ClickTool::Execute(ToolFinishedCallback callback) {
  CHECK(validated_target_.has_value())
      << "Execute tool was called before validation";
  WebMouseEvent::Button button;
  switch (action_->type) {
    case mojom::ClickType::kLeft: {
      button = WebMouseEvent::Button::kLeft;
      break;
    }
    case mojom::ClickType::kRight: {
      button = WebMouseEvent::Button::kRight;
      break;
    }
  }
  int click_count;
  switch (action_->count) {
    case mojom::ClickCount::kSingle: {
      click_count = 1;
      break;
    }
    case mojom::ClickCount::kDouble: {
      click_count = 2;
      break;
    }
  }

  ResolvedTarget target = validated_target_.value();
  journal_->Log(
      task_id_, "ClickTool::Execute",
      JournalDetailsBuilder().Add("point", target.widget_point).Build());

  // TODO(b/467336183): For debugging; should be removed once this bug is
  // resolved.
  journal_->SendLogBuffer();

  CHECK(!click_dispatcher_);
  click_dispatcher_.emplace(button, click_count, target, *this,
                            std::move(callback));
}

std::string ClickTool::DebugString() const {
  return absl::StrFormat("ClickTool[%s;type(%s);count(%s)]",
                         ToDebugString(target_), base::ToString(action_->type),
                         base::ToString(action_->count));
}

bool ClickTool::SupportsPaintStability() const {
  return true;
}

void ClickTool::Cancel() {
  if (click_dispatcher_) {
    click_dispatcher_->Cancel();
    click_dispatcher_.reset();
  }
}

ValidationResult ClickTool::Validate() {
  CHECK(frame_->GetWebFrame());
  CHECK(frame_->GetWebFrame()->FrameWidget());

  auto resolved_target = ValidateAndResolveTarget();
  if (!resolved_target.has_value()) {
    return ValidationResult(std::move(resolved_target.error()));
  }

  // Perform click validation on the resolved node.
  const WebNode& node = resolved_target->node;
  if (!node.IsNull()) {
    WebFormControlElement form_element =
        node.DynamicTo<WebFormControlElement>();
    if (!form_element.IsNull() && !form_element.IsEnabled()) {
      return ValidationResult(MakeResult(
          mojom::ActionResultCode::kElementDisabled,
          /*requires_page_stabilization=*/false,
          absl::StrFormat("[Element %s]", base::ToString(form_element))));
    }
  }

  validated_target_ = std::move(resolved_target.value());
  return ValidationResult(MakeOkResult(), validated_target_->widget_point);
}

}  // namespace actor
