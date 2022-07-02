// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_OBSERVER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_OBSERVER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/observer_list_types.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/execution_delegate.h"
#include "components/autofill_assistant/browser/info_box.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/tts_button_state.h"
#include "components/autofill_assistant/browser/ui_delegate.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/viewport_mode.h"

namespace autofill_assistant {

// Observes Controller's state.
class ControllerObserver : public base::CheckedObserver {
 public:
  ControllerObserver();
  ~ControllerObserver() override;

  // Called when the controller has entered a new state.
  virtual void OnStateChanged(AutofillAssistantState new_state) = 0;

  // Called when the suppression state of the keyboard changed.
  virtual void OnKeyboardSuppressionStateChanged(
      bool should_suppress_keyboard) = 0;

  // If the current chrome activity is a custom tab activity, close it.
  // Otherwise, do nothing.
  virtual void CloseCustomTab() = 0;

  // Report an error. This does not imply that the flow has ended and is usually
  // followed by |OnStop|.
  virtual void OnError(const std::string& error_message,
                       Metrics::DropOutReason reason) = 0;

  // Report that a field in |user_data| has changed.
  virtual void OnUserDataChanged(const UserData& user_data,
                                 UserDataFieldChange field_change) = 0;

  // Updates the area of the visible viewport that is accessible when the
  // overlay state is OverlayState::PARTIAL.
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
      const std::vector<RectF>& restricted_areas) = 0;

  // Called when the viewport mode has changed.
  virtual void OnViewportModeChanged(ViewportMode mode) = 0;

  // Called when the overlay colors have changed.
  virtual void OnOverlayColorsChanged(
      const ExecutionDelegate::OverlayColors& colors) = 0;

  // Called when client settings have changed.
  virtual void OnClientSettingsChanged(const ClientSettings& settings) = 0;

  // Called when the desired overlay behavior has changed.
  virtual void OnShouldShowOverlayChanged(bool should_show) = 0;

  // Called before starting to execute a script.
  virtual void OnExecuteScript(const std::string& start_message) = 0;

  // Called when execution is started.
  virtual void OnStart(const TriggerContext& trigger_context) = 0;

  // Called when the flow is stopped.
  virtual void OnStop() = 0;

  // Called when the state needs to be reset.
  virtual void OnResetState() = 0;

  // Called whenever the UI is shown or hidden.
  virtual void OnUiShownChanged(bool shown) = 0;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_OBSERVER_H_
