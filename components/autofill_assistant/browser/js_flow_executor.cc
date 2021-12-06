// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/js_flow_executor.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"

namespace {

// Initializes a |globalFlowState| variable on first run, and renews the
// promises that will let JS flows request native actions.
constexpr char kRunNativeAction[] = R"(
  if (typeof globalFlowState === 'undefined') {
    globalFlowState = {}
  }
    new Promise((fulfill, reject) => {
      globalFlowState.runNativeAction = fulfill
      globalFlowState.endNativeAction = reject
    })
)";

constexpr char kArrayGetNthElement[] = "function(index) { return this[index] }";
constexpr char kFulfillActionPromise[] = R"(
  function(status, result) {
    this([status, result])
  }
)";

constexpr char kMainFrame[] = "";

}  // namespace

namespace autofill_assistant {

JsFlowExecutor::JsFlowExecutor(content::WebContents* web_contents,
                               Delegate* delegate)
    : delegate_(delegate),
      devtools_client_(std::make_unique<DevtoolsClient>(
          content::DevToolsAgentHost::GetOrCreateFor(web_contents))) {}

JsFlowExecutor::~JsFlowExecutor() = default;

void JsFlowExecutor::Start(
    const std::string& js_flow,
    base::OnceCallback<void(const ClientStatus&, std::unique_ptr<base::Value>)>
        callback) {
  if (callback_) {
    LOG(ERROR) << "Invoked " << __func__ << " while already running";
    std::move(callback).Run(ClientStatus(INVALID_ACTION), nullptr);
    return;
  }

  js_flow_ = std::make_unique<std::string>(js_flow);
  callback_ = std::move(callback);
  if (isolated_world_context_id_ == -1) {
    devtools_client_->GetPage()->GetFrameTree(
        kMainFrame, base::BindOnce(&JsFlowExecutor::OnGetFrameTree,
                                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    InternalStart();
  }
}

void JsFlowExecutor::OnGetFrameTree(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<page::GetFrameTreeResult> result) {
  if (!result) {
    LOG(ERROR) << "Failed to retrieve frame tree";
    std::move(callback_).Run(
        JavaScriptErrorStatus(reply_status, __FILE__, __LINE__, nullptr),
        nullptr);
    return;
  }

  devtools_client_->GetPage()->CreateIsolatedWorld(
      page::CreateIsolatedWorldParams::Builder()
          .SetFrameId(result->GetFrameTree()->GetFrame()->GetId())
          .Build(),
      kMainFrame,
      base::BindOnce(&JsFlowExecutor::IsolatedWorldCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void JsFlowExecutor::IsolatedWorldCreated(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<page::CreateIsolatedWorldResult> result) {
  if (!result) {
    LOG(ERROR) << "Failed to create isolated world";
    std::move(callback_).Run(
        JavaScriptErrorStatus(reply_status, __FILE__, __LINE__, nullptr),
        nullptr);
    return;
  }

  isolated_world_context_id_ = result->GetExecutionContextId();
  InternalStart();
}

void JsFlowExecutor::InternalStart() {
  DCHECK(isolated_world_context_id_ != -1);
  DCHECK(callback_);

  // Before running the flow in the sandbox, we define a promise that
  // the flow may fulfill to request execution of a native action.
  RefreshNativeActionPromise();

  // Wrap the main js_flow in an async function containing a method to
  // request native actions. This is essentially providing |js_flow| with a
  // JS API to call native functionality.
  // TODO(b/208420231): adjust linenumbers to account for the offset introduced
  // by this wrapper, otherwise exception stacktraces will be hard to map to the
  // original js source.
  js_flow_ = std::make_unique<std::string>(base::StrCat({
      R"((async function() {
        function runNativeAction(native_action) {
          return new Promise(
              (fulfill, reject) => {
                   globalFlowState.runNativeAction([native_action, fulfill])
              }
          )
        }
    )",
      *js_flow_, "  }) ()"}));

  // Run the wrapped js_flow in the sandbox and serve potential native action
  // requests as they arrive.
  devtools_client_->GetRuntime()->Evaluate(
      runtime::EvaluateParams::Builder()
          .SetExpression(*js_flow_)
          .SetAwaitPromise(true)
          .SetReturnByValue(true)
          .SetContextId(isolated_world_context_id_)
          .Build(),
      kMainFrame,
      base::BindOnce(&JsFlowExecutor::OnFlowFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void JsFlowExecutor::RefreshNativeActionPromise() {
  devtools_client_->GetRuntime()->Evaluate(
      runtime::EvaluateParams::Builder()
          .SetExpression(kRunNativeAction)
          .SetAwaitPromise(true)
          .SetContextId(isolated_world_context_id_)
          .Build(),
      kMainFrame,
      base::BindOnce(&JsFlowExecutor::OnNativeActionRequested,
                     weak_ptr_factory_.GetWeakPtr()));
}

void JsFlowExecutor::OnNativeActionRequested(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) {
  if (!CheckResultAndStopOnError(reply_status, result, __FILE__, __LINE__)) {
    return;
  }

  // We expect 2 arguments from JS: (1) the requested action(JSON-compatible
  // value), and (2) the fulfill promise to call with the action result.
  std::string js_array_object_id = result->GetResult()->GetObjectId();
  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgument(/* index = */ 0, &arguments);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(js_array_object_id)
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kArrayGetNthElement))
          .Build(),
      kMainFrame,
      base::BindOnce(&JsFlowExecutor::OnNativeActionRequestActionRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), js_array_object_id));
}

void JsFlowExecutor::OnNativeActionRequestActionRetrieved(
    const std::string& js_array_object_id,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!CheckResultAndStopOnError(reply_status, result, __FILE__, __LINE__)) {
    return;
  }

  auto* remote_object = result->GetResult();
  if (!remote_object->HasValue()) {
    ClientStatus status(UNEXPECTED_JS_ERROR);
    auto* details = status.mutable_details()->mutable_unexpected_error_info();
    details->set_source_file(__FILE__);
    details->set_source_line_number(__LINE__);

    std::string stringified_result;
    base::JSONWriter::Write(*remote_object->Serialize(), &stringified_result);
    details->set_devtools_error_message(
        base::StrCat({"runNativeAction expected single JSON-compatible "
                      "argument, but was called with ",
                      stringified_result}));
    RunCallback(status, nullptr);
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgument(/* index = */ 1, &arguments);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(js_array_object_id)
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kArrayGetNthElement))
          .Build(),
      kMainFrame,
      base::BindOnce(
          &JsFlowExecutor::OnNativeActionRequestFulfillPromiseRetrieved,
          weak_ptr_factory_.GetWeakPtr(), remote_object->Serialize()));
}

void JsFlowExecutor::OnNativeActionRequestFulfillPromiseRetrieved(
    std::unique_ptr<base::Value> action_request,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!CheckResultAndStopOnError(reply_status, result, __FILE__, __LINE__)) {
    return;
  }

