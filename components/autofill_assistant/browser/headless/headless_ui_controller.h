// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_HEADLESS_HEADLESS_UI_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_HEADLESS_HEADLESS_UI_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "components/autofill_assistant/browser/autofill_assistant_impl.h"
#include "components/autofill_assistant/browser/controller_observer.h"
#include "components/autofill_assistant/browser/execution_delegate.h"
#include "components/autofill_assistant/browser/script_executor_ui_delegate.h"

namespace autofill_assistant {

class HeadlessUiController : public ScriptExecutorUiDelegate {
 public:
  HeadlessUiController();

  // Overrides ScriptExecutorUiDelegate
  void SetStatusMessage(const std::string& message) override;
  std::string GetStatusMessage() const override;
  void SetBubbleMessage(const std::string& message) override;
  std::string GetBubbleMessage() const override;
  void SetTtsMessage(const std::string& message) override;
  TtsButtonState GetTtsButtonState() const override;
  ConfigureBottomSheetProto::PeekMode GetPeekMode() override;
  std::string GetTtsMessage() const override;
  void MaybePlayTtsMessage() override;
  void SetDetails(std::unique_ptr<Details>, base::TimeDelta delay) override;
  void AppendDetails(std::unique_ptr<Details> details,
                     base::TimeDelta delay) override;
  void SetInfoBox(const InfoBox& info_box) override;
  void ClearInfoBox() override;
  bool SetProgressActiveStepIdentifier(
      const std::string& active_step_identifier) override;
  void SetProgressActiveStep(int active_step) override;
  void SetProgressVisible(bool visible) override;
  void SetProgressBarErrorState(bool error) override;
  void SetStepProgressBarConfiguration(
      const ShowProgressBarProto::StepProgressBarConfiguration& configuration)
      override;
  void SetUserActions(
      std::unique_ptr<std::vector<UserAction>> user_actions) override;
  void SetPeekMode(ConfigureBottomSheetProto::PeekMode peek_mode) override;
  void ExpandBottomSheet() override;
  void CollapseBottomSheet() override;
  bool SetForm(
      std::unique_ptr<FormProto> form,
      base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
      base::OnceCallback<void(const ClientStatus&)> cancel_callback) override;
  void SetGenericUi(
      std::unique_ptr<GenericUserInterfaceProto> generic_ui,
      base::OnceCallback<void(const ClientStatus&)> end_action_callback,
      base::OnceCallback<void(const ClientStatus&)>
          view_inflation_finished_callback) override;
  void SetPersistentGenericUi(
      std::unique_ptr<GenericUserInterfaceProto> generic_ui,
      base::OnceCallback<void(const ClientStatus&)>
          view_inflation_finished_callback) override;
  void ClearGenericUi() override;
  void ClearPersistentGenericUi() override;
  void SetShowFeedbackChip(bool show_feedback_chip) override;

  void SetExpandSheetForPromptAction(bool expand) override;
  void SetCollectUserDataOptions(CollectUserDataOptions* options) override;
  void SetLastSuccessfulUserDataOptions(std::unique_ptr<CollectUserDataOptions>
                                            collect_user_data_options) override;
  const CollectUserDataOptions* GetLastSuccessfulUserDataOptions()
      const override;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_HEADLESS_HEADLESS_UI_CONTROLLER_H_
