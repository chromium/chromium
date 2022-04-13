// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/check_on_top_worker.h"

#include <vector>

#include "base/logging.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"

namespace autofill_assistant {

CheckOnTopWorker::CheckOnTopWorker(DevtoolsClient* devtools_client)
    : devtools_client_(devtools_client) {}

CheckOnTopWorker::~CheckOnTopWorker() {}

void CheckOnTopWorker::Start(const ElementFinderResult& element,
                             Callback callback) {
  callback_ = std::move(callback);

  // Each containing iframe must be checked to detect the case where an iframe
  // element is covered by another element. The snippets might be run in
  // parallel, if iframes run in different JavaScript contexts.

  JsSnippet js_snippet;
  js_snippet.AddLine("function(element) {");
  AddReturnIfOnTop(&js_snippet, "element",
                   /* on_top= */ "true",
                   /* not_on_top= */ "false",
                   /* not_in_view= */ "false");
  js_snippet.AddLine("}");
  std::string function = js_snippet.ToString();

  pending_result_count_ = element.frame_stack().size() + 1;
  for (const auto& frame : element.frame_stack()) {
    CallFunctionOn(function, frame.node_frame_id, frame.object_id);
  }
  CallFunctionOn(function, element.node_frame_id(), element.object_id());
}

void CheckOnTopWorker::CallFunctionOn(const std::string& function,
                                      const std::string& frame_id,
                                      const std::string& object_id) {
  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgumentObjectId(object_id, &arguments);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(function)
          .SetReturnByValue(true)
          .Build(),
      frame_id,
      base::BindOnce(&CheckOnTopWorker::OnReply,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CheckOnTopWorker::OnReply(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!callback_) {
    // Already answered.
    return;
  }

  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << " Failed JavaScript with status: " << status;
    std::move(callback_).Run(status);
    return;
  }

  bool onTop = false;
  if (!SafeGetBool(result->GetResult(), &onTop)) {
    VLOG(1) << __func__ << " JavaScript function failed to return a boolean.";
    std::move(callback_).Run(UnexpectedErrorStatus(__FILE__, __LINE__));
    return;
  }

  if (!onTop) {
    std::move(callback_).Run(ClientStatus(ELEMENT_NOT_ON_TOP));
    return;
  }

  if (pending_result_count_ == 1) {
    std::move(callback_).Run(OkClientStatus());
    return;
  }

  // Wait for a result from other frames.
  DCHECK_GT(pending_result_count_, 1u);
  pending_result_count_--;
}

}  // namespace autofill_assistant