  auto* fulfill_promise_object = result->GetResult();
  DCHECK(fulfill_promise_object);
  if (!fulfill_promise_object->HasObjectId()) {
    // This should never happen, since the fulfill promise is programmatically
    // provided.
    ClientStatus status(OTHER_ACTION_STATUS);
    auto* details = status.mutable_details()->mutable_unexpected_error_info();
    details->set_source_file(__FILE__);
    details->set_source_line_number(__LINE__);
    details->set_devtools_error_message(
        "Native action requested, but no fulfill promise provided");
    RunCallback(status, nullptr);
    return;
  }

  delegate_->RunNativeAction(
      std::move(action_request),
      base::BindOnce(&JsFlowExecutor::OnNativeActionFinished,
                     weak_ptr_factory_.GetWeakPtr(),
                     fulfill_promise_object->GetObjectId()));
}

void JsFlowExecutor::OnNativeActionFinished(
    const std::string& fulfill_promise_object_id,
    const ClientStatus& result_status,
    std::unique_ptr<base::Value> result_value) {
  if (!callback_) {
    // No longer relevant.
    return;
  }

  // Refresh the native action request promise.
  RefreshNativeActionPromise();

  // Fulfill the promise and thus resume the JS flow.
  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgument(static_cast<int>(result_status.proto_status()),
                         &arguments);
  auto result_arg = std::make_unique<base::Value>();
  if (result_value) {
    result_arg = std::move(result_value);
  }
  arguments.emplace_back(
      runtime::CallArgument::Builder().SetValue(std::move(result_arg)).Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(fulfill_promise_object_id)
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kFulfillActionPromise))
          .Build(),
      kMainFrame,
      base::BindOnce(&JsFlowExecutor::OnFlowResumed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void JsFlowExecutor::OnFlowResumed(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  // This should never fail, but if it does, we need to catch it here to prevent
  // the client from stalling indefinitely, waiting for the flow to resume.
  if (!CheckResultAndStopOnError(reply_status, result, __FILE__, __LINE__)) {
    return;
  }
}

void JsFlowExecutor::OnFlowFinished(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) {
  // Note that the result is always serialized if available, not just if the
  // flow was successful. In particular, this serializes exceptions.
  RunCallback(
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__),
      (result != nullptr ? result->Serialize() : nullptr));
}

void JsFlowExecutor::RunCallback(const ClientStatus& status,
                                 std::unique_ptr<base::Value> result_value) {
  if (!status.ok() && result_value) {
    DVLOG(1) << "Flow failed with " << status
             << " and result: " << *result_value;
  }
  std::move(callback_).Run(status, std::move(result_value));
}

}  // namespace autofill_assistant
