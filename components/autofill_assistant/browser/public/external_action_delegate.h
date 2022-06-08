// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_EXTERNAL_ACTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_EXTERNAL_ACTION_DELEGATE_H_

#include "base/callback.h"
#include "components/autofill_assistant/browser/public/external_action.pb.h"

namespace autofill_assistant {

// Allows to handle external actions happening during the execution of a
// script.
class ExternalActionDelegate {
 public:
  // Called to notify a change in the DOM.
  using DomUpdateCallback =
      base::RepeatingCallback<void(const external::ElementConditionsUpdate&)>;

  virtual ~ExternalActionDelegate() = default;
  // Called when the script reaches an external action.
  // The |start_dom_checks_callback| can optionally be called to start the DOM
  // checks. This will allow interrupts to trigger (if the action itself allows
  // them). Calling |end_action_callback| will end the external action and
  // resume the execution of the rest of the script.
  virtual void OnActionRequested(
      const external::Action& action_info,
      base::OnceCallback<void(DomUpdateCallback)> start_dom_checks_callback,
      base::OnceCallback<void(const external::Result&)>
          end_action_callback) = 0;

  // Called before starting the execution of an interrupt.
  virtual void OnInterruptStarted() = 0;

  // Called after finishing to execute an interrupt, before resuming the
  // execution of the main script.
  virtual void OnInterruptFinished() = 0;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_EXTERNAL_ACTION_DELEGATE_H_
