// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_ACTION_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "components/autofill_assistant/browser/chip.h"
#include "components/autofill_assistant/browser/direct_action.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_context.h"

namespace autofill_assistant {

// An action that the user can perform, through the UI or, on Android Q, through
// a direct action.
class UserAction {
 public:
  // Executes a user action with the given additional trigger context.
  //
  // The context is relevant only for actions that execute a script.
  using Callback = base::OnceCallback<void(std::unique_ptr<TriggerContext>)>;

  UserAction(UserAction&&);
  UserAction();
  ~UserAction();
  UserAction& operator=(UserAction&&);

  // Initializes user action from proto.
  UserAction(const UserActionProto& action);
  UserAction(const ChipProto& chip, const DirectActionProto& direct_action);

  // Returns true if the action has no trigger, that is, there is no chip and no
  // direct action.
  bool has_triggers() const {
    return !chip_.empty() || !direct_action_.empty();
  }

  const Chip& chip() const { return chip_; }
  Chip& chip() { return chip_; }

  const DirectAction& direct_action() const { return direct_action_; }
  DirectAction& direct_action() { return direct_action_; }

  void SetEnabled(bool enabled) { enabled_ = enabled; }

  bool enabled() const { return enabled_; }

  // Checks whether a callback is assigned to the action. Actions without
  // callbacks do nothing.
  bool HasCallback() const { return callback_ ? true : false; }

  // Specifies a callback that accepts no context.
  void SetCallback(base::OnceCallback<void()> callback);

  // Specifies a callback that accepts a context.
  void SetCallback(
      base::OnceCallback<void(std::unique_ptr<TriggerContext>)> callback) {
    callback_ = std::move(callback);
  }

  // Intercept calls to this action.
  void AddInterceptor(
      base::OnceCallback<void(UserAction::Callback,
                              std::unique_ptr<TriggerContext>)> interceptor);

  // Call this action within the specific context, if a callback is set.
  void Call(std::unique_ptr<TriggerContext> context) {
    if (!callback_)
      return;

    std::move(callback_).Run(std::move(context));
  }

 private:
  // Specifies how the user can perform the action through the UI. Might be
  // empty.
  Chip chip_;

  // Specifies how the user can perform the action as a direct action. Might be
  // empty.
  DirectAction direct_action_;

  // Whether the action is enabled. The chip for a disabled action might still
  // be shown.
  bool enabled_ = true;

  // Callback triggered to trigger the action.
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(UserAction);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_ACTION_H_
