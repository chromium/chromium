// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/stopwatch.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

class ActionDelegate;
class ClientStatus;
class WaitForDomOperation;
class WaitForDocumentOperation;

// An action that performs a single step of a script on the website.
class Action {
 public:
  virtual ~Action();

  // Data only relevant to the currently running action.
  struct ActionData {
    ActionData();
    ~ActionData();

    // Navigation information relevant to this action.
    NavigationInfoProto navigation_info;

    std::unique_ptr<WaitForDomOperation> wait_for_dom;
    std::unique_ptr<WaitForDocumentOperation> wait_for_document;

    // This callback is set when a navigation event should terminate an ongoing
    // prompt action. Only a prompt action will set a valid callback here.
    base::OnceCallback<void()> end_prompt_on_navigation_callback;
  };

  // Callback runs after this action is executed, pass the result of this action
  // through a ProcessedActionProto object.
  // Delegate should outlive this object.
  using ProcessActionCallback =
      base::OnceCallback<void(std::unique_ptr<ProcessedActionProto>)>;

  void ProcessAction(ProcessActionCallback callback);

  const ActionProto& proto() const { return proto_; }

  // Returns the current action's ActionData.
  virtual ActionData& GetActionData();

 protected:
  // |delegate| must remain valid for the lifetime of this instance.
  explicit Action(ActionDelegate* delegate, const ActionProto& proto);

  // Subclasses must implement this method.
  virtual void InternalProcessAction(ProcessActionCallback callback) = 0;

  // Returns vector of string from a repeated proto field.
  static std::vector<std::string> ExtractVector(
      const google::protobuf::RepeatedPtrField<std::string>& repeated_strings);

  void UpdateProcessedAction(ProcessedActionStatusProto status);
  void UpdateProcessedAction(const ClientStatus& status);

  // Wraps the `ProcesseActionCallback` to also fill the relevant timing stat
  // fields.
  void RecordActionTimes(ProcessActionCallback callback,
                         std::unique_ptr<ProcessedActionProto>);

  // Wraps `callback` to record the wait in the action stopwatch for wait for
  // dom and short wait for element operations.
  void OnWaitForElementTimed(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& element_status,
      base::TimeDelta wait_time);

  // Intended for debugging. Writes a string representation of |action| to
  // |out|.
  friend std::ostream& operator<<(std::ostream& out, const Action& action);

  const ActionProto proto_;
  ActionData action_data_;

  // Accumulate any result of this action during ProcessAction. Is only valid
  // during a run of ProcessAction.
  std::unique_ptr<ProcessedActionProto> processed_action_proto_;
  // Reference to the delegate that owns this action.
  raw_ptr<ActionDelegate> delegate_;
  // Used to record active and wait times in the action execution.
  ActionStopwatch action_stopwatch_;

  base::WeakPtrFactory<Action> weak_ptr_factory_{this};

 private:
  friend class CollectUserDataActionTest;
  friend class JsFlowActionTest;
};

// Intended for debugging. Writes a string representation of |action_case| to
// |out|.
std::ostream& operator<<(std::ostream& out,
                         const ActionProto::ActionInfoCase& action_case);

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_H_
