// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/js_flow_executor_impl.h"
#include "base/base64.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/js_flow_util.h"
#include "components/autofill_assistant/browser/parse_jspb.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {
namespace {

// Messages must have a JSPB message ID starting with this prefix to be
// parseable in JSPB wire format.
//
// Such a message ID is used to distinguish arrays from JSPB messages.
constexpr char kMessageIdPrefix[] = "aa.msg";

// Initializes a |globalFlowState| variable on first run, and renews the
// promises that will let JS flows request native actions.
constexpr char kRunNativeActionPromise[] = R"(
  if (typeof globalFlowState === 'undefined') {
    globalFlowState = {};
  }

  new Promise((fulfill, reject) => {
    globalFlowState.runNativeAction = fulfill;
    globalFlowState.endNativeAction = reject;
  })
)";

// The code inserted before the JS flow. The flow is wrapped in an arrow
// function.
// NOTE: Do not remove the trailing new line as we want to have stack traces
// start at the first column.
constexpr char kLeadingWrapper[] = R"(
  function runNativeAction(native_action_id, native_action) {
    return new Promise(
        (fulfill, reject) => {
             globalFlowState.runNativeAction(
                 [{id: native_action_id, action: native_action}, fulfill]);
        }
    );
  }

  (async () => {

  // Keep the next empty line. The JS flow script should be concatenated
  // without leading spaces.

)";

// The code inserted after the JS flow. This closes and executes the arrow
// function from the leading wrapper.
// NOTE: Do not remove the leading new line. We want to make sure that the
// wrapper not on the same line as the JS flow source.
constexpr char kTrailingWrapper[] = "\n})()";

// The number of lines to subtract from all call stack entries sent to the
// backend.
constexpr int kJsLineOffset = []() {
  int num_lines = 0;
  for (const char c : kLeadingWrapper) {
    num_lines += c == '\n';
  }
  return num_lines;
}();

// The number of stack entries to drop before returning to the client. We drop
// one entry as the source sent from the backend is wrapped in an anonymous
// function.
constexpr int kNumStackEntriesToDrop = 1;

constexpr char kArrayGetNthElement[] =
    "function(index) { return this[index]; }";

constexpr char kFulfillActionPromise[] = R"(
  function(status, result) {
    this([status, result]);
  }
)";

absl::optional<std::string> ConvertActionToBytes(const base::Value* action,
                                                 std::string* error_message) {
  if (action == nullptr) {
    *error_message = "Null value";
    return absl::nullopt;
  }
  if (action->is_string()) {
    std::string bytes;
    // A base64-encoded string containing a serialized proto.
    if (base::Base64Decode(action->GetString(), &bytes)) {
      return bytes;
    }
    *error_message = "Invalid Base64-encoded string";
    return absl::nullopt;
  }
  if (action->is_list()) {
    // A JSON array containing a proto message in the JSPB wire format.
    return ParseJspb(kMessageIdPrefix, *action, error_message);
  }
  *error_message = "Unexpected value type";
  return absl::nullopt;
}

}  // namespace

JsFlowExecutorImpl::JsFlowExecutorImpl(
    Delegate* delegate,
    JsFlowDevtoolsWrapper* js_flow_devtools_wrapper)
    : delegate_(delegate),
      js_flow_devtools_wrapper_(js_flow_devtools_wrapper) {}

JsFlowExecutorImpl::~JsFlowExecutorImpl() = default;

void JsFlowExecutorImpl::Start(
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

  js_flow_devtools_wrapper_->GetDevtoolsAndMaybeInit(base::BindOnce(
      &JsFlowExecutorImpl::InternalStart, weak_ptr_factory_.GetWeakPtr()));
}

