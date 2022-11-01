// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/ui_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill_assistant/browser/actions/collect_user_data_action.h"
#include "components/autofill_assistant/browser/controller_observer.h"
#include "components/autofill_assistant/browser/display_strings_util.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/service/service_impl.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/view_layout.pb.h"
#include "components/google/core/common/google_util.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace autofill_assistant {
namespace {

// Experiment for non-sticky TTSButtonState. The TTSButtonState is reset to
// DEFAULT whenever tts/status message changes even when the button was
// DISABLED by the user tap.
const char kNonStickyTtsButtonStateExperiment[] = "4624822";

bool ShouldShowFeedbackChipForReason(Metrics::DropOutReason reason) {
  switch (reason) {
    case Metrics::DropOutReason::GET_SCRIPTS_FAILED:
    case Metrics::DropOutReason::GET_SCRIPTS_UNPARSABLE:
    case Metrics::DropOutReason::SCRIPT_FAILED:
    case Metrics::DropOutReason::MULTIPLE_AUTOSTARTABLE_SCRIPTS:
    case Metrics::DropOutReason::NO_INITIAL_SCRIPTS:
    case Metrics::DropOutReason::NAVIGATION:
    case Metrics::DropOutReason::NAVIGATION_WHILE_RUNNING:
    case Metrics::DropOutReason::DOMAIN_CHANGE_DURING_BROWSE_MODE:
    case Metrics::DropOutReason::CERTIFICATE_ERROR:
      return true;
    // These are possible error reasons for which we don't want to show the
    // feedback chip.
    case Metrics::DropOutReason::NO_SCRIPTS:
    case Metrics::DropOutReason::TAB_CHANGED:
    // These are currently not used as error reasons by the Controller and we
    // default to false.
    case Metrics::DropOutReason::AA_START:
    case Metrics::DropOutReason::AUTOSTART_TIMEOUT:
    case Metrics::DropOutReason::CUSTOM_TAB_CLOSED:
    case Metrics::DropOutReason::DECLINED:
    case Metrics::DropOutReason::SHEET_CLOSED:
    case Metrics::DropOutReason::OVERLAY_STOP:
    case Metrics::DropOutReason::PR_FAILED:
    case Metrics::DropOutReason::CONTENT_DESTROYED:
    case Metrics::DropOutReason::RENDER_PROCESS_GONE:
    case Metrics::DropOutReason::INTERSTITIAL_PAGE:
    case Metrics::DropOutReason::SCRIPT_SHUTDOWN:
    case Metrics::DropOutReason::SAFETY_NET_TERMINATE:
    case Metrics::DropOutReason::TAB_DETACHED:
    case Metrics::DropOutReason::DFM_INSTALL_FAILED:
    case Metrics::DropOutReason::BACK_BUTTON_CLICKED:
    case Metrics::DropOutReason::ONBOARDING_BACK_BUTTON_CLICKED:
    case Metrics::DropOutReason::UI_CLOSED_UNEXPECTEDLY:
    case Metrics::DropOutReason::ONBOARDING_NAVIGATION:
    case Metrics::DropOutReason::ONBOARDING_DIALOG_DISMISSED:
      return false;
  }
}

bool ShouldReloadData(const CollectUserDataOptions& options,
                      UserDataEventType event_type) {
  if (!options.use_alternative_edit_dialogs) {
    return false;
  }
  switch (event_type) {
    case UserDataEventType::ENTRY_CREATED:
    case UserDataEventType::ENTRY_EDITED:
      return true;
    case UserDataEventType::UNKNOWN:
    case UserDataEventType::NO_NOTIFICATION:
    case UserDataEventType::SELECTION_CHANGED:
      return false;
  }
}

bool ShouldStoreTemporaryData(const CollectUserDataOptions& options,
                              UserDataEventType event_type) {
  if (!options.use_alternative_edit_dialogs) {
    return false;
  }
  switch (event_type) {
    case UserDataEventType::ENTRY_CREATED:
    case UserDataEventType::ENTRY_EDITED:
      return true;
    case UserDataEventType::UNKNOWN:
    case UserDataEventType::NO_NOTIFICATION:
    case UserDataEventType::SELECTION_CHANGED:
      return false;
  }
}

}  // namespace

UiController::UiController(
    Client* client,
    ExecutionDelegate* execution_delegate,
    std::unique_ptr<AutofillAssistantTtsController> tts_controller)
    : client_(client),
      execution_delegate_(execution_delegate),
      tts_controller_(std::move(tts_controller)) {
  tts_controller_->SetTtsEventDelegate(weak_ptr_factory_.GetWeakPtr());
}

UiController::~UiController() {
  if (execution_delegate_) {
    GetUserModel()->RemoveObserver(this);
    execution_delegate_->RemoveObserver(this);
  }
}

void UiController::StartListening() {
  GetUserModel()->AddObserver(this);
  execution_delegate_->AddObserver(this);
}

UiController::DetailsHolder::DetailsHolder(
    std::unique_ptr<Details> details,
    std::unique_ptr<base::OneShotTimer> timer)
    : details_(std::move(details)), timer_(std::move(timer)) {}

UiController::DetailsHolder::~DetailsHolder() = default;
UiController::DetailsHolder::DetailsHolder(DetailsHolder&& other) = default;
UiController::DetailsHolder& UiController::DetailsHolder::operator=(
    DetailsHolder&& other) = default;

const Details& UiController::DetailsHolder::GetDetails() const {
  return *details_;
}

