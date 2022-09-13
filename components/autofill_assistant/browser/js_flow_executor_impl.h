// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JS_FLOW_EXECUTOR_IMPL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JS_FLOW_EXECUTOR_IMPL_H_

#include <memory>
#include <string>
#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/js_flow_devtools_wrapper.h"
#include "components/autofill_assistant/browser/js_flow_executor.h"

namespace autofill_assistant {

// Executes a JS flow in a sandbox JS context. The flow may request additional
// native actions to be performed by its delegate.
class JsFlowExecutorImpl : public JsFlowExecutor {
 public:
  // |delegate| and |devtools_wrapper| must outlive the JsFlowExecutorImpl.
  JsFlowExecutorImpl(Delegate* delegate,
                     JsFlowDevtoolsWrapper* js_flow_devtools_wrapper);
  ~JsFlowExecutorImpl() override;
  JsFlowExecutorImpl(const JsFlowExecutorImpl&) = delete;
  JsFlowExecutorImpl& operator=(const JsFlowExecutorImpl&) = delete;

  // Starts executing |js_flow| in an isolated JS context. Once finished (or on
  // error), |result_callback| is invoked with the final result. In the case of
  // an uncaught exception during flow execution, the returned status may
  // contain a stack trace and additional information (limited to the sandbox).
  // Only one flow may run at a time.
  //
  // The last statement of the |js_flow| must be a return statement.
  //
  // Flows may request additional native actions from the delegate, using the
  // following syntax:
  //
  // let [status, result] = await runNativeAction(id, action)
  //
  // - [id] is a field tag number in the ActionProto.action_info oneof
  // - [action] is a string containing a base64-encoded serialized proto of the
  //            type appropriate for [id]. It can also be a JSON array
  //            containing a proto in the JSPB wire format, though this comes
  //            with severe limitations and will not work for all protos. See
  //            parse_jspb.h for details.
  // - |status| is an int corresponding to a ProcessedActionStatusProto.
  // - [result] is a struct containing the result value, or an empty struct if
  //            no result was returned. The specific contents depend on the
  //            native action.
  //
  // The function runNativeAction() is guaranteed to be available when the
  // JavaScript snippet |js_flow| is run.
  //
  // The flow result is one of the following, depending on the |js_flow|:
  // (1) ACTION_APPLIED and a base::Value dictionary containing the overall
  // flow result. This will never contain string values - if the flow attempts
  // to return a dictionary containing strings, INVALID_ACTION and null are
  // returned instead.
  //
  // (2) UNEXPECTED_JS_ERROR in case of an execution error. The status may
  // contain the callstack line and column numbers, if available.
  //
  // (3) OTHER_ACTION_STATUS and null in case of internal errors during script
  // execution (i.e., unrecoverable devtools errors). The status details may
  // contain additional information.
  //
  // (4) INVALID_ACTION if the flow attempted to return a prohibited value, such
  // as a string.
  void Start(const std::string& js_flow,
             base::OnceCallback<void(const ClientStatus&,
                                     std::unique_ptr<base::Value>)>
                 result_callback) override;

 private:
  void InternalStart(const ClientStatus& status,
                     DevtoolsClient* devtools_client,
                     const int isolated_world_context_id);

  void RefreshNativeActionPromise();
  void OnNativeActionRequested(const DevtoolsClient::ReplyStatus& reply_status,
                               std::unique_ptr<runtime::EvaluateResult> result);
  void OnNativeActionRequestActionRetrieved(
      const std::string& js_array_object_id,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnNativeActionRequestFulfillPromiseRetrieved(
      std::unique_ptr<base::Value> action_request,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnNativeActionFinished(const std::string& fulfill_promise_object_id,
                              const ClientStatus& status,
                              std::unique_ptr<base::Value> result);
  void OnFlowResumed(const DevtoolsClient::ReplyStatus& reply_status,
                     std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnFlowFinished(const DevtoolsClient::ReplyStatus& reply_status,
                      std::unique_ptr<runtime::EvaluateResult> result);
  void RunCallback(const ClientStatus& status,
                   std::unique_ptr<base::Value> result_value);

  // Returns true if |reply_status| and |result| are ok. Else, stops the flow
  // and returns false.
  template <typename T>
  bool CheckResultAndStopOnError(
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<T>& result,
      const char* file,
      int line) {
    ClientStatus status =
        CheckJavaScriptResult(reply_status, result.get(), file, line);
    if (!status.ok()) {
      RunCallback(status, (result != nullptr ? result->Serialize() : nullptr));
      return false;
    }
    return true;
  }

  const raw_ptr<Delegate> delegate_;
  raw_ptr<JsFlowDevtoolsWrapper> js_flow_devtools_wrapper_;

  // Only set during a flow.
  raw_ptr<DevtoolsClient> devtools_client_;
  int isolated_world_context_id_ = -1;

  std::unique_ptr<std::string> js_flow_;

  base::OnceCallback<void(const ClientStatus&, std::unique_ptr<base::Value>)>
      callback_;

  base::WeakPtrFactory<JsFlowExecutorImpl> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JS_FLOW_EXECUTOR_IMPL_H_
