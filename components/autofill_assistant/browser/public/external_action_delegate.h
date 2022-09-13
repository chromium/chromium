// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_EXTERNAL_ACTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_EXTERNAL_ACTION_DELEGATE_H_

#include "base/callback.h"
#include "components/autofill_assistant/browser/public/external_action.pb.h"
#include "components/autofill_assistant/browser/public/rectf.h"

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
  // If |is_interrupt| is true, this action is part of an interrupt script.
  //
  // Note that if an ExternalAction allows interrupts, it's possible to receive
  // an |OnActionRequested| call before the |end_action_callback| for the
  // previous action has been called.
  virtual void OnActionRequested(
      const external::Action& action_info,
      bool is_interrupt,
      base::OnceCallback<void(DomUpdateCallback)> start_dom_checks_callback,
      base::OnceCallback<void(const external::Result&)>
          end_action_callback) = 0;

  // Called before starting the execution of an interrupt.
  virtual void OnInterruptStarted() {}

  // Called after finishing to execute an interrupt, before resuming the
  // execution of the main script.
  virtual void OnInterruptFinished() {}

  // Called to notify a change in the configuration of the touchable area.
  //
  // |visual_viewport| contains the position and size of the visual viewport in
  // the layout viewport. It might be empty if not known or the touchable area
  // is empty.
  //
  // |touchable_areas| contains one element per configured rectangle that should
  // be visible/touchable, though these can correspond to empty rectangles.
  //
  // |restricted_areas| contains one element per configured rectangle that
  // shouldn't be visible nor touchable. Those rectangles have precedence over
  // |touchable_areas|.
  //
  // All rectangles are expressed in absolute CSS coordinates.
  virtual void OnTouchableAreaChanged(
      const RectF& visual_viewport,
      const std::vector<RectF>& touchable_areas,
      const std::vector<RectF>& restricted_areas) {}
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_EXTERNAL_ACTION_DELEGATE_H_
