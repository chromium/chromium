// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/register_js_interrupt_for_parent_js_flow_action.h"

#include <utility>

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/actions/register_self_contained_interrupt_scripts_action.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

RegisterJsInterruptForParentJsFlowAction::
    RegisterJsInterruptForParentJsFlowAction(ActionDelegate* delegate,
                                             const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_register_js_interrupt_for_flow());
}

RegisterJsInterruptForParentJsFlowAction::
    ~RegisterJsInterruptForParentJsFlowAction() = default;

void RegisterJsInterruptForParentJsFlowAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  const Action* parent_action = delegate_->GetCurrentRootAction();
  if (!parent_action) {
    // Should never happen.
    EndAction(ClientStatus(OTHER_ACTION_STATUS));
    return;
  }

  if (!parent_action->proto().has_js_flow()) {
    LOG(ERROR) << __func__ << ": was not called from within a JS flow action";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  ActionProto nested_action_proto;
  RegisterSelfContainedInterruptScripts* register_interrupt_scripts_proto =
      nested_action_proto.mutable_register_interrupt_scripts();
  auto* supports_site_response =
      register_interrupt_scripts_proto->mutable_match_info()
          ->mutable_supports_site_response();
  auto* supports_site_script = supports_site_response->add_scripts();
  supports_site_script->set_path(
      proto_.register_js_interrupt_for_flow().path());
  supports_site_script->mutable_presentation()->set_interrupt(true);
  *supports_site_script->mutable_presentation()
       ->mutable_precondition()
       ->mutable_element_condition() =
      proto_.register_js_interrupt_for_flow().precondition();

  // Create self-contained interrupt script, containing a single JS flow action
  // reusing the parent flow's JS blob, and providing the proto-specified
  // startup parameter so the JS blob can know to run the interrupt and not the
  // main flow.
  // TODO(fga): it would be nice to avoid having to duplicate the entire flow
  // blob here.
  auto* routine_script = register_interrupt_scripts_proto->mutable_match_info()
                             ->add_routine_scripts();
  routine_script->set_script_path(
      proto_.register_js_interrupt_for_flow().path());
  auto* js_flow_proto = routine_script->mutable_action_response()
                            ->add_actions()
                            ->mutable_js_flow();
  js_flow_proto->set_js_flow(parent_action->proto().js_flow().js_flow());
  js_flow_proto->set_startup_param_name(
      proto_.register_js_interrupt_for_flow().js_startup_variable_name());
  js_flow_proto->set_startup_param_value(
      proto_.register_js_interrupt_for_flow().js_startup_variable_value());

  // Run a nested RegisterSelfContainedInterruptScriptsAction to register the
  // above-defined interrupt script.
  current_nested_action_ =
      std::make_unique<RegisterSelfContainedInterruptScriptsAction>(
          delegate_, nested_action_proto);
  current_nested_action_->ProcessAction(base::BindOnce(
      &RegisterJsInterruptForParentJsFlowAction::OnInterruptRegistered,
      weak_ptr_factory_.GetWeakPtr()));
}

void RegisterJsInterruptForParentJsFlowAction::OnInterruptRegistered(
    std::unique_ptr<ProcessedActionProto> processed_action) {
  // End the action by forwarding the client status of the nested action.
  EndAction(ClientStatus(processed_action->status()));
}

void RegisterJsInterruptForParentJsFlowAction::EndAction(
    const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
