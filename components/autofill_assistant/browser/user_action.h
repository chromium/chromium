// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_ACTION_H_

#include <string>

#include "base/callback.h"
#include "components/autofill_assistant/browser/chip.h"
#include "components/autofill_assistant/browser/direct_action.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_context.h"

namespace autofill_assistant {

// An action that the user can perform through the UI.
class UserAction {
 public:
  UserAction(UserAction&&);
  UserAction();

  UserAction(const UserAction&) = delete;
  UserAction& operator=(const UserAction&) = delete;

  ~UserAction();
  UserAction& operator=(UserAction&&);

  // Initializes user action from proto.
  UserAction(const UserActionProto& action);
  UserAction(const ChipProto& chip,
             bool enabled,
             const std::string& identifier);

  // Returns true if the action has a chip.
  bool has_chip() const { return !chip_.empty(); }
  const Chip& chip() const { return chip_; }
  Chip& chip() { return chip_; }

  std::string identifier() const { return identifier_; }

  void SetEnabled(bool enabled) { enabled_ = enabled; }

  bool enabled() const { return enabled_; }

  // Checks whether a callback is assigned to the action. Actions without
  // callbacks do nothing.
  bool HasCallback() const { return callback_ ? true : false; }
  void SetCallback(base::OnceCallback<void()> callback) {
    callback_ = std::move(callback);
  }
  void RunCallback() {
    if (!callback_)
      return;

    std::move(callback_).Run();
  }

 private:
  // Specifies how the user can perform the action through the UI. Might be
  // empty.
  Chip chip_;

  // Whether the action is enabled. The chip for a disabled action might still
  // be shown.
  bool enabled_ = true;

  // Callback triggered to trigger the action.
  base::OnceCallback<void()> callback_;

  // Optional identifier to uniquely identify this user action.
  std::string identifier_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_ACTION_H_
