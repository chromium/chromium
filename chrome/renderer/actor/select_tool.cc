// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/select_tool.h"

#include <cstdint>
#include <optional>

#include "base/check.h"
#include "base/notimplemented.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_option_element.h"
#include "third_party/blink/public/web/web_select_element.h"
#include "third_party/blink/public/web/web_view.h"

namespace actor {

using blink::WebElement;
using blink::WebNode;
using blink::WebOptionElement;
using blink::WebSelectElement;
using blink::WebString;

SelectTool::SelectTool(content::RenderFrame& frame,
                       TaskId task_id,
                       Journal& journal,
                       mojom::SelectActionPtr action,
                       mojom::ToolTargetPtr target,
                       mojom::ObservedToolTargetPtr observed_target)
    : ToolBase(frame,
               task_id,
               journal,
               std::move(target),
               std::move(observed_target)),
      action_(std::move(action)) {}

SelectTool::~SelectTool() = default;

void SelectTool::Execute(ToolFinishedCallback callback) {
  ValidatedResult validated_result = Validate();
  if (!validated_result.has_value()) {
    std::move(callback).Run(std::move(validated_result.error()));
    return;
  }

  WebSelectElement select = validated_result.value().select;
  WebString value = validated_result.value().option_value;
  select.SetValue(value, /*send_events=*/true);

  frame_->GetWebFrame()->View()->CancelPagePopup();

  std::move(callback).Run(MakeOkResult());
}

std::string SelectTool::DebugString() const {
  return absl::StrFormat("SelectTool[%s;value(%s)]", ToDebugString(target_),
                         action_->value);
}

SelectTool::ValidatedResult SelectTool::Validate() const {
  CHECK(frame_->GetWebFrame());
  CHECK(frame_->GetWebFrame()->FrameWidget());

  if (target_->is_coordinate_dip()) {
    NOTIMPLEMENTED() << "Coordinate-based target is not yet supported.";
    return base::unexpected(MakeErrorResult());
  }

  auto resolved_target = ValidateAndResolveTarget();
  if (!resolved_target.has_value()) {
    return base::unexpected(std::move(resolved_target.error()));
  }

  // Perform select validation on the resolved node.
  const WebNode& node = resolved_target->node;
  WebSelectElement select = node.DynamicTo<WebSelectElement>();
  if (!select) {
    return base::unexpected(
        MakeResult(mojom::ActionResultCode::kSelectInvalidElement,
                   /*requires_page_stabilization=*/false,
                   absl::StrFormat("Element [%s]", base::ToString(node))));
  }

  if (!select.IsEnabled()) {
    return base::unexpected(
        MakeResult(mojom::ActionResultCode::kElementDisabled,
                   /*requires_page_stabilization=*/false,
                   absl::StrFormat("Element [%s]", base::ToString(select))));
  }

  WebString value(WebString::FromUTF8(action_->value));
  for (const auto& e : select.GetListItems()) {
    auto option = e.DynamicTo<WebOptionElement>();
    if (option && option.Value() == value) {
      if (!option.IsEnabled()) {
        return base::unexpected(MakeResult(
            mojom::ActionResultCode::kSelectOptionDisabled,
            /*requires_page_stabilization=*/false,
            absl::StrFormat("SelectElement[%s] OptionElement [%s]",
                            base::ToString(select), base::ToString(option))));
      }
      return TargetAndValue{select, value};
    }
  }

  return base::unexpected(
      MakeResult(mojom::ActionResultCode::kSelectNoSuchOption,
                 /*requires_page_stabilization=*/false,
                 absl::StrFormat("SelectElement[%s]", base::ToString(select))));
}

}  // namespace actor