bool UiController::DetailsHolder::CurrentlyVisible() const {
  // If there is a timer associated to these details, then they should be shown
  // only once the timer has triggered.
  return !timer_;
}

void UiController::DetailsHolder::Enable() {
  timer_.reset();
}

std::string UiController::GetDisplayStringsLocale() {
  if (execution_delegate_->GetClientSettings().display_strings_locale.empty()) {
    // Fallback locale
    return client_->GetLocale();
  }
  return execution_delegate_->GetClientSettings().display_strings_locale;
}

void UiController::SetStatusMessage(const std::string& message) {
  status_message_ = message;
  for (UiControllerObserver& observer : observers_) {
    observer.OnStatusMessageChanged(message);
  }

  // Override tts_message every time status_message changes.
  SetTtsMessage(message);
}

std::string UiController::GetStatusMessage() const {
  return status_message_;
}

void UiController::SetBubbleMessage(const std::string& message) {
  bubble_message_ = message;
  for (UiControllerObserver& observer : observers_) {
    observer.OnBubbleMessageChanged(message);
  }
}

std::string UiController::GetBubbleMessage() const {
  return bubble_message_;
}

void UiController::SetTtsMessage(const std::string& message) {
  tts_message_ = message;

  // Stop any ongoing TTS and reset button state.
  if (tts_button_state_ == TtsButtonState::PLAYING) {
    // Will not cause any TTS event.
    tts_controller_->Stop();
    SetTtsButtonState(TtsButtonState::DEFAULT);
  }

  // Re-enable TTS button if "Non sticky Tts Button State" experiment is
  // enabled.
  if (tts_button_state_ == TtsButtonState::DISABLED &&
      execution_delegate_->GetTriggerContext() != nullptr &&
      execution_delegate_->GetTriggerContext()->HasExperimentId(
          kNonStickyTtsButtonStateExperiment)) {
    SetTtsButtonState(TtsButtonState::DEFAULT);
  }
}

std::string UiController::GetTtsMessage() const {
  return tts_message_;
}

void UiController::MaybePlayTtsMessage() {
  if (!tts_enabled_) {
    return;
  }

  // Will fire a TTS_START event.
  tts_controller_->Speak(tts_message_, GetDisplayStringsLocale());
}

void UiController::SetDetails(std::unique_ptr<Details> details,
                              base::TimeDelta delay) {
  details_.clear();

  // There is nothing to append: notify that we cleared the details and return.
  if (!details) {
    NotifyDetailsChanged();
    return;
  }

  // If there is a delay, notify now that details have been cleared. If there is
  // no delay, AppendDetails will take care of the notifying the observers after
  // appending the details.
  if (!delay.is_zero()) {
    NotifyDetailsChanged();
  }

  AppendDetails(std::move(details), delay);
}

void UiController::AppendDetails(std::unique_ptr<Details> details,
                                 base::TimeDelta delay) {
  if (!details) {
    return;
  }

  if (delay.is_zero()) {
    details_.push_back(DetailsHolder(std::move(details), /* timer= */ nullptr));
    NotifyDetailsChanged();
    return;
  }

  // Delay the addition of the new details.
  size_t details_index = details_.size();
  auto timer = std::make_unique<base::OneShotTimer>();
  timer->Start(FROM_HERE, delay,
               base::BindOnce(&UiController::MakeDetailsVisible,
                              weak_ptr_factory_.GetWeakPtr(), details_index));
  details_.push_back(DetailsHolder(std::move(details), std::move(timer)));
}

void UiController::MakeDetailsVisible(size_t details_index) {
  if (details_index < details_.size()) {
    details_[details_index].Enable();
    NotifyDetailsChanged();
  }
}

void UiController::NotifyDetailsChanged() {
  std::vector<Details> details = GetDetails();
  for (UiControllerObserver& observer : observers_) {
    observer.OnDetailsChanged(details);
  }
}

std::vector<Details> UiController::GetDetails() const {
  std::vector<Details> details;
  for (const auto& holder : details_) {
    if (holder.CurrentlyVisible()) {
      details.push_back(holder.GetDetails());
    }
  }
  return details;
}

int UiController::GetProgressActiveStep() const {
  return progress_active_step_;
}

ShowProgressBarProto::StepProgressBarConfiguration
UiController::GetStepProgressBarConfiguration() const {
  return step_progress_bar_configuration_;
}

void UiController::SetInfoBox(const InfoBox& info_box) {
  if (!info_box_) {
    info_box_ = std::make_unique<InfoBox>();
  }
  *info_box_ = info_box;
  for (UiControllerObserver& observer : observers_) {
    observer.OnInfoBoxChanged(info_box_.get());
  }
}

void UiController::ClearInfoBox() {
  info_box_.reset();
  for (UiControllerObserver& observer : observers_) {
    observer.OnInfoBoxChanged(nullptr);
  }
}

const InfoBox* UiController::GetInfoBox() const {
  return info_box_.get();
}

bool UiController::SetProgressActiveStepIdentifier(
    const std::string& active_step_identifier) {
  const auto it = base::ranges::find(
      step_progress_bar_configuration_.annotated_step_icons(),
      active_step_identifier,
      &ShowProgressBarProto::StepProgressBarIcon::identifier);
  if (it == step_progress_bar_configuration_.annotated_step_icons().cend()) {
    return false;
  }

  SetProgressActiveStep(std::distance(
      step_progress_bar_configuration_.annotated_step_icons().cbegin(), it));
  return true;
}

