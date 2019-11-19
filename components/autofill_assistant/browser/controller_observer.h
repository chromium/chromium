// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_OBSERVER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_OBSERVER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/observer_list.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/info_box.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/ui_delegate.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/viewport_mode.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace autofill_assistant {

// Observes Controller's state.
class ControllerObserver : public base::CheckedObserver {
 public:
  ControllerObserver();
  ~ControllerObserver() override;

  // Called when the controller has entered a new state.
  virtual void OnStateChanged(AutofillAssistantState new_state) = 0;

  // Report that the status message has changed.
  virtual void OnStatusMessageChanged(const std::string& message) = 0;

  // Report that the bubble / tooltip message has changed.
  virtual void OnBubbleMessageChanged(const std::string& message) = 0;

  // If the current chrome activity is a custom tab activity, close it.
  // Otherwise, do nothing.
  virtual void CloseCustomTab() = 0;

  // Report that the set of user actions has changed.
  virtual void OnUserActionsChanged(
      const std::vector<UserAction>& user_actions) = 0;

  // Report that the options configuring a CollectUserDataAction have changed.
  virtual void OnCollectUserDataOptionsChanged(
      const CollectUserDataOptions* options) = 0;

  // Report that a field in |user_data| has changed.
  virtual void OnUserDataChanged(const UserData* state,
                                 UserData::FieldChange field_change) = 0;

  // Called when details have changed. Details will be null if they have been
  // cleared.
  virtual void OnDetailsChanged(const Details* details) = 0;

  // Called when info box has changed. |info_box| will be null if it has been
  // cleared.
  virtual void OnInfoBoxChanged(const InfoBox* info_box) = 0;

  // Called when the current progress has changed. Progress, is expressed as a
  // percentage.
  virtual void OnProgressChanged(int progress) = 0;

  // Called when the current progress bar visibility has changed. If |visible|
  // is true, then the bar is now shown.
  virtual void OnProgressVisibilityChanged(bool visible) = 0;

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

  // Called when the peek mode has changed.
  virtual void OnPeekModeChanged(
      ConfigureBottomSheetProto::PeekMode peek_mode) = 0;

  // Called when the overlay colors have changed.
  virtual void OnOverlayColorsChanged(
      const UiDelegate::OverlayColors& colors) = 0;

  // Called when the form has changed.
  virtual void OnFormChanged(const FormProto* form) = 0;

  // Called when client settings have changed.
  virtual void OnClientSettingsChanged(const ClientSettings& settings) = 0;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_OBSERVER_H_
