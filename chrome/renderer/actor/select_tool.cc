// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/select_tool.h"

#include <cstdint>
#include <optional>

#include "base/check.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_option_element.h"
#include "third_party/blink/public/web/web_select_element.h"

namespace actor {

using blink::WebElement;
using blink::WebNode;
using blink::WebOptionElement;
using blink::WebSelectElement;
using blink::WebString;

SelectTool::SelectTool(mojom::SelectActionPtr action,
                       content::RenderFrame& frame)
    : frame_(frame), action_(std::move(action)) {}

SelectTool::~SelectTool() = default;

void SelectTool::Execute(ToolFinishedCallback callback) {
  if (!Validate()) {
    std::move(callback).Run(false);
    return;
  }

  auto element = GetNodeFromId(frame_.get(), action_->target->get_dom_node_id())
                     .To<WebSelectElement>();
  auto value = WebString::FromUTF8(action_->value);
  element.SetValue(value, /*send_events=*/true);

  // Check if the set value is now the current value in the <select>
  if (element.Value() != value) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

std::string SelectTool::DebugString() const {
  return absl::StrFormat("SelectTool[%s;value(%s)]",
                         ToDebugString(action_->target), action_->value);
}

bool SelectTool::Validate() const {
  if (!frame_->GetWebFrame()->FrameWidget()) {
    return false;
  }

  mojom::ToolTargetPtr& target = action_->target;

  if (target->is_coordinate()) {
    NOTIMPLEMENTED() << "Coordinate-based target is not yet supported.";
    return false;
  }

  int32_t dom_node_id = target->get_dom_node_id();

  WebNode node = GetNodeFromId(frame_.get(), dom_node_id);
  if (node.IsNull()) {
    ACTOR_LOG() << "DOM Node not found for id: " << dom_node_id;
    return false;
  }

  WebSelectElement select = node.DynamicTo<WebSelectElement>();
  if (!select) {
    ACTOR_LOG() << "Target element is not a <select>: " << node;
    return false;
  }

  if (!select.IsEnabled()) {
    ACTOR_LOG() << "Target element is disabled.";
    return false;
  }

  WebString value(WebString::FromUTF8(action_->value));
  for (const auto& e : select.GetListItems()) {
    auto option = e.DynamicTo<WebOptionElement>();
    if (option && option.Value() == value && option.IsEnabled()) {
      return true;
    }
  }

  ACTOR_LOG() << "Requested option [" << action_->value
              << "] is not available in target: " << select;
  return false;
}

}  // namespace actor
