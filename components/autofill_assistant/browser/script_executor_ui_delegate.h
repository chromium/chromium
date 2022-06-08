// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_UI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_UI_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/observer_list_types.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/info_box.h"
#include "components/autofill_assistant/browser/public/external_action_delegate.h"
#include "components/autofill_assistant/browser/public/external_script_controller.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/tts_button_state.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/wait_for_dom_observer.h"
#include "url/gurl.h"

namespace autofill_assistant {

// A delegate which provides the ScriptExecutor with methods to control the
// Autofill Assistant UI.
class ScriptExecutorUiDelegate : public WaitForDomObserver {
 public:
  virtual void SetStatusMessage(const std::string& message) = 0;
  virtual std::string GetStatusMessage() const = 0;
  virtual void SetBubbleMessage(const std::string& message) = 0;
  virtual std::string GetBubbleMessage() const = 0;
  virtual void SetTtsMessage(const std::string& message) = 0;
  virtual std::string GetTtsMessage() const = 0;
  virtual TtsButtonState GetTtsButtonState() const = 0;
  virtual void MaybePlayTtsMessage() = 0;
  virtual void SetDetails(std::unique_ptr<Details> details,
                          base::TimeDelta delay) = 0;
  virtual void AppendDetails(std::unique_ptr<Details> details,
                             base::TimeDelta delay) = 0;
  virtual void SetInfoBox(const InfoBox& info_box) = 0;
  virtual void ClearInfoBox() = 0;
  virtual void SetCollectUserDataOptions(
      CollectUserDataOptions* collect_user_data_options) = 0;
  virtual void SetCollectUserDataUiState(bool loading,
                                         UserDataEventField event_field) = 0;
  virtual void SetLastSuccessfulUserDataOptions(
      std::unique_ptr<CollectUserDataOptions> collect_user_data_options) = 0;
  virtual const CollectUserDataOptions* GetLastSuccessfulUserDataOptions()
      const = 0;
  virtual bool SetProgressActiveStepIdentifier(
      const std::string& active_step_identifier) = 0;
  virtual void SetProgressActiveStep(int active_step) = 0;
  virtual void SetProgressVisible(bool visible) = 0;
  virtual void SetProgressBarErrorState(bool error) = 0;
  virtual void SetStepProgressBarConfiguration(
      const ShowProgressBarProto::StepProgressBarConfiguration&
          configuration) = 0;
  virtual void SetUserActions(
      std::unique_ptr<std::vector<UserAction>> user_action) = 0;
  virtual void SetPeekMode(ConfigureBottomSheetProto::PeekMode peek_mode) = 0;
  virtual ConfigureBottomSheetProto::PeekMode GetPeekMode() = 0;
  virtual void ExpandBottomSheet() = 0;
  virtual void CollapseBottomSheet() = 0;
  virtual bool SetForm(
      std::unique_ptr<FormProto> form,
      base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
      base::OnceCallback<void(const ClientStatus&)> cancel_callback) = 0;
  virtual void SetShowFeedbackChip(bool show_feedback_chip) = 0;

  // Set how the sheet should behave when entering a prompt state.
  virtual void SetExpandSheetForPromptAction(bool expand) = 0;

  // Sets the generic UI to show to the user.
  virtual void SetGenericUi(
      std::unique_ptr<GenericUserInterfaceProto> generic_ui,
      base::OnceCallback<void(const ClientStatus&)> end_action_callback,
      base::OnceCallback<void(const ClientStatus&)>
          view_inflation_finished_callback) = 0;

  // Sets the persistent generic UI to show to the user.
  virtual void SetPersistentGenericUi(
      std::unique_ptr<GenericUserInterfaceProto> generic_ui,
      base::OnceCallback<void(const ClientStatus&)>
          view_inflation_finished_callback) = 0;

  // Clears the generic UI.
  virtual void ClearGenericUi() = 0;

  // Clears the persistent generic UI.
  virtual void ClearPersistentGenericUi() = 0;

  // Whether this supports external actions.
  virtual bool SupportsExternalActions() = 0;

  // Executes the external action.
  virtual void ExecuteExternalAction(
      const external::Action& external_action,
      base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>
          start_dom_checks_callback,
      base::OnceCallback<void(const external::Result& result)>
          end_action_callback) = 0;

 protected:
  virtual ~ScriptExecutorUiDelegate() {}
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_UI_DELEGATE_H_