void UiController::SetProgressActiveStep(int active_step) {
  // Default step progress bar has 2 steps.
  int max_step = std::max(
      2, step_progress_bar_configuration_.annotated_step_icons().size());

  int new_active_step = active_step;
  if (active_step < 0 || active_step > max_step) {
    new_active_step = max_step;
  }

  // Step can only increase.
  if (progress_active_step_ >= new_active_step) {
    return;
  }

  progress_active_step_ = new_active_step;
  for (UiControllerObserver& observer : observers_) {
    observer.OnProgressActiveStepChanged(new_active_step);
  }
}

void UiController::SetProgressVisible(bool visible) {
  if (progress_visible_ == visible)
    return;

  progress_visible_ = visible;
  for (UiControllerObserver& observer : observers_) {
    observer.OnProgressVisibilityChanged(visible);
  }
}

bool UiController::GetProgressVisible() const {
  return progress_visible_;
}

bool UiController::GetTtsButtonVisible() const {
  return tts_enabled_;
}

bool UiController::GetDisableScrollbarFading() const {
  return disable_scrollbar_fading_;
}

TtsButtonState UiController::GetTtsButtonState() const {
  return tts_button_state_;
}

void UiController::SetStepProgressBarConfiguration(
    const ShowProgressBarProto::StepProgressBarConfiguration& configuration) {
  step_progress_bar_configuration_ = configuration;
  if (!configuration.annotated_step_icons().empty() &&
      configuration.annotated_step_icons().size() < progress_active_step_) {
    progress_active_step_ = configuration.annotated_step_icons().size();
  }
  for (UiControllerObserver& observer : observers_) {
    observer.OnStepProgressBarConfigurationChanged(configuration);
    observer.OnProgressActiveStepChanged(progress_active_step_);
    observer.OnProgressBarErrorStateChanged(progress_bar_error_state_);
  }
}

void UiController::SetProgressBarErrorState(bool error) {
  if (progress_bar_error_state_ == error) {
    return;
  }

  progress_bar_error_state_ = error;
  for (UiControllerObserver& observer : observers_) {
    observer.OnProgressBarErrorStateChanged(error);
  }
}

bool UiController::GetProgressBarErrorState() const {
  return progress_bar_error_state_;
}

const std::vector<UserAction>& UiController::GetUserActions() const {
  static const base::NoDestructor<std::vector<UserAction>> no_user_actions_;
  return user_actions_ ? *user_actions_ : *no_user_actions_;
}

void UiController::SetUserActions(
    std::unique_ptr<std::vector<UserAction>> user_actions) {
  if (user_actions) {
    SetDefaultChipType(user_actions.get());
  }
  user_actions_ = std::move(user_actions);
  SetVisibilityAndUpdateUserActions();
}

void UiController::SetLegalDisclaimer(
    std::unique_ptr<LegalDisclaimerProto> legal_disclaimer,
    base::OnceCallback<void(int)> legal_disclaimer_link_callback) {
  legal_disclaimer_link_callback_.Reset();
  if (legal_disclaimer) {
    legal_disclaimer_link_callback_ = std::move(legal_disclaimer_link_callback);
  }
  legal_disclaimer_ = std::move(legal_disclaimer);

  for (UiControllerObserver& observer : observers_) {
    observer.OnLegalDisclaimerChanged(legal_disclaimer_.get());
  }
}

bool UiController::ShouldChipsBeVisible() {
  return !(is_keyboard_showing_ && is_focus_on_bottom_sheet_text_input_);
}

bool UiController::ShouldUpdateChipVisibility() {
  return are_chips_visible_ != ShouldChipsBeVisible();
}

void UiController::SetVisibilityAndUpdateUserActions() {
  // All non-cancel chips should be hidden while the keyboard is showing to fill
  // an input text field in the bottom sheet.
  are_chips_visible_ = ShouldChipsBeVisible();
  if (user_actions_) {
    for (UserAction& user_action : *user_actions_) {
      if (user_action.chip().type != CANCEL_ACTION) {
        user_action.chip().visible = are_chips_visible_;
      }
    }
  }

  for (UiControllerObserver& observer : observers_) {
    observer.OnUserActionsChanged(GetUserActions());
  }
}

void UiController::ShowQrCodeScanUi(
    std::unique_ptr<PromptQrCodeScanProto> qr_code_scan,
    base::OnceCallback<void(const ClientStatus&,
                            const absl::optional<ValueProto>&)> callback) {
  qr_code_scan_ = std::move(qr_code_scan);
  qr_code_scan_callback_ = std::move(callback);
  for (UiControllerObserver& observer : observers_) {
    observer.OnQrCodeScanUiChanged(qr_code_scan_.get());
  }
}

void UiController::ClearQrCodeScanUi() {
  qr_code_scan_.reset();
  qr_code_scan_callback_ = base::DoNothing();
  for (UiControllerObserver& observer : observers_) {
    observer.OnQrCodeScanUiChanged(nullptr);
  }
}

void UiController::SetGenericUi(
    std::unique_ptr<GenericUserInterfaceProto> generic_ui,
    base::OnceCallback<void(const ClientStatus&)> end_action_callback,
    base::OnceCallback<void(const ClientStatus&)>
        view_inflation_finished_callback,
    base::RepeatingCallback<void(const RequestBackendDataProto&)>
        request_backend_data_callback,
    base::RepeatingCallback<void(const ShowAccountScreenProto&)>
        show_account_screen_callback) {
  generic_user_interface_ = std::move(generic_ui);
  basic_interactions_.SetEndActionCallback(std::move(end_action_callback));
  basic_interactions_.SetViewInflationFinishedCallback(
      std::move(view_inflation_finished_callback));
  basic_interactions_.SetRequestBackendDataCallback(
      std::move(request_backend_data_callback));
  basic_interactions_.SetShowAccountScreenCallback(
      std::move(show_account_screen_callback));
  for (UiControllerObserver& observer : observers_) {
    observer.OnGenericUserInterfaceChanged(generic_user_interface_.get());
  }
}

