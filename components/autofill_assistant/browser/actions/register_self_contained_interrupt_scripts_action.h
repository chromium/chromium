// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_REGISTER_SELF_CONTAINED_INTERRUPT_SCRIPTS_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_REGISTER_SELF_CONTAINED_INTERRUPT_SCRIPTS_ACTION_H_

#include <memory>
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {
class ServiceRequestSender;

// Registers one or multiple self-contained interrupt scripts. Self-contained
// interrupt scripts will never communicate with a backend to fetch scripts or
// actions, but they are allowed to call the ReportProgress endpoint for MSBB
// users.
class RegisterSelfContainedInterruptScriptsAction : public Action {
 public:
  explicit RegisterSelfContainedInterruptScriptsAction(
      ActionDelegate* delegate,
      const ActionProto& proto);
  ~RegisterSelfContainedInterruptScriptsAction() override;

  RegisterSelfContainedInterruptScriptsAction(
      const RegisterSelfContainedInterruptScriptsAction&) = delete;
  RegisterSelfContainedInterruptScriptsAction& operator=(
      const RegisterSelfContainedInterruptScriptsAction&) = delete;

 private:
  friend class RegisterSelfContainedInterruptScriptsActionTest;

  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(const ClientStatus& status);

  // Only for testing: a service request sender to inject.
  std::unique_ptr<ServiceRequestSender> service_request_sender_to_inject_;
  ProcessActionCallback callback_;
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_REGISTER_SELF_CONTAINED_INTERRUPT_SCRIPTS_ACTION_H_