void JsFlowExecutorImpl::InternalStart(const ClientStatus& status,
                                       DevtoolsClient* devtools_client,
                                       const int isolated_world_context_id) {
  DCHECK(callback_);

  if (!status.ok()) {
    RunCallback(status, nullptr);
    return;
  }

  devtools_client_ = devtools_client;
  isolated_world_context_id_ = isolated_world_context_id;

  // Before running the flow in the sandbox, we define a promise that
  // the flow may fulfill to request execution of a native action.
  RefreshNativeActionPromise();

  // Wrap the main js_flow in an async function containing a method to
  // request native actions. This is essentially providing |js_flow| with a
  // JS API to call native functionality. Also appends the source url.
  js_flow_ = std::make_unique<std::string>(
      base::StrCat({kLeadingWrapper, *js_flow_, kTrailingWrapper,
                    js_flow_util::GetDevtoolsSourceUrlCommentToAppend(
                        UnexpectedErrorInfoProto::JS_FLOW)}));

  // Run the wrapped js_flow in the sandbox and serve potential native action
  // requests as they arrive.
  devtools_client_->GetRuntime()->Evaluate(
      runtime::EvaluateParams::Builder()
          .SetExpression(*js_flow_)
          .SetAwaitPromise(true)
          .SetReturnByValue(true)
          .SetContextId(isolated_world_context_id_)
          .Build(),
      js_flow_util::kMainFrame,
      base::BindOnce(&JsFlowExecutorImpl::OnFlowFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void JsFlowExecutorImpl::RefreshNativeActionPromise() {
  devtools_client_->GetRuntime()->Evaluate(
      runtime::EvaluateParams::Builder()
          .SetExpression(kRunNativeActionPromise)
          .SetAwaitPromise(true)
          .SetContextId(isolated_world_context_id_)
          .Build(),
      js_flow_util::kMainFrame,
      base::BindOnce(&JsFlowExecutorImpl::OnNativeActionRequested,
                     weak_ptr_factory_.GetWeakPtr()));
}

void JsFlowExecutorImpl::OnNativeActionRequested(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) {
  if (!CheckResultAndStopOnError(reply_status, result, __FILE__, __LINE__)) {
    return;
  }

  // We expect 2 arguments from JS: (1) the requested action(JSON-compatible
  // value), and (2) the fulfill promise to call with the action result.
  std::string js_array_object_id = result->GetResult()->GetObjectId();
  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgument(/* value = */ 0, &arguments);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(js_array_object_id)
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(kArrayGetNthElement)
          .SetReturnByValue(true)
          .Build(),
      js_flow_util::kMainFrame,
      base::BindOnce(&JsFlowExecutorImpl::OnNativeActionRequestActionRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), js_array_object_id));
}

void JsFlowExecutorImpl::OnNativeActionRequestActionRetrieved(
    const std::string& js_array_object_id,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!CheckResultAndStopOnError(reply_status, result, __FILE__, __LINE__)) {
    return;
  }

  auto* remote_object = result->GetResult();
  if (!remote_object->HasValue()) {
    // This shouldn't be possible, as the argument is built by
    // JsFlowExecutorImpl::InternalStart()
    RunCallback(UnexpectedErrorStatus(__FILE__, __LINE__), nullptr);
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgument(/* value = */ 1, &arguments);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(js_array_object_id)
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(kArrayGetNthElement)
          .Build(),
      js_flow_util::kMainFrame,
      base::BindOnce(
          &JsFlowExecutorImpl::OnNativeActionRequestFulfillPromiseRetrieved,
          weak_ptr_factory_.GetWeakPtr(),
          base::Value::ToUniquePtrValue(remote_object->GetValue()->Clone())));
}

void JsFlowExecutorImpl::OnNativeActionRequestFulfillPromiseRetrieved(
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
    RunCallback(UnexpectedErrorStatus(__FILE__, __LINE__), nullptr);
    return;
  }

  absl::optional<int> id;
  base::Value* action = nullptr;
  if (action_request->is_dict()) {
    id = action_request->FindIntKey("id");
    action = action_request->FindKey("action");
  }
  if (!id) {
    DVLOG(1) << "id passed to runNativeAction(id, action) is not a number in "
             << action_request->DebugString();
    RunCallback(ClientStatus(INVALID_ACTION), nullptr);
    return;
  }
  std::string error_message;
  absl::optional<std::string> action_bytes =
      ConvertActionToBytes(action, &error_message);
  if (!action_bytes) {
    DVLOG(1) << "action passed to runNativeAction(id, action) cannot "
             << "be parsed: " << error_message << " in "
             << action_request->DebugString();
    RunCallback(ClientStatus(INVALID_ACTION), nullptr);
    return;
  }
  delegate_->RunNativeAction(
      *id, *action_bytes,
      base::BindOnce(&JsFlowExecutorImpl::OnNativeActionFinished,
                     weak_ptr_factory_.GetWeakPtr(),
                     fulfill_promise_object->GetObjectId()));
}

void JsFlowExecutorImpl::OnNativeActionFinished(
    const std::string& fulfill_promise_object_id,
    const ClientStatus& result_status,
    std::unique_ptr<base::Value> result_value) {
  if (!callback_) {
    VLOG(2) << "Native action finished after js flow finished";
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
          .SetFunctionDeclaration(kFulfillActionPromise)
          .Build(),
      js_flow_util::kMainFrame,
      base::BindOnce(&JsFlowExecutorImpl::OnFlowResumed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void JsFlowExecutorImpl::OnFlowResumed(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  // This should never fail, but if it does, we need to catch it here to prevent
  // the client from stalling indefinitely, waiting for the flow to resume.
  if (!CheckResultAndStopOnError(reply_status, result, __FILE__, __LINE__)) {
    return;
  }
}

void JsFlowExecutorImpl::OnFlowFinished(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) {
  // Check and extract the return value. In case of exceptions, the sanitized
  // stack trace will be part of the returned ClientStatus. Only primitive
  // values are allowed (see js_flow_util::ExtractFlowReturnValue for details).
  std::unique_ptr<base::Value> out_result_value;
  ClientStatus status = js_flow_util::ExtractFlowReturnValue(
      reply_status, result.get(), out_result_value,
      /* js_line_offsets= */
      {{js_flow_util::GetDevtoolsSourceUrl(UnexpectedErrorInfoProto::JS_FLOW),
        kJsLineOffset}},
      kNumStackEntriesToDrop);

  RunCallback(status, std::move(out_result_value));
}

void JsFlowExecutorImpl::RunCallback(
    const ClientStatus& status,
    std::unique_ptr<base::Value> result_value) {
  if (!status.ok() && result_value) {
    VLOG(1) << "Flow failed with " << status
            << " and result: " << *result_value;
  } else if (!status.ok()) {
    VLOG(1) << "Flow failed with " << status;
  }

  std::move(callback_).Run(status, std::move(result_value));
}

}  // namespace autofill_assistant