void UiController::ShowAccountScreen(const ShowAccountScreenProto& proto,
                                     const std::string& email_address) {
  for (UiControllerObserver& observer : observers_) {
    observer.OnShowAccountScreen(proto, email_address);
  }
}

void UiController::SetPersistentGenericUi(
    std::unique_ptr<GenericUserInterfaceProto> generic_ui,
    base::OnceCallback<void(const ClientStatus&)>
        view_inflation_finished_callback) {
  persistent_generic_user_interface_ = std::move(generic_ui);
  basic_interactions_.SetPersistentViewInflationFinishedCallback(
      std::move(view_inflation_finished_callback));
  for (UiControllerObserver& observer : observers_) {
    observer.OnPersistentGenericUserInterfaceChanged(
        persistent_generic_user_interface_.get());
  }
}

void UiController::ClearGenericUi() {
  generic_user_interface_.reset();
  basic_interactions_.ClearCallbacks();
  for (UiControllerObserver& observer : observers_) {
    observer.OnGenericUserInterfaceChanged(nullptr);
  }
}

void UiController::ClearPersistentGenericUi() {
  persistent_generic_user_interface_.reset();
  basic_interactions_.ClearPersistentUiCallbacks();
  for (UiControllerObserver& observer : observers_) {
    observer.OnPersistentGenericUserInterfaceChanged(nullptr);
  }
}

void UiController::SetShowFeedbackChip(bool show_feedback_chip) {
  show_feedback_chip_ = show_feedback_chip;
}

void UiController::SetExpandSheetForPromptAction(bool expand) {
  expand_sheet_for_prompt_action_ = expand;
}

bool UiController::PerformUserAction(int index) {
  if (!user_actions_ || index < 0 ||
      static_cast<size_t>(index) >= user_actions_->size()) {
    NOTREACHED() << "Invalid user_action index: " << index;
    return false;
  }

  if (!(*user_actions_)[index].enabled()) {
    NOTREACHED() << "Action at index " << index << " is disabled.";
    return false;
  }

  UserAction user_action = std::move((*user_actions_)[index]);

  if (base::FeatureList::IsEnabled(features::kAutofillAssistantFastShutdown) &&
      user_action.has_chip() &&
      (user_action.chip().type == DONE_ACTION ||
       user_action.chip().type == CLOSE_ACTION)) {
    // Special case: we assume that tapping the DONE or CLOSE chip signals
    // script shutdown, and we handle it specially to streamline how this looks.
    // See also b/233063571 for details.
    EnterBrowseModeForShutdown();
  }
  SetUserActions(nullptr);

  user_action.RunCallback();
  event_handler_.DispatchEvent(
      {EventProto::kOnUserActionCalled, user_action.identifier()});
  return true;
}

void UiController::SetPeekMode(ConfigureBottomSheetProto::PeekMode peek_mode) {
  if (peek_mode == peek_mode_)
    return;

  peek_mode_ = peek_mode;
  for (UiControllerObserver& observer : observers_) {
    observer.OnPeekModeChanged(peek_mode);
  }
}

void UiController::ExpandBottomSheet() {
  for (UiControllerObserver& observer : observers_) {
    // TODO(crbug/806868): The interface here and in some of the other On*
    // events should be coming from the UI layer, not the controller. Or at
    // least be renamed to something like On*Requested.
    observer.OnExpandBottomSheet();
  }
}

void UiController::CollapseBottomSheet() {
  for (UiControllerObserver& observer : observers_) {
    // TODO(crbug/806868): The interface here and in some of the other On*
    // events should be coming from the UI layer, not the controller. Or at
    // least be renamed to something like On*Requested.
    observer.OnCollapseBottomSheet();
  }
}

const LegalDisclaimerProto* UiController::GetLegalDisclaimer() const {
  return legal_disclaimer_.get();
}

const FormProto* UiController::GetForm() const {
  return form_.get();
}

const FormProto::Result* UiController::GetFormResult() const {
  return form_result_.get();
}

bool UiController::SetForm(
    std::unique_ptr<FormProto> form,
    base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
    base::OnceCallback<void(const ClientStatus&)> cancel_callback) {
  form_.reset();
  form_result_.reset();
  form_changed_callback_ = base::DoNothing();
  form_cancel_callback_ = base::DoNothing();

  if (!form) {
    for (UiControllerObserver& observer : observers_) {
      observer.OnFormChanged(nullptr, nullptr);
    }
    return true;
  }

  // Initialize form result. This will return false if the form is invalid or
  // contains unsupported inputs.
  auto form_result = std::make_unique<FormProto::Result>();
  for (FormInputProto& input : *form->mutable_inputs()) {
    FormInputProto::Result* result = form_result->add_input_results();
    switch (input.input_type_case()) {
      case FormInputProto::InputTypeCase::kCounter:
        // Add the initial value of each counter into the form result.
        for (const CounterInputProto::Counter& counter :
             input.counter().counters()) {
          result->mutable_counter()->add_values(counter.initial_value());
        }
        break;
      case FormInputProto::InputTypeCase::kSelection: {
        // Add the initial selected state of each choice into the form result.
        bool has_selected = false;
        for (const SelectionInputProto::Choice& choice :
             input.selection().choices()) {
          if (choice.selected()) {
            if (has_selected && !input.selection().allow_multiple()) {
              // Multiple choices are initially selected even though it is not
              // allowed by the input.
              return false;
            }
            has_selected = true;
          }
          result->mutable_selection()->add_selected(choice.selected());
        }
        break;
      }
      case FormInputProto::InputTypeCase::INPUT_TYPE_NOT_SET:
        VLOG(1) << "Encountered input with INPUT_TYPE_NOT_SET";
        return false;
        // Intentionally no default case to make compilation fail if a new
        // value was added to the enum but not to this list.
    }
  }

  // Form is valid.
  form_ = std::move(form);
  form_result_ = std::move(form_result);
  form_changed_callback_ = changed_callback;
  form_cancel_callback_ = std::move(cancel_callback);

  // Call the callback with initial result.
  form_changed_callback_.Run(form_result_.get());

  for (UiControllerObserver& observer : observers_) {
    observer.OnFormChanged(form_.get(), form_result_.get());
  }
  return true;
}

