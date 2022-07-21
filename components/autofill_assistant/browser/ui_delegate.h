// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "bottom_sheet_state.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/event_handler.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/public/rectf.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/tts_button_state.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/viewport_mode.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {
class UiControllerObserver;
class Details;
class InfoBox;
class BasicInteractions;

// UI delegate called for script executions.
class UiDelegate {
 public:
  virtual ~UiDelegate() = default;

  // Returns the current status message.
  virtual std::string GetStatusMessage() const = 0;

  // Returns the current bubble / tooltip message.
  virtual std::string GetBubbleMessage() const = 0;

  // Returns the current contextual information. May be empty.
  virtual std::vector<Details> GetDetails() const = 0;

  // Returns the current info box data. May be null if empty.
  virtual const InfoBox* GetInfoBox() const = 0;

  // Returns the currently active progress step.
  virtual int GetProgressActiveStep() const = 0;

  // Returns whether the progress bar is visible.
  virtual bool GetProgressVisible() const = 0;

  // Returns whether the TTS button is visible.
  virtual bool GetTtsButtonVisible() const = 0;

  // Returns the current TTS button state.
  virtual TtsButtonState GetTtsButtonState() const = 0;

  // Returns the current configuration of the step progress bar.
  virtual ShowProgressBarProto::StepProgressBarConfiguration
  GetStepProgressBarConfiguration() const = 0;

  // Returns whether the progress bar should show an error state.
  virtual bool GetProgressBarErrorState() const = 0;

  // Returns the current set of user actions.
  virtual const std::vector<UserAction>& GetUserActions() const = 0;

  // Performs an action, from the set of actions returned by GetUserAction().
  //
  // Returns true if the action was triggered, false if the index did not
  // correspond to any enabled action.
  virtual bool PerformUserAction(int index) = 0;

  // If the controller is waiting for user data, this field contains a non-null
  // options describing the request.
  virtual const CollectUserDataOptions* GetCollectUserDataOptions() const = 0;

  // Handles a change in shipping address, in response to the current collect
  // user data options.
  virtual void HandleShippingAddressChange(
      std::unique_ptr<autofill::AutofillProfile> address,
      UserDataEventType event_type) = 0;

  // Handles a change in contact info, in response to the current collect user
  // data options.
  virtual void HandleContactInfoChange(
      std::unique_ptr<autofill::AutofillProfile> profile,
      UserDataEventType event_type) = 0;

  // Handles a change in the phone number, in response to the current collect
  // user data options.
  virtual void HandlePhoneNumberChange(
      std::unique_ptr<autofill::AutofillProfile> profile,
      UserDataEventType event_type) = 0;

  // Handles a change in credit card and billing profile, in response to the
  // current collect user data options.
  virtual void HandleCreditCardChange(
      std::unique_ptr<autofill::CreditCard> card,
      std::unique_ptr<autofill::AutofillProfile> billing_profile,
      UserDataEventType event_type) = 0;

  // Sets the state of the third party terms & conditions, pertaining to the
  // current collect user data options.
  virtual void SetTermsAndConditions(
      TermsAndConditionsState terms_and_conditions) = 0;

  // Sets the chosen login option, pertaining to the current collect user data
  // options.
  virtual void SetLoginOption(const std::string& identifier) = 0;

  // Called when the user clicks a link of the form <link0>text</link0> in a
  // text message.
  virtual void OnTextLinkClicked(int link) = 0;

  // Called when the user clicks a link in the form action.
  virtual void OnFormActionLinkClicked(int link) = 0;

  // Called when the user clicks the TTS button.
  virtual void OnTtsButtonClicked() = 0;

  // Sets an additional value.
  virtual void SetAdditionalValue(const std::string& client_memory_key,
                                  const ValueProto& value) = 0;

  // Called when QR Code Scan Action is finished.
  virtual void OnQrCodeScanFinished(
      const ClientStatus& status,
      const absl::optional<ValueProto>& value) = 0;

  // Peek mode state and whether it was changed automatically last time.
  virtual ConfigureBottomSheetProto::PeekMode GetPeekMode() = 0;

  // Gets the bottom sheet state.
  virtual BottomSheetState GetBottomSheetState() = 0;

  // Sets the state of the bottom sheet.
  virtual void SetBottomSheetState(BottomSheetState state) = 0;

  // Returns the current form. May be null if there is no form to show.
  virtual const FormProto* GetForm() const = 0;

  // Returns the current form data. May be null if there is no form to show.
  virtual const FormProto::Result* GetFormResult() const = 0;

  // Sets a counter value.
  virtual void SetCounterValue(int input_index,
                               int counter_index,
                               int value) = 0;

  // Sets whether a selection choice is selected.
  virtual void SetChoiceSelected(int input_index,
                                 int choice_index,
                                 bool selected) = 0;

  // Register an observer. Observers get told about changes to the
  // ui controller.
  virtual void AddObserver(UiControllerObserver* observer) = 0;

  // Remove a previously registered observer.
  virtual void RemoveObserver(const UiControllerObserver* observer) = 0;

  // Dispatches an event to the event handler.
  virtual void DispatchEvent(const EventHandler::EventKey& key) = 0;

  // Returns the event handler.
  virtual EventHandler* GetEventHandler() = 0;

  // Returns an object that provides basic interactions for the UI framework.
  virtual BasicInteractions* GetBasicInteractions() = 0;

  // Whether the sheet should be auto expanded when entering the prompt state.
  virtual bool ShouldPromptActionExpandSheet() const = 0;

  // Get PromptQrCodeScanProto, if any.
  virtual const PromptQrCodeScanProto* GetPromptQrCodeScanProto() const = 0;

  // The generic user interface to show, if any.
  virtual const GenericUserInterfaceProto* GetGenericUiProto() const = 0;

  // The persistent generic user interface to show, if any.
  virtual const GenericUserInterfaceProto* GetPersistentGenericUiProto()
      const = 0;

  // Called when the visibility of the keyboard has changed.
  virtual void OnKeyboardVisibilityChanged(bool visible) = 0;

  // Called when the user starts or finishes to focus an input text field in the
  // bottom sheet.
  virtual void OnInputTextFocusChanged(bool is_text_focused) = 0;

 protected:
  UiDelegate() = default;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_DELEGATE_H_
