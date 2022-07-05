// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_EXTERNAL_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_EXTERNAL_ACTION_H_

#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/public/external_action_delegate.h"
#include "components/autofill_assistant/browser/public/headless_script_controller.h"
#include "components/autofill_assistant/browser/wait_for_dom_observer.h"

namespace autofill_assistant {

class ExternalAction : public Action {
 public:
  explicit ExternalAction(ActionDelegate* delegate, const ActionProto& proto);

  ExternalAction(const ExternalAction&) = delete;
  ExternalAction& operator=(const ExternalAction&) = delete;

  ~ExternalAction() override;

 private:
  struct ConditionStatus {
    ExternalActionProto::ExternalCondition proto;

    // We always update the result and send the notification for each condition
    // after the first check so the initial value of these does not matter.
    bool result = false;
    bool changed = false;
  };

  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void StartDomChecks(
      ExternalActionDelegate::DomUpdateCallback dom_update_callback);
  void SetupConditions();
  void OnPreconditionResult(
      size_t condition_index,
      const ClientStatus& status,
      const std::vector<std::string>& ignored_payloads,
      const std::vector<std::string>& ignored_tags,
      const base::flat_map<std::string, DomObjectFrameStack>& ignored_elements);
  void RegisterChecks(
      BatchElementChecker* checker,
      base::OnceCallback<void(const ClientStatus&)> wait_for_dom_callback);
  void OnElementChecksDone(
      base::OnceCallback<void(const ClientStatus&)> wait_for_dom_callback);
  void OnDoneWaitForDom(const ClientStatus& status);
  void OnExternalActionFinished(const external::Result& success);
  void ProcessExternalResult();
  void EndAction(const ClientStatus& status);
  void SetSelectedProfiles(
      const google::protobuf::Map<std::string,
                                  autofill_assistant::external::ProfileProto>&
          profiles_proto);
  void SetSelectedCreditCard(
      const autofill_assistant::external::CreditCardProto& credit_card_proto);

  ProcessActionCallback callback_;

  // The list of conditions to be checked.
  std::vector<ConditionStatus> conditions_;
  // Keeps track of whether we have already sent the first notification about
  // the conditions.
  bool first_condition_notification_sent_ = false;
  // Whether there is a currently running WaitForDom.
  bool has_pending_wait_for_dom_ = false;
  // The callback to notify element condition updates.
  ExternalActionDelegate::DomUpdateCallback dom_update_callback_;

  // Whether we received a notification from the external caller to end the
  // action.
  bool external_action_end_requested_ = false;
  // The external result reported when the external caller requested to end the
  // action.
  external::Result external_action_result_;

  base::WeakPtrFactory<ExternalAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_EXTERNAL_ACTION_H_
