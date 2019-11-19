// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/rectf.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/viewport_mode.h"

namespace autofill_assistant {
class ControllerObserver;
struct ClientSettings;

// UI delegate called for script executions.
class UiDelegate {
 public:
  // Colors of the overlay. Empty string to use the default.
  struct OverlayColors {
    // Overlay background color.
    std::string background;

    // Color of the border around the highlighted portions of the overlay.
    std::string highlight_border;
  };

  virtual ~UiDelegate() = default;

  // Returns the current state of the controller.
  virtual AutofillAssistantState GetState() = 0;

  // Asks for updated coordinates for the touchable area. This is called to
  // speed up update of the touchable areas when there are good reasons to think
  // that the current coordinates are out of date, such as while scrolling.
  virtual void UpdateTouchableArea() = 0;

  // Called when user interaction within the allowed touchable area was
  // detected. This should cause rerun of preconditions check.
  virtual void OnUserInteractionInsideTouchableArea() = 0;

  // Returns a string describing the current execution context. This is useful
  // when analyzing feedback forms and for debugging in general.
  virtual std::string GetDebugContext() = 0;

  // Returns the current status message.
  virtual std::string GetStatusMessage() const = 0;

  // Returns the current bubble / tooltip message.
  virtual std::string GetBubbleMessage() const = 0;

  // Returns the current contextual information. May be null if empty.
  virtual const Details* GetDetails() const = 0;

  // Returns the current info box data. May be null if empty.
  virtual const InfoBox* GetInfoBox() const = 0;

  // Returns the current progress; a percentage.
  virtual int GetProgress() const = 0;

  // Returns whether the progress bar is visible.
  virtual bool GetProgressVisible() const = 0;

  // Returns the current set of user actions.
  virtual const std::vector<UserAction>& GetUserActions() const = 0;

  // Performs an action, from the set of actions returned by GetUserAction().
  //
  // If non-empty, |context| is added to the global trigger context when
  // executing scripts. Ignored if no scripts are executed by the action.
  //
  // Returns true if the action was triggered, false if the index did not
  // correspond to any enabled actions.
  virtual bool PerformUserActionWithContext(
      int index,
      std::unique_ptr<TriggerContext> context) = 0;

  // Performs an action with no additional trigger context set.
  //
  // Returns true if the action was triggered, false if the index did not
  // correspond to any enabled actions.
  bool PerformUserAction(int index) {
    return PerformUserActionWithContext(index, TriggerContext::CreateEmpty());
  }

  // If the controller is waiting for user data, this field contains a non-null
  // options describing the request.
  virtual const CollectUserDataOptions* GetCollectUserDataOptions() const = 0;

  // If the controller is waiting for user data, this field contains a non-null
  // object describing the currently selected data.
  virtual const UserData* GetUserData() const = 0;

  // Sets shipping address, in response to the current collect user data
  // options.
  virtual void SetShippingAddress(
      std::unique_ptr<autofill::AutofillProfile> address) = 0;

  // Sets contact info, in response to the current collect user data options.
  virtual void SetContactInfo(
      std::unique_ptr<autofill::AutofillProfile> profile) = 0;

  // Sets credit card and billing profile, in response to the current collect
  // user data options.
  virtual void SetCreditCard(
      std::unique_ptr<autofill::CreditCard> card,
      std::unique_ptr<autofill::AutofillProfile> billing_profile) = 0;

  // Sets the state of the third party terms & conditions, pertaining to the
  // current collect user data options.
  virtual void SetTermsAndConditions(
      TermsAndConditionsState terms_and_conditions) = 0;

  // Sets the chosen login option, pertaining to the current collect user data
  // options.
  virtual void SetLoginOption(std::string identifier) = 0;

  // Called when the user clicks a link on the terms & conditions message.
  virtual void OnTermsAndConditionsLinkClicked(int link) = 0;

  // Sets the start of the date/time range.
  virtual void SetDateTimeRangeStart(int year,
                                     int month,
                                     int day,
                                     int hour,
                                     int minute,
                                     int second) = 0;

  // Sets the end of the date/time range.
  virtual void SetDateTimeRangeEnd(int year,
                                   int month,
                                   int day,
                                   int hour,
                                   int minute,
                                   int second) = 0;

  // Sets an additional value.
  virtual void SetAdditionalValue(const std::string& client_memory_key,
                                  const std::string& value) = 0;

  // Adds the rectangles that correspond to the current touchable area to
  // the given vector.
  //
  // At the end of this call, |rectangles| contains one element per
  // configured rectangles, though these can correspond to empty rectangles.
  // Coordinates absolute CSS coordinates.
  //
  // Note that the vector is not cleared before rectangles are added.
  virtual void GetTouchableArea(std::vector<RectF>* rectangles) const = 0;
  virtual void GetRestrictedArea(std::vector<RectF>* rectangles) const = 0;

  // Returns the current size of the visual viewport. May be empty if
  // unknown.
  //
  // The rectangle is expressed in absolute CSS coordinates.
  virtual void GetVisualViewport(RectF* viewport) const = 0;

  // Reports a fatal error to Autofill Assistant, which should then stop.
  virtual void OnFatalError(const std::string& error_message,
                            Metrics::DropOutReason reason) = 0;

  // Returns whether the viewport should be resized.
  virtual ViewportMode GetViewportMode() = 0;

  virtual ConfigureBottomSheetProto::PeekMode GetPeekMode() = 0;

  // Fills in the overlay colors.
  virtual void GetOverlayColors(OverlayColors* colors) const = 0;

  // Gets the current Client Settings
  virtual const ClientSettings& GetClientSettings() const = 0;

  // Returns the current form. May be null if there is no form to show.
  virtual const FormProto* GetForm() const = 0;

  // Sets a counter value.
  virtual void SetCounterValue(int input_index,
                               int counter_index,
                               int value) = 0;

  // Sets whether a selection choice is selected.
  virtual void SetChoiceSelected(int input_index,
                                 int choice_index,
                                 bool selected) = 0;

  // Register an observer. Observers get told about changes to the
  // controller.
  virtual void AddObserver(ControllerObserver* observer) = 0;

  // Remove a previously registered observer.
  virtual void RemoveObserver(const ControllerObserver* observer) = 0;

 protected:
 protected:
  UiDelegate() = default;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_DELEGATE_H_
