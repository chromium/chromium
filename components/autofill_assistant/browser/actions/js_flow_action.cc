// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/js_flow_action.h"
#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/metrics/field_trial.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/js_flow_executor_impl.h"
#include "components/autofill_assistant/browser/js_flow_util.h"
#include "components/autofill_assistant/browser/protocol_utils.h"

namespace autofill_assistant {

namespace {

// When starting a JS flow action, a synthetic field trial is recorded. This is
// used to allow tracking stability metrics as we start using this new action.
// Note there is no control group - this is purely for stability tracking.
const char kJsFlowActionSyntheticFieldTrialName[] =
    "AutofillAssistantJsFlowAction";
const char kJsFlowActionEnabledGroup[] = "Enabled";

}  // namespace

JsFlowAction::JsFlowAction(ActionDelegate* delegate, const ActionProto& proto)
    : Action(delegate, proto),
      js_flow_executor_(
          std::make_unique<JsFlowExecutorImpl>(delegate->GetWebContents(),
                                               this)) {
  DCHECK(proto_.has_js_flow());
}

JsFlowAction::JsFlowAction(ActionDelegate* delegate,
                           const ActionProto& proto,
                           std::unique_ptr<JsFlowExecutor> js_flow_executor)
    : Action(delegate, proto), js_flow_executor_(std::move(js_flow_executor)) {
  DCHECK(proto_.has_js_flow());
}

JsFlowAction::~JsFlowAction() = default;

Action::ActionData& JsFlowAction::GetActionData() {
  if (!current_native_action_) {
    return Action::GetActionData();
  }
  return current_native_action_->GetActionData();
}

void JsFlowAction::RunNativeAction(
    int action_id,
    const std::string& action,
    base::OnceCallback<void(const ClientStatus& result_status,
                            std::unique_ptr<base::Value> result_value)>
        finished_callback) {
  DCHECK(!current_native_action_) << "Must not call RunNativeAction while "
                                     "already executing a native action";
  std::string error_message;
  absl::optional<ActionProto> action_proto =
      ProtocolUtils::ParseFromString(action_id, action, &error_message);
  if (!action_proto) {
    VLOG(1) << error_message;
    std::move(finished_callback).Run(ClientStatus(INVALID_ACTION), nullptr);
    return;
  }

  if (action_proto->action_info_case() ==
      ActionProto::ActionInfoCase::kJsFlow) {
    LOG(ERROR) << "Nested JS flow actions are not allowed!";
    std::move(finished_callback).Run(ClientStatus(INVALID_ACTION), nullptr);
    return;
  }

  current_native_action_ =
      ProtocolUtils::CreateAction(delegate_, *action_proto);
  VLOG(2) << "Running native action: " << action_proto->action_info_case();
  current_native_action_->ProcessAction(base::BindOnce(
      &JsFlowAction::OnNativeActionFinished, weak_ptr_factory_.GetWeakPtr(),
      std::move(finished_callback)));
}

void JsFlowAction::OnNativeActionFinished(
    base::OnceCallback<void(const ClientStatus& result_status,
                            std::unique_ptr<base::Value> result_value)>
        finished_callback,
    std::unique_ptr<ProcessedActionProto> processed_action) {
  VLOG(2) << "Native action finished with status "
          << processed_action->status();

  current_native_action_.reset();

  std::move(finished_callback)
      .Run(ClientStatus(processed_action->status(),
                        processed_action->status_details()),
           js_flow_util::NativeActionResultToResultValue(*processed_action));
}

void JsFlowAction::InternalProcessAction(ProcessActionCallback callback) {
  base::FieldTrialList::CreateFieldTrial(kJsFlowActionSyntheticFieldTrialName,
                                         kJsFlowActionEnabledGroup);

  js_flow_executor_->Start(
      proto_.js_flow().js_flow(),
      base::BindOnce(&JsFlowAction::OnFlowFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void JsFlowAction::OnFlowFinished(ProcessActionCallback callback,
                                  const ClientStatus& status,
                                  std::unique_ptr<base::Value> return_value) {
  UpdateProcessedAction(status);

  // If the flow returned a value, we extract the status and possibly a flow
  // return value from that, and the overall action result will be whatever the
  // flow returned. Flows that don't return a value are assumed to have
  // succeeded. See js_flow_util::ExtractJsFlowActionReturnValue for details.
  if (return_value) {
    std::unique_ptr<base::Value> out_return_value;
    UpdateProcessedAction(js_flow_util::ExtractJsFlowActionReturnValue(
        *return_value, out_return_value));
    if (out_return_value) {
      base::JSONWriter::Write(*out_return_value,
                              processed_action_proto_->mutable_js_flow_result()
                                  ->mutable_result_json());
    }
  }

  // Since JS flows have the potential to be quite big, we remove them from the
  // action response. The backend has access to the full script anyway.
  processed_action_proto_->mutable_action()->mutable_js_flow()->clear_js_flow();

  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
