// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/headless/headless_ui_controller.h"

namespace autofill_assistant {

HeadlessUiController::HeadlessUiController(
    ExternalActionDelegate* action_extension_delegate)
    : action_extension_delegate_(action_extension_delegate) {}

bool HeadlessUiController::SupportsExternalActions() {
  return action_extension_delegate_ != nullptr;
}

void HeadlessUiController::ExecuteExternalAction(
    const external::Action& external_action,
    bool is_interrupt,
    base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>
        start_dom_checks_callback,
    base::OnceCallback<void(const external::Result& result)>
        end_action_callback) {
  DCHECK(action_extension_delegate_);

  action_extension_delegate_->OnActionRequested(
      external_action, is_interrupt, std::move(start_dom_checks_callback),
      std::move(end_action_callback));
}

void HeadlessUiController::OnInterruptStarted() {
  if (action_extension_delegate_) {
    action_extension_delegate_->OnInterruptStarted();
  }
}
void HeadlessUiController::OnInterruptFinished() {
  if (action_extension_delegate_) {
    action_extension_delegate_->OnInterruptFinished();
  }
}

// TODO(b/249983799): fail execution instead of just logging a warning if a
// method is unexpectedly called.

void HeadlessUiController::SetStatusMessage(const std::string& message) {}
std::string HeadlessUiController::GetStatusMessage() const {
  VLOG(2) << "Unexpected UI method called: " << __func__;
  return "";
}
void HeadlessUiController::SetBubbleMessage(const std::string& message) {}
std::string HeadlessUiController::GetBubbleMessage() const {
  VLOG(2) << "Unexpected UI method called: " << __func__;
  return "";
}
void HeadlessUiController::SetTtsMessage(const std::string& message) {}
std::string HeadlessUiController::GetTtsMessage() const {
  VLOG(2) << "Unexpected UI method called: " << __func__;
  return "";
}
TtsButtonState HeadlessUiController::GetTtsButtonState() const {
  VLOG(2) << "Unexpected UI method called: " << __func__;
  return TtsButtonState::DEFAULT;
}
ConfigureBottomSheetProto::PeekMode HeadlessUiController::GetPeekMode() {
  VLOG(2) << "Unexpected UI method called: " << __func__;
  return ConfigureBottomSheetProto::UNDEFINED_PEEK_MODE;
}
void HeadlessUiController::MaybePlayTtsMessage() {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
void HeadlessUiController::SetDetails(std::unique_ptr<Details>,
                                      base::TimeDelta delay) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
void HeadlessUiController::AppendDetails(std::unique_ptr<Details> details,
                                         base::TimeDelta delay) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
void HeadlessUiController::SetInfoBox(const InfoBox& info_box) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
void HeadlessUiController::ClearInfoBox() {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
bool HeadlessUiController::SetProgressActiveStepIdentifier(
    const std::string& active_step_identifier) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
  // this returns true so that we just ignore this call instead of failing the
  // SetProgressBar action as INVALID.
  return true;
}
void HeadlessUiController::SetProgressActiveStep(int active_step) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
void HeadlessUiController::SetProgressVisible(bool visible) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
void HeadlessUiController::SetProgressBarErrorState(bool error) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
void HeadlessUiController::SetStepProgressBarConfiguration(
    const ShowProgressBarProto::StepProgressBarConfiguration& configuration) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
void HeadlessUiController::SetUserActions(
    std::unique_ptr<std::vector<UserAction>> user_actions) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
void HeadlessUiController::SetPeekMode(
    ConfigureBottomSheetProto::PeekMode peek_mode) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
void HeadlessUiController::ExpandBottomSheet() {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
void HeadlessUiController::CollapseBottomSheet() {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
bool HeadlessUiController::SetForm(
    std::unique_ptr<FormProto> form,
    base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
    base::OnceCallback<void(const ClientStatus&)> cancel_callback) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
  return false;
}
void HeadlessUiController::ShowQrCodeScanUi(
    std::unique_ptr<PromptQrCodeScanProto> qr_code_scan,
    base::OnceCallback<void(const ClientStatus&,
                            const absl::optional<ValueProto>&)> callback) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}

void HeadlessUiController::ClearQrCodeScanUi() {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
void HeadlessUiController::SetGenericUi(
    std::unique_ptr<GenericUserInterfaceProto> generic_ui,
    base::OnceCallback<void(const ClientStatus&)> end_action_callback,
    base::OnceCallback<void(const ClientStatus&)>
        view_inflation_finished_callback,
    base::RepeatingCallback<void(const RequestBackendDataProto&)>
        request_backend_data_callback,
    base::RepeatingCallback<void(const ShowAccountScreenProto&)>
        show_account_screen_callback) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}

void HeadlessUiController::ShowAccountScreen(
    const ShowAccountScreenProto& proto,
    const std::string& email_address) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}

void HeadlessUiController::SetPersistentGenericUi(
    std::unique_ptr<GenericUserInterfaceProto> generic_ui,
    base::OnceCallback<void(const ClientStatus&)>
        view_inflation_finished_callback) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
void HeadlessUiController::ClearGenericUi() {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
void HeadlessUiController::ClearPersistentGenericUi() {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
void HeadlessUiController::SetShowFeedbackChip(bool show_feedback_chip) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}

void HeadlessUiController::SetExpandSheetForPromptAction(bool expand) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
void HeadlessUiController::SetCollectUserDataOptions(
    CollectUserDataOptions* options) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
void HeadlessUiController::SetCollectUserDataUiState(
    bool loading,
    UserDataEventField event_field) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
void HeadlessUiController::SetLastSuccessfulUserDataOptions(
    std::unique_ptr<CollectUserDataOptions> collect_user_data_options) {
  VLOG(2) << "Unexpected UI method called: " << __func__;
}
const CollectUserDataOptions*
HeadlessUiController::GetLastSuccessfulUserDataOptions() const {
  VLOG(2) << "Unexpected UI method called: " << __func__;
  return nullptr;
}

void HeadlessUiController::OnTouchableAreaChanged(
    const RectF& visual_viewport,
    const std::vector<RectF>& touchable_areas,
    const std::vector<RectF>& restricted_areas) {
  if (action_extension_delegate_) {
    action_extension_delegate_->OnTouchableAreaChanged(
        visual_viewport, touchable_areas, restricted_areas);
  }
}

}  // namespace autofill_assistant