void UiController::SetCounterValue(int input_index,
                                   int counter_index,
                                   int value) {
  if (!form_result_ || input_index < 0 ||
      input_index >= form_result_->input_results_size()) {
    NOTREACHED() << "Invalid input index: " << input_index;
    return;
  }

  FormInputProto::Result* input_result =
      form_result_->mutable_input_results(input_index);
  if (!input_result->has_counter() || counter_index < 0 ||
      counter_index >= input_result->counter().values_size()) {
    NOTREACHED() << "Invalid counter index: " << counter_index;
    return;
  }

  input_result->mutable_counter()->set_values(counter_index, value);
  form_changed_callback_.Run(form_result_.get());
}

void UiController::SetChoiceSelected(int input_index,
                                     int choice_index,
                                     bool selected) {
  if (!form_result_ || input_index < 0 ||
      input_index >= form_result_->input_results_size()) {
    NOTREACHED() << "Invalid input index: " << input_index;
    return;
  }

  FormInputProto::Result* input_result =
      form_result_->mutable_input_results(input_index);
  if (!input_result->has_selection() || choice_index < 0 ||
      choice_index >= input_result->selection().selected_size()) {
    NOTREACHED() << "Invalid choice index: " << choice_index;
    return;
  }

  input_result->mutable_selection()->set_selected(choice_index, selected);
  form_changed_callback_.Run(form_result_.get());
}

UserModel* UiController::GetUserModel() {
  return execution_delegate_->GetUserModel();
}

EventHandler* UiController::GetEventHandler() {
  return &event_handler_;
}

bool UiController::ShouldPromptActionExpandSheet() const {
  return expand_sheet_for_prompt_action_;
}

BasicInteractions* UiController::GetBasicInteractions() {
  return &basic_interactions_;
}

const PromptQrCodeScanProto* UiController::GetPromptQrCodeScanProto() const {
  return qr_code_scan_.get();
}

const GenericUserInterfaceProto* UiController::GetGenericUiProto() const {
  return generic_user_interface_.get();
}

const GenericUserInterfaceProto* UiController::GetPersistentGenericUiProto()
    const {
  return persistent_generic_user_interface_.get();
}

