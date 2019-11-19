// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

class ActionDelegate;
class ClientStatus;

// An action that performs a single step of a script on the website.
class Action {
 public:
  virtual ~Action();

  // Callback runs after this action is executed, pass the result of this action
  // through a ProcessedActionProto object.
  // Delegate should outlive this object.
  using ProcessActionCallback =
      base::OnceCallback<void(std::unique_ptr<ProcessedActionProto>)>;

  void ProcessAction(ProcessActionCallback callback);

  const ActionProto& proto() const { return proto_; }

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

  // Intended for debugging. Writes a string representation of |action| to
  // |out|.
  friend std::ostream& operator<<(std::ostream& out, const Action& action);

  const ActionProto proto_;

  // Accumulate any result of this action during ProcessAction. Is only valid
  // during a run of ProcessAction.
  std::unique_ptr<ProcessedActionProto> processed_action_proto_;
  // Reference to the delegate that owns this action.
  ActionDelegate* delegate_;
};

// Intended for debugging. Writes a string representation of |action_case| to
// |out|.
std::ostream& operator<<(std::ostream& out,
                         const ActionProto::ActionInfoCase& action_case);

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_H_
