// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/element_rect_getter.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/rectf.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"

namespace autofill_assistant {
namespace {

const char* const kGetBoundingClientRectAsList =
    R"(function(node, addWindowScroll) {
    const offsetX = addWindowScroll ? window.scrollX : 0;
    const offsetY = addWindowScroll ? window.scrollY : 0;
    const r = node.getBoundingClientRect();
    return [offsetX + r.left,
            offsetY + r.top,
            offsetX + r.right,
            offsetY + r.bottom];
  })";

}  // namespace

ElementRectGetter::ElementRectGetter(DevtoolsClient* devtools_client)
    : devtools_client_(devtools_client), weak_ptr_factory_(this) {}

ElementRectGetter::~ElementRectGetter() = default;

void ElementRectGetter::Start(std::unique_ptr<ElementFinderResult> element,
                              ElementRectCallback callback) {
  GetBoundingClientRect(std::move(element), 0, RectF(), std::move(callback));
}

void ElementRectGetter::GetBoundingClientRect(
    std::unique_ptr<ElementFinderResult> element,
    size_t index,
    const RectF& stacked_rect,
    ElementRectCallback callback) {
  std::string object_id;
  std::string node_frame_id;
  if (index < element->frame_stack().size()) {
    object_id = element->frame_stack()[index].object_id;
    node_frame_id = element->frame_stack()[index].node_frame_id;
  } else {
    object_id = element->object_id();
    node_frame_id = element->node_frame_id();
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgumentObjectId(object_id, &arguments);
  // Only the main frame should add window scrolling. Do not add scrolling from
  // iFrame, those are already accounted for in the client bounding rect.
  bool addWindowScroll = index == 0;
  AddRuntimeCallArgument(addWindowScroll, &arguments);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kGetBoundingClientRectAsList))
          .SetReturnByValue(true)
          .Build(),
      node_frame_id,
      base::BindOnce(&ElementRectGetter::OnGetClientRectResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(element), index, stacked_rect));
}

void ElementRectGetter::OnGetClientRectResult(
    ElementRectCallback callback,
    std::unique_ptr<ElementFinderResult> element,
    size_t index,
    const RectF& stacked_rect,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok() || !result->GetResult()->HasValue() ||
      !result->GetResult()->GetValue()->is_list() ||
      result->GetResult()->GetValue()->GetListDeprecated().size() != 4u) {
    VLOG(2) << __func__ << " Failed to get element rect: " << status;
    std::move(callback).Run(
        JavaScriptErrorStatus(reply_status, __FILE__, __LINE__, nullptr),
        RectF());
    return;
  }

  const auto& list = result->GetResult()->GetValue()->GetListDeprecated();
  // Value::GetDouble() is safe to call without checking the value type; it'll
  // return 0.0 if the value has the wrong type.

  RectF rect;
  rect.left = static_cast<float>(list[0].GetDouble());
  rect.top = static_cast<float>(list[1].GetDouble());
  rect.right = static_cast<float>(list[2].GetDouble());
  rect.bottom = static_cast<float>(list[3].GetDouble());

  if (index > 0) {
    rect.left = std::min(stacked_rect.right, stacked_rect.left + rect.left);
    rect.top = std::min(stacked_rect.bottom, stacked_rect.top + rect.top);
    rect.right = std::min(stacked_rect.right, stacked_rect.left + rect.right);
    rect.bottom = std::min(stacked_rect.bottom, stacked_rect.top + rect.bottom);
  }

  if (index >= element->frame_stack().size()) {
    std::move(callback).Run(OkClientStatus(), rect);
    return;
  }

  GetBoundingClientRect(std::move(element), index + 1, rect,
                        std::move(callback));
}

}  // namespace autofill_assistant