void UiController::AddObserver(UiControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void UiController::RemoveObserver(const UiControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void UiController::DispatchEvent(const EventHandler::EventKey& key) {
  event_handler_.DispatchEvent(key);
}

ConfigureBottomSheetProto::PeekMode UiController::GetPeekMode() {
  return peek_mode_;
}

BottomSheetState UiController::GetBottomSheetState() {
  return bottom_sheet_state_;
}

void UiController::SetBottomSheetState(BottomSheetState state) {
  bottom_sheet_state_ = state;
}
base::Value UiController::GetDebugContext() const {
  base::Value::Dict dict;

  dict.Set("status", status_message_);

  base::Value::List details_list;
  for (const auto& holder : details_) {
    details_list.Append(holder.GetDetails().GetDebugContext());
  }
  dict.Set("details", std::move(details_list));

  return base::Value(std::move(dict));
}

const CollectUserDataOptions* UiController::GetCollectUserDataOptions() const {
  return collect_user_data_options_;
}

UserData* UiController::GetUserData() {
  return execution_delegate_->GetUserData();
}

void UiController::OnCollectUserDataContinueButtonClicked() {
  if (!collect_user_data_options_)
    return;

  auto callback = std::move(collect_user_data_options_->confirm_callback);

  SetCollectUserDataOptions(nullptr);
  std::move(callback).Run(GetUserData(), GetUserModel());
}

void UiController::OnCollectUserDataAdditionalActionTriggered(int index) {
  if (!collect_user_data_options_)
    return;

  auto callback =
      std::move(collect_user_data_options_->additional_actions_callback);
  SetCollectUserDataOptions(nullptr);
  std::move(callback).Run(index, GetUserData(), GetUserModel());
}

void UiController::OnTextLinkClicked(int link) {
  auto callback = std::move(collect_user_data_options_->terms_link_callback);
  SetCollectUserDataOptions(nullptr);
  std::move(callback).Run(link, GetUserData(), GetUserModel());
}

void UiController::OnFormActionLinkClicked(int link) {
  if (form_cancel_callback_ && form_result_ != nullptr) {
    form_result_->set_link(link);
    form_changed_callback_.Run(form_result_.get());
    std::move(form_cancel_callback_).Run(ClientStatus(ACTION_APPLIED));
  }
}

void UiController::OnLegalDisclaimerLinkClicked(int link) {
  if (legal_disclaimer_link_callback_) {
    std::move(legal_disclaimer_link_callback_).Run(link);
  }
}

void UiController::OnTtsButtonClicked() {
  switch (tts_button_state_) {
    case TtsButtonState::DEFAULT:
      // Will fire a TTS_START event.
      tts_controller_->Speak(tts_message_, GetDisplayStringsLocale());
      Metrics::RecordTtsButtonAction(Metrics::TtsButtonAction::PLAY_TTS);
      break;
    case TtsButtonState::PLAYING:
      // Will not cause any TTS event.
      tts_controller_->Stop();
      SetTtsButtonState(TtsButtonState::DISABLED);
      Metrics::RecordTtsButtonAction(Metrics::TtsButtonAction::DISABLE_BUTTON);
      break;
    case TtsButtonState::DISABLED:
      SetTtsButtonState(TtsButtonState::DEFAULT);
      // Will fire a TTS_START event.
      tts_controller_->Speak(tts_message_, GetDisplayStringsLocale());
      Metrics::RecordTtsButtonAction(
          Metrics::TtsButtonAction::ENABLE_BUTTON_AND_PLAY_TTS);
      break;
  }
}

void UiController::OnTtsEvent(
    AutofillAssistantTtsController::TtsEventType event) {
  switch (event) {
    case AutofillAssistantTtsController::TTS_START:
      SetTtsButtonState(TtsButtonState::PLAYING);
      break;
    case AutofillAssistantTtsController::TTS_END:
    case AutofillAssistantTtsController::TTS_ERROR:
      SetTtsButtonState(TtsButtonState::DEFAULT);
      break;
  }
}

void UiController::SetTtsButtonState(TtsButtonState state) {
  tts_button_state_ = state;
  for (UiControllerObserver& observer : observers_) {
    observer.OnTtsButtonStateChanged(tts_button_state_);
  }
}

void UiController::OnSpokenFeedbackAccessibilityServiceChanged(bool enabled) {
  if (!enabled) {
    // Nothing to do when the a11y service is disabled.
    return;
  }

  if (!tts_enabled_) {
    return;
  }
  // Disable TTS and hide TTS button.
  tts_enabled_ = false;
  for (UiControllerObserver& observer : observers_) {
    observer.OnTtsButtonVisibilityChanged(/* visible= */ false);
  }
  // Stop any ongoing TTS and reset button state.
  if (tts_button_state_ == TtsButtonState::PLAYING) {
    // Will not cause any TTS event.
    tts_controller_->Stop();
    SetTtsButtonState(TtsButtonState::DEFAULT);
  }
}

void UiController::SetAdditionalValue(const std::string& client_memory_key,
                                      const ValueProto& value) {
  if (!GetUserData()->HasAdditionalValue(client_memory_key)) {
    NOTREACHED() << client_memory_key << " not found";
    return;
  }
  GetUserData()->SetAdditionalValue(client_memory_key, value);

  execution_delegate_->NotifyUserDataChange(
      UserDataFieldChange::ADDITIONAL_VALUES);
}

void UiController::OnQrCodeScanFinished(
    const ClientStatus& status,
    const absl::optional<ValueProto>& value) {
  if (qr_code_scan_callback_) {
    std::move(qr_code_scan_callback_).Run(status, value);
  }
}

void UiController::HandleShippingAddressChange(
    std::unique_ptr<autofill::AutofillProfile> address,
    UserDataEventType event_type) {
  if (collect_user_data_options_ == nullptr) {
    return;
  }

  collect_user_data_options_->selected_user_data_changed_callback.Run(
      UserDataEventField::SHIPPING_EVENT, event_type);

  if (ShouldReloadData(*collect_user_data_options_, event_type)) {
    ReloadUserData(UserDataEventField::SHIPPING_EVENT, event_type);
    return;
  }

  DCHECK(!collect_user_data_options_->shipping_address_name.empty());
  SetProfile(collect_user_data_options_->shipping_address_name,
             UserDataFieldChange::SHIPPING_ADDRESS, std::move(address));
}

void UiController::HandleContactInfoChange(
    std::unique_ptr<autofill::AutofillProfile> profile,
    UserDataEventType event_type) {
  if (collect_user_data_options_ == nullptr) {
    return;
  }

  collect_user_data_options_->selected_user_data_changed_callback.Run(
      UserDataEventField::CONTACT_EVENT, event_type);

  if (ShouldStoreTemporaryData(*collect_user_data_options_, event_type)) {
    UserData* user_data = GetUserData();
    DCHECK(user_data);
    user_data::UpsertContact(*profile, user_data->transient_contacts_);
  }

  DCHECK(!collect_user_data_options_->contact_details_name.empty());
  SetProfile(collect_user_data_options_->contact_details_name,
             UserDataFieldChange::CONTACT_PROFILE, std::move(profile));
}

void UiController::HandlePhoneNumberChange(
    std::unique_ptr<autofill::AutofillProfile> profile,
    UserDataEventType event_type) {
  if (collect_user_data_options_ == nullptr) {
    return;
  }

  collect_user_data_options_->selected_user_data_changed_callback.Run(
      UserDataEventField::PHONE_NUMBER_EVENT, event_type);

  if (ShouldStoreTemporaryData(*collect_user_data_options_, event_type)) {
    UserData* user_data = GetUserData();
    DCHECK(user_data);
    user_data::UpsertPhoneNumber(*profile, user_data->transient_phone_numbers_);
  }

  GetUserData()->SetSelectedPhoneNumber(std::move(profile));
  execution_delegate_->NotifyUserDataChange(UserDataFieldChange::PHONE_NUMBER);
}

void UiController::HandleCreditCardChange(
    std::unique_ptr<autofill::CreditCard> card,
    std::unique_ptr<autofill::AutofillProfile> billing_profile,
    UserDataEventType event_type) {
  if (collect_user_data_options_ == nullptr) {
    return;
  }

  collect_user_data_options_->selected_user_data_changed_callback.Run(
      UserDataEventField::CREDIT_CARD_EVENT, event_type);

  if (ShouldReloadData(*collect_user_data_options_, event_type)) {
    ReloadUserData(UserDataEventField::CREDIT_CARD_EVENT, event_type);
    return;
  }

  DCHECK(!collect_user_data_options_->billing_address_name.empty());
  SetProfile(collect_user_data_options_->billing_address_name,
             UserDataFieldChange::BILLING_ADDRESS, std::move(billing_profile));
  GetUserModel()->SetSelectedCreditCard(std::move(card), GetUserData());
  execution_delegate_->NotifyUserDataChange(UserDataFieldChange::CARD);
}

void UiController::SetProfile(
    const std::string& key,
    UserDataFieldChange field_change,
    std::unique_ptr<autofill::AutofillProfile> profile) {
  GetUserModel()->SetSelectedAutofillProfile(key, std::move(profile),
                                             GetUserData());

  execution_delegate_->NotifyUserDataChange(field_change);
}

void UiController::ReloadUserData(UserDataEventField event_field,
                                  UserDataEventType event_type) {
  DCHECK(collect_user_data_options_);
  std::move(collect_user_data_options_->reload_data_callback)
      .Run(event_field, GetUserData());
}

void UiController::SetTermsAndConditions(
    TermsAndConditionsState terms_and_conditions) {
  GetUserData()->terms_and_conditions_ = terms_and_conditions;
  execution_delegate_->NotifyUserDataChange(
      UserDataFieldChange::TERMS_AND_CONDITIONS);
}

void UiController::SetLoginOption(const std::string& identifier) {
  if (!collect_user_data_options_)
    return;

  GetUserModel()->SetSelectedLoginChoiceByIdentifier(
      identifier, *collect_user_data_options_, GetUserData());

  execution_delegate_->NotifyUserDataChange(UserDataFieldChange::LOGIN_CHOICE);
}

void UiController::UpdateCollectUserDataActions() {
  if (!collect_user_data_options_) {
    SetUserActions(nullptr);
    return;
  }

  bool confirm_button_enabled = CollectUserDataAction::IsUserDataComplete(
      *GetUserData(), *GetUserModel(), *collect_user_data_options_);

  UserAction confirm(collect_user_data_options_->confirm_action);
  confirm.SetEnabled(confirm_button_enabled);
  if (confirm_button_enabled) {
    confirm.SetCallback(
        base::BindOnce(&UiController::OnCollectUserDataContinueButtonClicked,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  auto user_actions = std::make_unique<std::vector<UserAction>>();
  user_actions->emplace_back(std::move(confirm));

  // Add additional actions.
  for (size_t i = 0; i < collect_user_data_options_->additional_actions.size();
       ++i) {
    auto action = collect_user_data_options_->additional_actions[i];
    user_actions->push_back({action});
    user_actions->back().SetCallback(base::BindOnce(
        &UiController::OnCollectUserDataAdditionalActionTriggered,
        weak_ptr_factory_.GetWeakPtr(), i));
  }

  SetUserActions(std::move(user_actions));
}

void UiController::OnValueChanged(const std::string& identifier,
                                  const ValueProto& new_value) {
  event_handler_.DispatchEvent({EventProto::kOnValueChanged, identifier});
  // TODO(b/145043394) Remove this once chips are part of generic UI.
  if (collect_user_data_options_ != nullptr &&
      collect_user_data_options_->additional_model_identifier_to_check
          .has_value() &&
      identifier ==
          *collect_user_data_options_->additional_model_identifier_to_check) {
    UpdateCollectUserDataActions();
  }
}

void UiController::SetCollectUserDataOptions(CollectUserDataOptions* options) {
  DCHECK(!options ||
         (options->confirm_callback && options->additional_actions_callback &&
          options->terms_link_callback));

  if (collect_user_data_options_ == nullptr && options == nullptr)
    return;

  collect_user_data_options_ = options;
  for (UiControllerObserver& observer : observers_) {
    observer.OnCollectUserDataOptionsChanged(collect_user_data_options_);
  }
  execution_delegate_->NotifyUserDataChange(UserDataFieldChange::ALL);
}

void UiController::SetCollectUserDataUiState(bool loading,
                                             UserDataEventField event) {
  for (UiControllerObserver& observer : observers_) {
    observer.OnCollectUserDataUiStateChanged(loading, event);
  }
}

void UiController::SetLastSuccessfulUserDataOptions(
    std::unique_ptr<CollectUserDataOptions> collect_user_data_options) {
  last_collect_user_data_options_ = std::move(collect_user_data_options);
}

const CollectUserDataOptions* UiController::GetLastSuccessfulUserDataOptions()
    const {
  return last_collect_user_data_options_.get();
}

void UiController::OnKeyboardVisibilityChanged(bool visible) {
  is_keyboard_showing_ = visible;

  if (ShouldUpdateChipVisibility()) {
    SetVisibilityAndUpdateUserActions();
  }
}

void UiController::OnInputTextFocusChanged(bool is_text_focused) {
  is_focus_on_bottom_sheet_text_input_ = is_text_focused;

  if (ShouldUpdateChipVisibility()) {
    SetVisibilityAndUpdateUserActions();
  }
}

void UiController::OnFeedbackSent() {
  execution_delegate_->ShutdownIfNecessary();
}

void UiController::OnError(const std::string& error_message,
                           Metrics::DropOutReason reason) {
  show_feedback_chip_ = ShouldShowFeedbackChipForReason(reason);
  SetStatusMessage(error_message);
  SetProgressBarErrorState(true);
}

void UiController::OnUserDataChanged(const UserData& user_data,
                                     UserDataFieldChange field_change) {
  UpdateCollectUserDataActions();
}

void UiController::OnExecuteScript(const std::string& start_message) {
  if (!start_message.empty())
    SetStatusMessage(start_message);

  SetUserActions(nullptr);
}

void UiController::InitFromParameters(const TriggerContext& trigger_context) {
  auto details = std::make_unique<Details>();
  if (details->UpdateFromParameters(trigger_context.GetScriptParameters()))
    SetDetails(std::move(details), base::TimeDelta());

  const bool enable_tts = trigger_context.GetScriptParameters().GetEnableTts();
  if (enable_tts && !client_->IsSpokenFeedbackAccessibilityServiceEnabled()) {
    tts_enabled_ = true;
    for (UiControllerObserver& observer : observers_) {
      observer.OnTtsButtonVisibilityChanged(/* visible= */ true);
    }
  }

  disable_scrollbar_fading_ =
      trigger_context.GetScriptParameters().GetDisableScrollbarFading();
  for (UiControllerObserver& observer : observers_) {
    observer.OnDisableScrollbarFadingChanged(disable_scrollbar_fading_);
  }
}

void UiController::OnStart(const TriggerContext& trigger_context) {
  // Clean up from previous shutdown state, if set. The main use case for this
  // are consecutive direct actions.
  is_shutting_down_ = false;
  SetUserActions(nullptr);

  InitFromParameters(trigger_context);

  // |status_message_| may be non-empty due to a trigger script that was run.
  SetStatusMessage(
      status_message_.empty()
          ? l10n_util::GetStringFUTF8(
                IDS_AUTOFILL_ASSISTANT_LOADING,
                base::UTF8ToUTF16(execution_delegate_->GetCurrentURL().host()))
          : status_message_);

  SetStepProgressBarConfiguration(step_progress_bar_configuration_);
  SetProgressActiveStep(progress_active_step_);
}

void UiController::OnStop() {
  std::unique_ptr<std::vector<UserAction>> final_actions;
  if (base::FeatureList::IsEnabled(features::kAutofillAssistantFeedbackChip) &&
      show_feedback_chip_) {
    final_actions = std::make_unique<std::vector<UserAction>>();
    UserAction feedback_action;
    Chip feedback_chip;
    feedback_chip.type = FEEDBACK_ACTION;
    feedback_chip.text =
        GetDisplayStringUTF8(ClientSettingsProto::SEND_FEEDBACK,
                             execution_delegate_->GetClientSettings());
    feedback_action.SetCallback(base::BindOnce(&UiController::OnFeedbackSent,
                                               weak_ptr_factory_.GetWeakPtr()));
    feedback_action.chip() = feedback_chip;
    final_actions->emplace_back(std::move(feedback_action));
  }

  ClearInfoBox();
  SetDetails(nullptr, base::TimeDelta());
  SetUserActions(std::move(final_actions));
  SetCollectUserDataOptions(nullptr);
  SetForm(nullptr, base::DoNothing(), base::DoNothing());
}

void UiController::OnResetState() {
  // TODO(b/204963552): this list is incomplete. It would be much better if,
  // instead of selectively clearing fields, we'd solve this in a more holistic
  // way.
  bubble_message_.clear();
  tts_message_.clear();
  status_message_.clear();
  details_.clear();
  info_box_.reset();
  progress_visible_ = true;
  progress_bar_error_state_ = false;
  progress_active_step_ = 0;
  step_progress_bar_configuration_ =
      ShowProgressBarProto::StepProgressBarConfiguration();
  peek_mode_ = ConfigureBottomSheetProto::HANDLE;
}

void UiController::OnUiShownChanged(bool shown) {
  // Stop any ongoing TTS if UI is hidden.
  if (!shown && tts_button_state_ == TtsButtonState::PLAYING) {
    // Will not cause any TTS event.
    tts_controller_->Stop();
    SetTtsButtonState(TtsButtonState::DEFAULT);
  }
}

bool UiController::SupportsExternalActions() {
  return false;
}

void UiController::ExecuteExternalAction(
    const external::Action& external_action,
    bool is_interrupt,
    base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>
        start_dom_checks_callback,
    base::OnceCallback<void(const external::Result& result)>
        end_action_callback) {
  NOTREACHED() << "Flows using default UI don't support external actions.";
}

bool UiController::IsUiShuttingDown() const {
  return is_shutting_down_;
}

void UiController::OnInterruptStarted() {}
void UiController::OnInterruptFinished() {}

void UiController::EnterBrowseModeForShutdown() {
  DCHECK(!is_shutting_down_) << "should only be called once per flow";

  // This state is necessary to prevent follow-up actions from clearing or
  // resetting the user actions during shutdown, which would lead to visual
  // noise.
  is_shutting_down_ = true;

  // Hide the UI, but don't directly shut down to allow the last actions to
  // complete.
  execution_delegate_->EnterBrowseModeForShutdown();
}

}  // namespace autofill_assistant
