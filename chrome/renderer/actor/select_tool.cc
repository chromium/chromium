// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/select_tool.h"

#include <cstdint>
#include <optional>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/strings/to_string.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "components/actor/core/actor_logging.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_option_element.h"
#include "third_party/blink/public/web/web_select_element.h"
#include "third_party/blink/public/web/web_view.h"

namespace actor {

namespace {

// When this flag is enabled, return ArgumentsInvalid if the match_text is
// empty. This is a kill-switch to a change to previous behavior.
BASE_FEATURE(kActorSelectToolAllowEmptyTextMatch,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

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
  CHECK(validated_target_and_option_.has_value())
      << "Execute tool was called before validation";
  WebSelectElement select = validated_target_and_option_.value().select;
  WebOptionElement option = validated_target_and_option_.value().option;

  // Use a weak pointer to check if `this` is still valid after SelectOption()
  // returns, as it synchronously dispatches DOM events that might destroy the
  // owning frame and this tool.
  base::WeakPtr<SelectTool> weak_this = weak_ptr_factory_.GetWeakPtr();
  select.SelectOption(&option, /*send_events=*/true);

  if (!weak_this) {
    // If the tool was destroyed, its owner (ToolExecutor) is also likely being
    // destroyed (e.g. due to frame detachment). Since the callback is bound to
    // a weak pointer of the ToolExecutor, running it would be a no-op.
    return;
  }

  frame_->GetWebFrame()->View()->CancelPagePopup();

  std::move(callback).Run(MakeOkResult());
}

std::string SelectTool::DebugString() const {
  return absl::StrFormat("SelectTool[%s;match_text(%s);match_mode(%s)]",
                         ToDebugString(target_), action_->match_text,
                         base::ToString(action_->match_mode));
}

ValidationResult SelectTool::Validate() {
  CHECK(frame_->GetWebFrame());
  CHECK(frame_->GetWebFrame()->FrameWidget());

  if (action_->match_text.empty() &&
      (action_->match_mode == mojom::SelectAction::OptionMatchMode::kValue ||
       base::FeatureList::IsEnabled(kActorSelectToolAllowEmptyTextMatch))) {
    return ValidationResult(
        MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                   /*requires_page_stabilization=*/false, "Empty match_text"));
  }

  if (target_->is_coordinate_dip()) {
    static constexpr std::string_view kErrorMessage =
        "Coordinate-based target is not yet supported.";
    NOTIMPLEMENTED() << kErrorMessage;
    return ValidationResult(MakeResult(mojom::ActionResultCode::kNotImplemented,
                                       /*requires_page_stabilization=*/false,
                                       kErrorMessage));
  }

  auto resolved_target = ValidateAndResolveTarget();
  if (!resolved_target.has_value()) {
    return ValidationResult(std::move(resolved_target.error()));
  }

  // Perform select validation on the resolved node.
  const WebNode& node = resolved_target->node;
  WebSelectElement select = node.DynamicTo<WebSelectElement>();
  if (!select) {
    return ValidationResult(
        MakeResult(mojom::ActionResultCode::kSelectInvalidElement,
                   /*requires_page_stabilization=*/false,
                   absl::StrFormat("Element [%s]", base::ToString(node))));
  }

  if (!select.IsEnabled()) {
    return ValidationResult(
        MakeResult(mojom::ActionResultCode::kElementDisabled,
                   /*requires_page_stabilization=*/false,
                   absl::StrFormat("Element [%s]", base::ToString(select))));
  }

  WebString match_text(WebString::FromUtf8(action_->match_text));
  for (const auto& e : select.GetListItems()) {
    auto option = e.DynamicTo<WebOptionElement>();
    if (!option) {
      continue;
    }
    bool matches = false;
    switch (action_->match_mode) {
      case mojom::SelectAction::OptionMatchMode::kValue:
        matches = option.Value() == match_text;
        break;
      case mojom::SelectAction::OptionMatchMode::kText: {
        // Match AnnotatedPageContent's select_options extraction, which uses
        // the option's inner text and falls back to its display label when
        // empty.
        WebString option_text = option.GetInnerText();
        if (option_text.IsEmpty()) {
          option_text = option.GetText();
        }
        matches = option_text == match_text;
        break;
      }
    }
    if (matches) {
      if (!option.IsEnabled()) {
        return ValidationResult(MakeResult(
            mojom::ActionResultCode::kSelectOptionDisabled,
            /*requires_page_stabilization=*/false,
            absl::StrFormat("SelectElement[%s] OptionElement [%s]",
                            base::ToString(select), base::ToString(option))));
      }
      validated_target_and_option_.emplace(TargetAndOption{select, option});
      return ValidationResult(MakeOkResult());
    }
  }
  return ValidationResult(
      MakeResult(mojom::ActionResultCode::kSelectNoSuchOption,
                 /*requires_page_stabilization=*/false,
                 absl::StrFormat("SelectElement[%s]", base::ToString(select))));
}

}  // namespace actor
