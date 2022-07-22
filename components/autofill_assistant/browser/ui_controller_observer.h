// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_CONTROLLER_OBSERVER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_CONTROLLER_OBSERVER_H_

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

namespace autofill_assistant {

// Observes UiController's state.
class UiControllerObserver : public base::CheckedObserver {
 public:
  UiControllerObserver();
  ~UiControllerObserver() override;

  // Report that the status message has changed.
  virtual void OnStatusMessageChanged(const std::string& message) = 0;

  // Report that the bubble / tooltip message has changed.
  virtual void OnBubbleMessageChanged(const std::string& message) = 0;

  // Report that the set of user actions has changed.
  virtual void OnUserActionsChanged(
      const std::vector<UserAction>& user_actions) = 0;

  // Report that the options configuring a CollectUserDataAction have changed.
  virtual void OnCollectUserDataOptionsChanged(
      const CollectUserDataOptions* options) = 0;

  // Report that the state of the User Data UI has changed.
  virtual void OnCollectUserDataUiStateChanged(
      bool loading,
      UserDataEventField event_field) = 0;

  // Called when details have changed. Details will be empty if they have been
  // cleared.
  virtual void OnDetailsChanged(const std::vector<Details>& details) = 0;

  // Called when info box has changed. |info_box| will be null if it has been
  // cleared.
  virtual void OnInfoBoxChanged(const InfoBox* info_box) = 0;

  // Called when the currently active progress step has changed.
  virtual void OnProgressActiveStepChanged(int active_step) = 0;

  // Called when the current progress bar visibility has changed. If |visible|
  // is true, then the bar is now shown.
  virtual void OnProgressVisibilityChanged(bool visible) = 0;

  virtual void OnStepProgressBarConfigurationChanged(
      const ShowProgressBarProto::StepProgressBarConfiguration&
          configuration) = 0;

  // Called when  the progress bar error state changes.
  virtual void OnProgressBarErrorStateChanged(bool error) = 0;

  // Called when the peek mode has changed.
  virtual void OnPeekModeChanged(
      ConfigureBottomSheetProto::PeekMode peek_mode) = 0;

  // Called when the bottom sheet should be expanded.
  virtual void OnExpandBottomSheet() = 0;

  // Called when the bottom sheet should be collapsed.
  virtual void OnCollapseBottomSheet() = 0;

  // Called when the form has changed.
  virtual void OnFormChanged(const FormProto* form,
                             const FormProto::Result* result) = 0;

  // Called when the generic user interface to show has been changed or cleared.
  virtual void OnGenericUserInterfaceChanged(
      const GenericUserInterfaceProto* generic_ui) = 0;

  // Called when the persistent generic user interface to show has been changed
  // or cleared.
  virtual void OnPersistentGenericUserInterfaceChanged(
      const GenericUserInterfaceProto* generic_ui) = 0;

  // Called when the TTS button visibility has changed. If |visible| is true,
  // then the button is shown.
  virtual void OnTtsButtonVisibilityChanged(bool visible) = 0;

  // Called when Tts Button State has changed.
  virtual void OnTtsButtonStateChanged(TtsButtonState state) = 0;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_CONTROLLER_OBSERVER_H_
