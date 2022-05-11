// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_SCRIPT_EXECUTOR_UI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_SCRIPT_EXECUTOR_UI_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill_assistant/browser/script_executor_ui_delegate.h"
#include "components/autofill_assistant/browser/trigger_context.h"

namespace autofill_assistant {

// Implementation of ScriptExecutorUiDelegate that's convenient to use in
// unittests.
class FakeScriptExecutorUiDelegate : public ScriptExecutorUiDelegate {
 public:
  FakeScriptExecutorUiDelegate();

  FakeScriptExecutorUiDelegate(const FakeScriptExecutorUiDelegate&) = delete;
  FakeScriptExecutorUiDelegate& operator=(const FakeScriptExecutorUiDelegate&) =
      delete;

  ~FakeScriptExecutorUiDelegate() override;
  void SetStatusMessage(const std::string& message) override;
  std::string GetStatusMessage() const override;
  void SetBubbleMessage(const std::string& message) override;
  std::string GetBubbleMessage() const override;
  void SetTtsMessage(const std::string& message) override;
  std::string GetTtsMessage() const override;
  TtsButtonState GetTtsButtonState() const override;
  void MaybePlayTtsMessage() override;
  void SetDetails(std::unique_ptr<Details> details,
                  base::TimeDelta delay) override;
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
  void SetCollectUserDataOptions(CollectUserDataOptions* options) override;
  void SetCollectUserDataUiState(bool enabled) override;
  void SetLastSuccessfulUserDataOptions(std::unique_ptr<CollectUserDataOptions>
                                            collect_user_data_options) override;
  const CollectUserDataOptions* GetLastSuccessfulUserDataOptions()
      const override;
  void SetPeekMode(ConfigureBottomSheetProto::PeekMode peek_mode) override;
  ConfigureBottomSheetProto::PeekMode GetPeekMode() override;
  void ExpandBottomSheet() override;
  void CollapseBottomSheet() override;
  bool SetForm(
      std::unique_ptr<FormProto> form,
      base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
      base::OnceCallback<void(const ClientStatus&)> cancel_callback) override;
  void SetExpandSheetForPromptAction(bool expand) override;
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
  bool SupportsExternalActions() override;
  void ExecuteExternalAction(
      const external::Action& external_action,
      base::OnceCallback<void(ExternalActionDelegate::ActionResult result)>
          callback) override;

  const std::vector<Details>& GetDetails() { return details_; }

  const GenericUserInterfaceProto* GetPersistentGenericUi() {
    return persistent_generic_ui_.get();
  }

  InfoBox* GetInfoBox() { return info_box_.get(); }

  std::vector<UserAction>* GetUserActions() { return user_actions_.get(); }

  CollectUserDataOptions* GetOptions() { return collect_user_data_options_; }

  bool GetCollectUserDataUiEnabled() { return collect_user_data_ui_enabled_; }

 private:
  std::string status_message_;
  std::string tts_message_;
  std::string bubble_message_;
  std::vector<Details> details_;
  std::unique_ptr<InfoBox> info_box_;
  std::unique_ptr<std::vector<UserAction>> user_actions_;
  std::unique_ptr<CollectUserDataOptions> last_collect_user_data_options_;
  raw_ptr<CollectUserDataOptions> collect_user_data_options_;
  bool collect_user_data_ui_enabled_ = true;
  std::unique_ptr<UserData> payment_request_info_;
  ConfigureBottomSheetProto::PeekMode peek_mode_ =
      ConfigureBottomSheetProto::HANDLE;
  bool expand_or_collapse_updated_ = false;
  bool expand_or_collapse_value_ = false;
  bool expand_sheet_for_prompt_ = true;
  std::unique_ptr<GenericUserInterfaceProto> persistent_generic_ui_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_SCRIPT_EXECUTOR_UI_DELEGATE_H_
