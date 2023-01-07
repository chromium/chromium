// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/fake_script_executor_ui_delegate.h"

#include <utility>

namespace autofill_assistant {

FakeScriptExecutorUiDelegate::FakeScriptExecutorUiDelegate() {}

FakeScriptExecutorUiDelegate::~FakeScriptExecutorUiDelegate() = default;

void FakeScriptExecutorUiDelegate::SetStatusMessage(
    const std::string& message) {
  status_message_ = message;
}

std::string FakeScriptExecutorUiDelegate::GetStatusMessage() const {
  return status_message_;
}

void FakeScriptExecutorUiDelegate::SetBubbleMessage(
    const std::string& message) {
  bubble_message_ = message;
}

std::string FakeScriptExecutorUiDelegate::GetBubbleMessage() const {
  return bubble_message_;
}

void FakeScriptExecutorUiDelegate::SetTtsMessage(const std::string& message) {
  tts_message_ = message;
}

std::string FakeScriptExecutorUiDelegate::GetTtsMessage() const {
  return tts_message_;
}

TtsButtonState FakeScriptExecutorUiDelegate::GetTtsButtonState() const {
  return TtsButtonState::DEFAULT;
}

void FakeScriptExecutorUiDelegate::MaybePlayTtsMessage() {}

void FakeScriptExecutorUiDelegate::SetDetails(std::unique_ptr<Details> details,
                                              base::TimeDelta delay) {
  // We ignore |delay|.
  if (details) {
    details_ = {*details};
  } else {
    details_ = {};
  }
}

void FakeScriptExecutorUiDelegate::AppendDetails(
    std::unique_ptr<Details> details,
    base::TimeDelta delay) {
  // We ignore |delay|.
  if (details) {
    details_.push_back(*details);
  }
}

void FakeScriptExecutorUiDelegate::SetInfoBox(const InfoBox& info_box) {
  info_box_ = std::make_unique<InfoBox>(info_box);
}

void FakeScriptExecutorUiDelegate::ClearInfoBox() {
  info_box_ = nullptr;
}

bool FakeScriptExecutorUiDelegate::SetProgressActiveStepIdentifier(
    const std::string& active_step_identifier) {
  return true;
}

void FakeScriptExecutorUiDelegate::SetProgressActiveStep(int active_step) {}

void FakeScriptExecutorUiDelegate::SetProgressVisible(bool visible) {}

void FakeScriptExecutorUiDelegate::SetProgressBarErrorState(bool error) {}

void FakeScriptExecutorUiDelegate::SetStepProgressBarConfiguration(
    const ShowProgressBarProto::StepProgressBarConfiguration& configuration) {}

void FakeScriptExecutorUiDelegate::SetUserActions(
    std::unique_ptr<std::vector<UserAction>> user_actions) {
  user_actions_ = std::move(user_actions);
}

void FakeScriptExecutorUiDelegate::SetCollectUserDataOptions(
    CollectUserDataOptions* options) {
  collect_user_data_options_ = options;
}

void FakeScriptExecutorUiDelegate::SetCollectUserDataUiState(
    bool loading,
    UserDataEventField event_field) {
  collect_user_data_ui_loading__field_ = event_field;
}

void FakeScriptExecutorUiDelegate::SetLastSuccessfulUserDataOptions(
    std::unique_ptr<CollectUserDataOptions> collect_user_data_options) {
  last_collect_user_data_options_ = std::move(collect_user_data_options);
}

const CollectUserDataOptions*
FakeScriptExecutorUiDelegate::GetLastSuccessfulUserDataOptions() const {
  return last_collect_user_data_options_.get();
}

void FakeScriptExecutorUiDelegate::SetPeekMode(
    ConfigureBottomSheetProto::PeekMode peek_mode) {
  peek_mode_ = peek_mode;
}

ConfigureBottomSheetProto::PeekMode
FakeScriptExecutorUiDelegate::GetPeekMode() {
  return peek_mode_;
}

void FakeScriptExecutorUiDelegate::ExpandBottomSheet() {
  expand_or_collapse_updated_ = true;
  expand_or_collapse_value_ = true;
}

void FakeScriptExecutorUiDelegate::CollapseBottomSheet() {
  expand_or_collapse_updated_ = true;
  expand_or_collapse_value_ = false;
}

void FakeScriptExecutorUiDelegate::SetExpandSheetForPromptAction(bool expand) {
  expand_sheet_for_prompt_ = expand;
}

bool FakeScriptExecutorUiDelegate::SetForm(
    std::unique_ptr<FormProto> form,
    base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
    base::OnceCallback<void(const ClientStatus&)> cancel_callback) {
  return true;
}

void FakeScriptExecutorUiDelegate::ShowQrCodeScanUi(
    std::unique_ptr<PromptQrCodeScanProto> qr_code_scan,
    base::OnceCallback<void(const ClientStatus&,
                            const absl::optional<ValueProto>&)> callback) {
  show_qr_code_scan_ui_ = true;
}

void FakeScriptExecutorUiDelegate::ClearQrCodeScanUi() {
  show_qr_code_scan_ui_ = false;
}

void FakeScriptExecutorUiDelegate::SetGenericUi(
    std::unique_ptr<GenericUserInterfaceProto> generic_ui,
    base::OnceCallback<void(const ClientStatus&)> end_action_callback,
    base::OnceCallback<void(const ClientStatus&)>
        view_inflation_finished_callback,
    base::RepeatingCallback<void(const RequestBackendDataProto&)>
        request_backend_data_callback,
    base::RepeatingCallback<void(const ShowAccountScreenProto&)>
        show_account_screen_callback) {
  generic_ui_ = std::move(generic_ui);
  end_action_callback_ = std::move(end_action_callback);
  view_inflation_finished_callback_ =
      std::move(view_inflation_finished_callback);
  request_backend_data_callback_ = std::move(request_backend_data_callback);
  show_account_screen_callback_ = std::move(show_account_screen_callback);
}

void FakeScriptExecutorUiDelegate::ShowAccountScreen(
    const ShowAccountScreenProto& proto,
    const std::string& email_address) {
  show_account_screen_proto_ = proto;
  user_email_ = email_address;
}

void FakeScriptExecutorUiDelegate::SetPersistentGenericUi(
    std::unique_ptr<GenericUserInterfaceProto> generic_ui,
    base::OnceCallback<void(const ClientStatus&)>
        view_inflation_finished_callback) {
  persistent_generic_ui_ = std::move(generic_ui);
  view_inflation_finished_callback_ =
      std::move(view_inflation_finished_callback);
}

void FakeScriptExecutorUiDelegate::ClearGenericUi() {}

void FakeScriptExecutorUiDelegate::ClearPersistentGenericUi() {
  persistent_generic_ui_.reset();
}

void FakeScriptExecutorUiDelegate::SetShowFeedbackChip(
    bool show_feedback_chip) {}

bool FakeScriptExecutorUiDelegate::SupportsExternalActions() {
  return true;
}

void FakeScriptExecutorUiDelegate::ExecuteExternalAction(
    const external::Action& external_action,
    bool is_interrupt,
    base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>
        start_dom_checks_callback,
    base::OnceCallback<void(const external::Result& result)>
        end_action_callback) {
  external::Result result;
  result.set_success(true);
  std::move(end_action_callback).Run(result);
}

void FakeScriptExecutorUiDelegate::OnInterruptStarted() {
  interrupt_notification_history_.emplace_back(INTERRUPT_STARTED);
}

void FakeScriptExecutorUiDelegate::OnInterruptFinished() {
  interrupt_notification_history_.emplace_back(INTERRUPT_FINISHED);
}

}  // namespace autofill_assistant
