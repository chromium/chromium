// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_executor.h"

#include <cstdio>
#include <ostream>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/self_delete_full_card_requester.h"
#include "components/autofill_assistant/browser/service.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {
namespace {

std::ostream& operator<<(std::ostream& out,
                         const ScriptExecutor::AtEnd& at_end) {
#ifdef NDEBUG
  out << static_cast<int>(at_end);
  return out;
#else
  switch (at_end) {
    case ScriptExecutor::CONTINUE:
      out << "CONTINUE";
      break;
    case ScriptExecutor::SHUTDOWN:
      out << "SHUTDOWN";
      break;
    case ScriptExecutor::SHUTDOWN_GRACEFULLY:
      out << "SHUTDOWN_GRACEFULLY";
      break;
    case ScriptExecutor::CLOSE_CUSTOM_TAB:
      out << "CLOSE_CUSTOM_TAB";
      break;
      // Intentionally no default case to make compilation fail if a new value
      // was added to the enum but not to this list.
  }
  return out;
#endif  // NDEBUG
}

}  // namespace

ScriptExecutor::ScriptExecutor(
    const std::string& script_path,
    std::unique_ptr<TriggerContext> additional_context,
    const std::string& global_payload,
    const std::string& script_payload,
    ScriptExecutor::Listener* listener,
    std::map<std::string, ScriptStatusProto>* scripts_state,
    const std::vector<std::unique_ptr<Script>>* ordered_interrupts,
    ScriptExecutorDelegate* delegate)
    : script_path_(script_path),
      additional_context_(std::move(additional_context)),
      last_global_payload_(global_payload),
      initial_script_payload_(script_payload),
      last_script_payload_(script_payload),
      listener_(listener),
      delegate_(delegate),
      ordered_interrupts_(ordered_interrupts),
      scripts_state_(scripts_state) {
  DCHECK(delegate_);
  DCHECK(ordered_interrupts_);
}

ScriptExecutor::~ScriptExecutor() {
  delegate_->RemoveNavigationListener(this);
  delegate_->RemoveListener(this);
}

ScriptExecutor::Result::Result() = default;
ScriptExecutor::Result::~Result() = default;

void ScriptExecutor::Run(const UserData* user_data,
                         RunScriptCallback callback) {
#ifdef NDEBUG
  VLOG(2) << "Starting script";
#else
  DVLOG(2) << "Starting script " << script_path_;
#endif
  (*scripts_state_)[script_path_] = SCRIPT_STATUS_RUNNING;

  DCHECK(user_data);
  user_data_ = user_data;

  delegate_->AddNavigationListener(this);
  delegate_->AddListener(this);

  callback_ = std::move(callback);
  DCHECK(delegate_->GetService());

#ifdef NDEBUG
  VLOG(2) << "GetActions for (redacted)";
#else
  VLOG(2) << "GetActions for " << delegate_->GetCurrentURL().host();
#endif

  delegate_->GetService()->GetActions(
      script_path_, delegate_->GetScriptURL(),
      MergedTriggerContext(
          {delegate_->GetTriggerContext(), additional_context_.get()}),
      last_global_payload_, last_script_payload_,
      base::BindOnce(&ScriptExecutor::OnGetActions,
                     weak_ptr_factory_.GetWeakPtr()));
}

const UserData* ScriptExecutor::GetUserData() const {
  DCHECK(user_data_);
  return user_data_;
}

UserModel* ScriptExecutor::GetUserModel() {
  return delegate_->GetUserModel();
}

void ScriptExecutor::OnNavigationStateChanged() {
  NavigationInfoProto& navigation_info = current_action_data_.navigation_info;
  if (delegate_->IsNavigatingToNewDocument()) {
    navigation_info.set_started(true);
    navigation_info.set_unexpected(expected_navigation_step_ !=
                                   ExpectedNavigationStep::EXPECTED);
  } else {
    navigation_info.set_ended(true);
  }

  if (delegate_->HasNavigationError()) {
    navigation_info.set_has_error(true);
  }

  switch (expected_navigation_step_) {
    case ExpectedNavigationStep::UNEXPECTED:
      break;

    case ExpectedNavigationStep::EXPECTED:
      if (delegate_->IsNavigatingToNewDocument()) {
        expected_navigation_step_ = ExpectedNavigationStep::STARTED;
      }
      break;

    case ExpectedNavigationStep::STARTED:
      if (!delegate_->IsNavigatingToNewDocument()) {
        expected_navigation_step_ = ExpectedNavigationStep::DONE;
        if (on_expected_navigation_done_)
          std::move(on_expected_navigation_done_)
              .Run(!delegate_->HasNavigationError());
      }
      // Early return since current_action_data_ is no longer valid at this
      // point.
      return;

    case ExpectedNavigationStep::DONE:
      // nothing to do
      break;
  }

  // Potentially terminate an ongoing prompt action.
  if (navigation_info.ended() &&
      current_action_data_.end_prompt_on_navigation_callback) {
    std::move(current_action_data_.end_prompt_on_navigation_callback).Run();
  }
}

bool ScriptExecutor::ShouldInterruptOnPause(const ActionProto& proto) {
  switch (proto.action_info_case()) {
    case ActionProto::ActionInfoCase::kPrompt:
    case ActionProto::ActionInfoCase::kCollectUserData:
    case ActionProto::ActionInfoCase::kShowGenericUi:
      return true;
    case ActionProto::ActionInfoCase::kClick:
    case ActionProto::ActionInfoCase::kTell:
    case ActionProto::ActionInfoCase::kFocusElement:
    case ActionProto::ActionInfoCase::kUseAddress:
    case ActionProto::ActionInfoCase::kUseCard:
    case ActionProto::ActionInfoCase::kWaitForDom:
    case ActionProto::ActionInfoCase::kSelectOption:
    case ActionProto::ActionInfoCase::kNavigate:
    case ActionProto::ActionInfoCase::kStop:
    case ActionProto::ActionInfoCase::kHighlightElement:
    case ActionProto::ActionInfoCase::kUploadDom:
    case ActionProto::ActionInfoCase::kShowDetails:
    case ActionProto::ActionInfoCase::kSetFormValue:
    case ActionProto::ActionInfoCase::kShowProgressBar:
    case ActionProto::ActionInfoCase::kSetAttribute:
    case ActionProto::ActionInfoCase::kShowInfoBox:
    case ActionProto::ActionInfoCase::kExpectNavigation:
    case ActionProto::ActionInfoCase::kWaitForNavigation:
    case ActionProto::ActionInfoCase::kConfigureBottomSheet:
    case ActionProto::ActionInfoCase::kShowForm:
    case ActionProto::ActionInfoCase::kPopupMessage:
    case ActionProto::ActionInfoCase::kWaitForDocument:
    case ActionProto::ActionInfoCase::kGeneratePasswordForFormField:
    case ActionProto::ActionInfoCase::kSaveGeneratedPassword:
    case ActionProto::ActionInfoCase::kConfigureUiState:
    case ActionProto::ActionInfoCase::kPresaveGeneratedPassword:
    case ActionProto::ActionInfoCase::ACTION_INFO_NOT_SET:
      return false;
  }
}

void ScriptExecutor::OnPause(const std::string& message,
                             const std::string& button_label) {
  if (current_action_index_.has_value()) {
    DCHECK_LT(*current_action_index_, actions_.size());
    if (ShouldInterruptOnPause(actions_[*current_action_index_]->proto())) {
      actions_[*current_action_index_] = ProtocolUtils::CreateAction(
          this, actions_[*current_action_index_]->proto());
      current_action_data_ = CurrentActionData();
      current_action_index_.reset();
    }
  }

  delegate_->ClearInfoBox();
  delegate_->SetDetails(nullptr);
  delegate_->SetCollectUserDataOptions(nullptr);
  delegate_->SetForm(nullptr, base::DoNothing(), base::DoNothing());

  last_status_message_ = GetStatusMessage();
  delegate_->SetStatusMessage(message);

  auto user_actions = std::make_unique<std::vector<UserAction>>();

  UserAction undo_action;
  Chip undo_chip;
  undo_chip.type = ChipType::HIGHLIGHTED_ACTION;
  undo_chip.text = button_label;
  undo_action.chip() = undo_chip;
  undo_action.SetCallback(base::BindOnce(&ScriptExecutor::OnResume,
                                         weak_ptr_factory_.GetWeakPtr()));
  user_actions->emplace_back(std::move(undo_action));

  delegate_->SetUserActions(std::move(user_actions));
  delegate_->EnterState(AutofillAssistantState::STOPPED);
  is_paused_ = true;
}

void ScriptExecutor::OnResume() {
  DCHECK(is_paused_);
  is_paused_ = false;

  delegate_->EnterState(AutofillAssistantState::RUNNING);
  delegate_->SetStatusMessage(last_status_message_);

  if (!current_action_index_.has_value()) {
    ProcessNextAction();
  }
}

void ScriptExecutor::RunElementChecks(BatchElementChecker* checker) {
  return checker->Run(delegate_->GetWebController());
}

void ScriptExecutor::ShortWaitForElement(
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  current_action_data_.wait_for_dom = std::make_unique<WaitForDomOperation>(
      this, delegate_, delegate_->GetSettings().short_wait_for_element_deadline,
      /* allow_interrupt= */ false,
      base::BindRepeating(&ScriptExecutor::CheckElementMatches,
                          weak_ptr_factory_.GetWeakPtr(), selector),
      base::BindOnce(&ScriptExecutor::OnShortWaitForElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  current_action_data_.wait_for_dom->Run();
}

void ScriptExecutor::WaitForDom(
    base::TimeDelta max_wait_time,
    bool allow_interrupt,
    base::RepeatingCallback<void(BatchElementChecker*,
                                 base::OnceCallback<void(const ClientStatus&)>)>
        check_elements,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  current_action_data_.wait_for_dom = std::make_unique<WaitForDomOperation>(
      this, delegate_, max_wait_time, allow_interrupt, check_elements,
      base::BindOnce(&ScriptExecutor::OnWaitForElementVisibleWithInterrupts,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  current_action_data_.wait_for_dom->Run();
}

void ScriptExecutor::SetStatusMessage(const std::string& message) {
  delegate_->SetStatusMessage(message);
}

std::string ScriptExecutor::GetStatusMessage() {
  return delegate_->GetStatusMessage();
}

void ScriptExecutor::SetBubbleMessage(const std::string& message) {
  delegate_->SetBubbleMessage(message);
}

std::string ScriptExecutor::GetBubbleMessage() {
  return delegate_->GetBubbleMessage();
}

void ScriptExecutor::FindElement(const Selector& selector,
                                 ElementFinder::Callback callback) {
  delegate_->GetWebController()->FindElement(selector, /* strict_mode= */ true,
                                             std::move(callback));
}

void ScriptExecutor::WaitForDocumentToBecomeInteractive(
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  delegate_->GetWebController()->WaitForDocumentToBecomeInteractive(
      element, std::move(callback));
}

void ScriptExecutor::ScrollIntoView(
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  delegate_->GetWebController()->ScrollIntoView(element, std::move(callback));
}

void ScriptExecutor::ClickOrTapElement(
    ClickType click_type,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  delegate_->GetWebController()->ClickOrTapElement(element, click_type,
                                                   std::move(callback));
}

void ScriptExecutor::CollectUserData(
    CollectUserDataOptions* collect_user_data_options) {
  collect_user_data_options->confirm_callback = base::BindOnce(
      &ScriptExecutor::OnGetUserData, weak_ptr_factory_.GetWeakPtr(),
      std::move(collect_user_data_options->confirm_callback));
  collect_user_data_options->additional_actions_callback = base::BindOnce(
      &ScriptExecutor::OnAdditionalActionTriggered,
      weak_ptr_factory_.GetWeakPtr(),
      std::move(collect_user_data_options->additional_actions_callback));
  collect_user_data_options->terms_link_callback =
      base::BindOnce(&ScriptExecutor::OnTermsAndConditionsLinkClicked,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(collect_user_data_options->terms_link_callback));
  delegate_->SetCollectUserDataOptions(collect_user_data_options);
  delegate_->EnterState(AutofillAssistantState::PROMPT);
}

void ScriptExecutor::SetLastSuccessfulUserDataOptions(
    std::unique_ptr<CollectUserDataOptions> collect_user_data_options) {
  delegate_->SetLastSuccessfulUserDataOptions(
      std::move(collect_user_data_options));
}

const CollectUserDataOptions* ScriptExecutor::GetLastSuccessfulUserDataOptions()
    const {
  return delegate_->GetLastSuccessfulUserDataOptions();
}

void ScriptExecutor::WriteUserData(
    base::OnceCallback<void(UserData*, UserData::FieldChange*)>
        write_callback) {
  delegate_->WriteUserData(std::move(write_callback));
}

void ScriptExecutor::OnGetUserData(
    base::OnceCallback<void(UserData*, const UserModel*)> callback,
    UserData* user_data,
    const UserModel* user_model) {
  delegate_->EnterState(AutofillAssistantState::RUNNING);
  delegate_->SetUserActions(nullptr);
  std::move(callback).Run(user_data, user_model);
}

void ScriptExecutor::OnAdditionalActionTriggered(
    base::OnceCallback<void(int, UserData*, const UserModel*)> callback,
    int index,
    UserData* user_data,
    const UserModel* user_model) {
  delegate_->EnterState(AutofillAssistantState::RUNNING);
  std::move(callback).Run(index, user_data, user_model);
}

void ScriptExecutor::OnTermsAndConditionsLinkClicked(
    base::OnceCallback<void(int, UserData*, const UserModel*)> callback,
    int link,
    UserData* user_data,
    const UserModel* user_model) {
  delegate_->EnterState(AutofillAssistantState::RUNNING);
  std::move(callback).Run(link, user_data, user_model);
}

void ScriptExecutor::GetFullCard(
    const autofill::CreditCard* credit_card,
    base::OnceCallback<void(std::unique_ptr<autofill::CreditCard> card,
                            const base::string16& cvc)> callback) {
  DCHECK(credit_card);

  // User might be asked to provide the cvc.
  delegate_->EnterState(AutofillAssistantState::MODAL_DIALOG);

  // TODO(crbug.com/806868): Consider refactoring SelfDeleteFullCardRequester
  // so as to unit test it.
  (new SelfDeleteFullCardRequester())
      ->GetFullCard(
          GetWebContents(), credit_card,
          base::BindOnce(&ScriptExecutor::OnGetFullCard,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ScriptExecutor::OnGetFullCard(GetFullCardCallback callback,
                                   std::unique_ptr<autofill::CreditCard> card,
                                   const base::string16& cvc) {
  delegate_->EnterState(AutofillAssistantState::RUNNING);
  std::move(callback).Run(std::move(card), cvc);
}

void ScriptExecutor::Prompt(
    std::unique_ptr<std::vector<UserAction>> user_actions,
    bool disable_force_expand_sheet,
    base::OnceCallback<void()> end_on_navigation_callback,
    bool browse_mode,
    bool browse_mode_invisible) {
  // First communicate to the delegate that prompt actions should or should not
  // expand the sheet intitially.
  delegate_->SetExpandSheetForPromptAction(!disable_force_expand_sheet);
  delegate_->SetBrowseModeInvisible(browse_mode_invisible);
  if (browse_mode) {
    delegate_->EnterState(AutofillAssistantState::BROWSE);
  } else if (delegate_->EnterState(AutofillAssistantState::PROMPT)) {
    if (touchable_element_area_) {
      // Prompt() reproduces the end-of-script appearance and behavior during
      // script execution. This includes allowing access to touchable elements,
      // set through a previous call to the focus action with touchable_elements
      // set.
      delegate_->SetTouchableElementArea(*touchable_element_area_);

      // The touchable element and overlays are cleared by calling
      // ScriptExecutor::CleanUpAfterPrompt
    }

    if (end_on_navigation_callback) {
      current_action_data_.end_prompt_on_navigation_callback =
          std::move(end_on_navigation_callback);
    }
  }

  if (user_actions != nullptr) {
    for (auto& user_action : *user_actions) {
      if (!user_action.HasCallback())
        continue;

      user_action.AddInterceptor(base::BindOnce(
          &ScriptExecutor::OnChosen, weak_ptr_factory_.GetWeakPtr()));
    }
    delegate_->SetUserActions(std::move(user_actions));
  }
}

void ScriptExecutor::CleanUpAfterPrompt() {
  delegate_->SetUserActions(nullptr);
  // Mark touchable_elements_ as consumed, so that it won't affect the next
  // prompt or the end of the script.
  touchable_element_area_.reset();

  delegate_->ClearTouchableElementArea();
  delegate_->SetExpandSheetForPromptAction(true);
  delegate_->SetBrowseModeInvisible(false);
  delegate_->EnterState(AutofillAssistantState::RUNNING);
}

void ScriptExecutor::SetBrowseDomainsWhitelist(
    std::vector<std::string> domains) {
  delegate_->SetBrowseDomainsWhitelist(std::move(domains));
}

void ScriptExecutor::OnChosen(UserAction::Callback callback,
                              std::unique_ptr<TriggerContext> context) {
  if (context->is_direct_action()) {
    current_action_data_.direct_action = true;
  }
  std::move(callback).Run(std::move(context));
}

void ScriptExecutor::FillAddressForm(
    const autofill::AutofillProfile* profile,
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  delegate_->GetWebController()->FillAddressForm(profile, selector,
                                                 std::move(callback));
}

void ScriptExecutor::FillCardForm(
    std::unique_ptr<autofill::CreditCard> card,
    const base::string16& cvc,
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  delegate_->GetWebController()->FillCardForm(std::move(card), cvc, selector,
                                              std::move(callback));
}

void ScriptExecutor::RetrieveElementFormAndFieldData(
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&,
                            const autofill::FormData&,
                            const autofill::FormFieldData&)> callback) {
  delegate_->GetWebController()->RetrieveElementFormAndFieldData(
      selector, std::move(callback));
}

void ScriptExecutor::SelectOption(
    const std::string& value,
    DropdownSelectStrategy select_strategy,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  delegate_->GetWebController()->SelectOption(element, value, select_strategy,
                                              std::move(callback));
}

void ScriptExecutor::HighlightElement(
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  delegate_->GetWebController()->HighlightElement(selector,
                                                  std::move(callback));
}

void ScriptExecutor::FocusElement(
    const Selector& selector,
    const TopPadding& top_padding,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  last_focused_element_selector_ = selector;
  last_focused_element_top_padding_ = top_padding;
  delegate_->GetWebController()->FocusElement(selector, top_padding,
                                              std::move(callback));
}

void ScriptExecutor::SetTouchableElementArea(
    const ElementAreaProto& touchable_element_area) {
  touchable_element_area_ =
      std::make_unique<ElementAreaProto>(touchable_element_area);
}

void ScriptExecutor::SetProgress(int progress) {
  delegate_->SetProgress(progress);
}

bool ScriptExecutor::SetProgressActiveStepIdentifier(
    const std::string& active_step_identifier) {
  return delegate_->SetProgressActiveStepIdentifier(active_step_identifier);
}

void ScriptExecutor::SetProgressActiveStep(int active_step) {
  delegate_->SetProgressActiveStep(active_step);
}

void ScriptExecutor::SetProgressVisible(bool visible) {
  delegate_->SetProgressVisible(visible);
}

void ScriptExecutor::SetProgressBarErrorState(bool error) {
  delegate_->SetProgressBarErrorState(error);
}

void ScriptExecutor::SetStepProgressBarConfiguration(
    const ShowProgressBarProto::StepProgressBarConfiguration& configuration) {
  delegate_->SetStepProgressBarConfiguration(configuration);
}

void ScriptExecutor::GetFieldValue(
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&, const std::string&)>
        callback) {
  delegate_->GetWebController()->GetFieldValue(selector, std::move(callback));
}

void ScriptExecutor::SetFieldValue(
    const std::string& value,
    KeyboardValueFillStrategy fill_strategy,
    int key_press_delay_in_millisecond,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  delegate_->GetWebController()->SetFieldValue(element, value, fill_strategy,
                                               key_press_delay_in_millisecond,
                                               std::move(callback));
}

void ScriptExecutor::SetAttribute(
    const std::vector<std::string>& attributes,
    const std::string& value,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  delegate_->GetWebController()->SetAttribute(element, attributes, value,
                                              std::move(callback));
}

void ScriptExecutor::SendKeyboardInput(
    const std::vector<UChar32>& codepoints,
    int key_press_delay_in_millisecond,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  delegate_->GetWebController()->SendKeyboardInput(
      element, codepoints, key_press_delay_in_millisecond, std::move(callback));
}

void ScriptExecutor::GetOuterHtml(
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&, const std::string&)>
        callback) {
  delegate_->GetWebController()->GetOuterHtml(selector, std::move(callback));
}

void ScriptExecutor::GetElementTag(
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&, const std::string&)>
        callback) {
  delegate_->GetWebController()->GetElementTag(element, std::move(callback));
}

void ScriptExecutor::ExpectNavigation() {
  // TODO(b/160948417): Clean this up such that the logic is not required in
  //  both |ScriptExecutor| and |Controller|.
  delegate_->ExpectNavigation();
  expected_navigation_step_ = ExpectedNavigationStep::EXPECTED;
}

bool ScriptExecutor::ExpectedNavigationHasStarted() {
  return expected_navigation_step_ != ExpectedNavigationStep::EXPECTED;
}

bool ScriptExecutor::WaitForNavigation(
    base::OnceCallback<void(bool)> callback) {
  switch (expected_navigation_step_) {
    case ExpectedNavigationStep::UNEXPECTED:
      return false;

    case ExpectedNavigationStep::DONE:
      std::move(callback).Run(!delegate_->HasNavigationError());
      break;

    case ExpectedNavigationStep::EXPECTED:
    case ExpectedNavigationStep::STARTED:
      on_expected_navigation_done_ = std::move(callback);
      break;

      // No default to make compilation fail if not all cases are covered
  }
  return true;
}

void ScriptExecutor::GetDocumentReadyState(
    const Selector& optional_frame,
    base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
        callback) {
  delegate_->GetWebController()->GetDocumentReadyState(optional_frame,
                                                       std::move(callback));
}

void ScriptExecutor::WaitForDocumentReadyState(
    const Selector& optional_frame,
    DocumentReadyState min_ready_state,
    base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
        callback) {
  delegate_->GetWebController()->WaitForDocumentReadyState(
      optional_frame, min_ready_state, std::move(callback));
}

void ScriptExecutor::LoadURL(const GURL& url) {
  delegate_->GetWebController()->LoadURL(url);
}

void ScriptExecutor::Shutdown() {
  // The following handles the case where scripts end with tell + stop
  // differently from just stop. TODO(b/806868): Make that difference explicit:
  // add an optional message to stop and update the scripts to use that.
  if (previous_action_type_ == ActionProto::kTell) {
    at_end_ = SHUTDOWN_GRACEFULLY;
  } else {
    at_end_ = SHUTDOWN;
  }
}

void ScriptExecutor::Close() {
  at_end_ = CLOSE_CUSTOM_TAB;
  should_stop_script_ = true;
}

autofill::PersonalDataManager* ScriptExecutor::GetPersonalDataManager() {
  return delegate_->GetPersonalDataManager();
}

WebsiteLoginManager* ScriptExecutor::GetWebsiteLoginManager() {
  return delegate_->GetWebsiteLoginManager();
}

content::WebContents* ScriptExecutor::GetWebContents() {
  return delegate_->GetWebContents();
}

std::string ScriptExecutor::GetEmailAddressForAccessTokenAccount() {
  return delegate_->GetEmailAddressForAccessTokenAccount();
}

std::string ScriptExecutor::GetLocale() {
  return delegate_->GetLocale();
}

void ScriptExecutor::SetDetails(std::unique_ptr<Details> details) {
  return delegate_->SetDetails(std::move(details));
}

void ScriptExecutor::ClearInfoBox() {
  delegate_->ClearInfoBox();
}

void ScriptExecutor::SetInfoBox(const InfoBox& info_box) {
  delegate_->SetInfoBox(info_box);
}

void ScriptExecutor::SetViewportMode(ViewportMode mode) {
  delegate_->SetViewportMode(mode);
}

ViewportMode ScriptExecutor::GetViewportMode() {
  return delegate_->GetViewportMode();
}

void ScriptExecutor::SetPeekMode(
    ConfigureBottomSheetProto::PeekMode peek_mode) {
  delegate_->SetPeekMode(peek_mode);
}

ConfigureBottomSheetProto::PeekMode ScriptExecutor::GetPeekMode() {
  return delegate_->GetPeekMode();
}

void ScriptExecutor::ExpandBottomSheet() {
  return delegate_->ExpandBottomSheet();
}

void ScriptExecutor::CollapseBottomSheet() {
  return delegate_->CollapseBottomSheet();
}

void ScriptExecutor::WaitForWindowHeightChange(
    base::OnceCallback<void(const ClientStatus&)> callback) {
  delegate_->GetWebController()->WaitForWindowHeightChange(std::move(callback));
}

const ClientSettings& ScriptExecutor::GetSettings() {
  return delegate_->GetSettings();
}

bool ScriptExecutor::SetForm(
    std::unique_ptr<FormProto> form,
    base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
    base::OnceCallback<void(const ClientStatus&)> cancel_callback) {
  return delegate_->SetForm(std::move(form), std::move(changed_callback),
                            std::move(cancel_callback));
}

void ScriptExecutor::RequireUI() {
  delegate_->RequireUI();
}

void ScriptExecutor::SetGenericUi(
    std::unique_ptr<GenericUserInterfaceProto> generic_ui,
    base::OnceCallback<void(const ClientStatus&)> end_action_callback,
    base::OnceCallback<void(const ClientStatus&)>
        view_inflation_finished_callback) {
  delegate_->SetGenericUi(std::move(generic_ui), std::move(end_action_callback),
                          std::move(view_inflation_finished_callback));
}

void ScriptExecutor::ClearGenericUi() {
  delegate_->ClearGenericUi();
}

void ScriptExecutor::SetOverlayBehavior(
    ConfigureUiStateProto::OverlayBehavior overlay_behavior) {
  delegate_->SetOverlayBehavior(overlay_behavior);
}

base::WeakPtr<ActionDelegate> ScriptExecutor::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ScriptExecutor::OnGetActions(bool result, const std::string& response) {
  bool success = result && ProcessNextActionResponse(response);
  VLOG(2) << __func__ << " result=" << result;
  if (should_stop_script_) {
    // The last action forced the script to stop. Sending the result of the
    // action is considered best effort in this situation. Report a successful
    // run to the caller no matter what, so we don't confuse users with an error
    // message.
    RunCallback(true);
    return;
  }

  if (!success) {
    RunCallback(false);
    return;
  }

  if (!actions_.empty()) {
    ProcessNextAction();
    return;
  }

  RunCallback(true);
}

bool ScriptExecutor::ProcessNextActionResponse(const std::string& response) {
  processed_actions_.clear();
  actions_.clear();

  bool should_update_scripts = false;
  std::vector<std::unique_ptr<Script>> scripts;
  bool parse_result = ProtocolUtils::ParseActions(
      this, response, &last_global_payload_, &last_script_payload_, &actions_,
      &scripts, &should_update_scripts);
  if (!parse_result) {
    return false;
  }

  ReportPayloadsToListener();
  if (should_update_scripts) {
    ReportScriptsUpdateToListener(std::move(scripts));
  }
  return true;
}

void ScriptExecutor::ReportPayloadsToListener() {
  if (!listener_)
    return;

  listener_->OnServerPayloadChanged(last_global_payload_, last_script_payload_);
}

void ScriptExecutor::ReportScriptsUpdateToListener(
    std::vector<std::unique_ptr<Script>> scripts) {
  if (!listener_)
    return;

  listener_->OnScriptListChanged(std::move(scripts));
}

void ScriptExecutor::RunCallback(bool success) {
  if (should_clean_contextual_ui_on_finish_ || !success) {
    SetDetails(nullptr);
    should_clean_contextual_ui_on_finish_ = false;
  }

  Result result;
  result.success = success;
  result.at_end = at_end_;
  result.touchable_element_area = std::move(touchable_element_area_);

  RunCallbackWithResult(result);
}

void ScriptExecutor::RunCallbackWithResult(const Result& result) {
  DCHECK(callback_);
  (*scripts_state_)[script_path_] =
      result.success ? SCRIPT_STATUS_SUCCESS : SCRIPT_STATUS_FAILURE;
  std::move(callback_).Run(result);
}

void ScriptExecutor::ProcessNextAction() {
  current_action_index_.reset();
  if (is_paused_) {
    return;
  }

  // We could get into a strange situation if ProcessNextAction is called before
  // the action was reported as processed, which should not happen. In that case
  // we could have more |processed_actions| than |actions_|.
  if (actions_.size() <= processed_actions_.size()) {
    DCHECK_EQ(actions_.size(), processed_actions_.size());
    VLOG(2) << __func__ << ", get more actions";
    GetNextActions();
    return;
  }

  current_action_index_ = processed_actions_.size();
  Action* action = actions_[*current_action_index_].get();
  should_clean_contextual_ui_on_finish_ = action->proto().clean_contextual_ui();
  int delay_ms = action->proto().action_delay_ms();
  if (delay_ms > 0) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ScriptExecutor::ProcessAction,
                       weak_ptr_factory_.GetWeakPtr(), action),
        base::TimeDelta::FromMilliseconds(delay_ms));
  } else {
    ProcessAction(action);
  }
}

void ScriptExecutor::ProcessAction(Action* action) {
  VLOG(2) << "Begin action: " << *action;

  current_action_data_ = CurrentActionData();
  current_action_data_.navigation_info.set_has_error(
      delegate_->HasNavigationError());

  action->ProcessAction(base::BindOnce(&ScriptExecutor::OnProcessedAction,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       base::TimeTicks::Now()));
}

void ScriptExecutor::GetNextActions() {
  delegate_->GetService()->GetNextActions(
      MergedTriggerContext(
          {delegate_->GetTriggerContext(), additional_context_.get()}),
      last_global_payload_, last_script_payload_, processed_actions_,
      base::BindOnce(&ScriptExecutor::OnGetActions,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScriptExecutor::OnProcessedAction(
    base::TimeTicks start_time,
    std::unique_ptr<ProcessedActionProto> processed_action_proto) {
  base::TimeDelta run_time = base::TimeTicks::Now() - start_time;
  previous_action_type_ = processed_action_proto->action().action_info_case();
  processed_actions_.emplace_back(*processed_action_proto);

  auto& processed_action = processed_actions_.back();
  processed_action.set_run_time_ms(run_time.InMilliseconds());
  processed_action.set_direct_action(current_action_data_.direct_action);
  *processed_action.mutable_navigation_info() =
      current_action_data_.navigation_info;

  if (processed_action.status() != ProcessedActionStatusProto::ACTION_APPLIED) {
    if (delegate_->HasNavigationError()) {
      // Overwrite the original error, as the root cause is most likely a
      // navigation error.
      processed_action.mutable_status_details()->set_original_status(
          processed_action.status());
      processed_action.set_status(ProcessedActionStatusProto::NAVIGATION_ERROR);
    }
    VLOG(1) << "Action failed: " << processed_action.status();
    // Remove unexecuted actions, this will cause the |ProcessNextActions| call
    // to immediately ask for new actions.
    actions_.resize(processed_actions_.size());
  }
  ProcessNextAction();
}

void ScriptExecutor::CheckElementMatches(
    const Selector& selector,
    BatchElementChecker* checker,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  checker->AddElementCheck(selector, std::move(callback));
}

void ScriptExecutor::OnShortWaitForElement(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& element_status,
    const Result* interrupt_result) {
  // Interrupts cannot run, so should never be reported.
  DCHECK(!interrupt_result);

  std::move(callback).Run(element_status);
}

void ScriptExecutor::OnWaitForElementVisibleWithInterrupts(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& element_status,
    const Result* interrupt_result) {
  if (interrupt_result) {
    if (!interrupt_result->success) {
      std::move(callback).Run(ClientStatus(INTERRUPT_FAILED));
      return;
    }
    if (interrupt_result->at_end != CONTINUE) {
      at_end_ = interrupt_result->at_end;
      should_stop_script_ = true;
      std::move(callback).Run(ClientStatus(MANUAL_FALLBACK));
      return;
    }
  }
  std::move(callback).Run(element_status);
}

ScriptExecutor::WaitForDomOperation::WaitForDomOperation(
    ScriptExecutor* main_script,
    ScriptExecutorDelegate* delegate,
    base::TimeDelta max_wait_time,
    bool allow_interrupt,
    base::RepeatingCallback<void(BatchElementChecker*,
                                 base::OnceCallback<void(const ClientStatus&)>)>
        check_elements,
    WaitForDomOperation::Callback callback)
    : main_script_(main_script),
      delegate_(delegate),
      max_wait_time_(max_wait_time),
      allow_interrupt_(allow_interrupt),
      check_elements_(std::move(check_elements)),
      callback_(std::move(callback)),
      retry_timer_(main_script->delegate_->GetSettings()
                       .periodic_element_check_interval) {}

ScriptExecutor::WaitForDomOperation::~WaitForDomOperation() {
  delegate_->RemoveNavigationListener(this);
}

void ScriptExecutor::WaitForDomOperation::Run() {
  delegate_->AddNavigationListener(this);
  Start();
}

void ScriptExecutor::WaitForDomOperation::Start() {
  retry_timer_.Start(
      max_wait_time_,
      base::BindRepeating(&ScriptExecutor::WaitForDomOperation::RunChecks,
                          // safe since this instance owns retry_timer_
                          base::Unretained(this)),
      base::BindOnce(&ScriptExecutor::WaitForDomOperation::RunCallback,
                     base::Unretained(this)));
}

void ScriptExecutor::WaitForDomOperation::Pause() {
  if (interrupt_executor_) {
    // If an interrupt is running, it'll be the one to be paused, if necessary.
    return;
  }

  retry_timer_.Cancel();
}

void ScriptExecutor::WaitForDomOperation::Continue() {
  if (retry_timer_.running() || !callback_)
    return;

  Start();
}

void ScriptExecutor::WaitForDomOperation::OnNavigationStateChanged() {
  if (delegate_->IsNavigatingToNewDocument()) {
    Pause();
  } else {
    Continue();
  }
}

void ScriptExecutor::WaitForDomOperation::OnServerPayloadChanged(
    const std::string& global_payload,
    const std::string& script_payload) {
  // Interrupts and main scripts share global payloads, but not script payloads.
  main_script_->last_global_payload_ = global_payload;
  main_script_->ReportPayloadsToListener();
}

void ScriptExecutor::WaitForDomOperation::OnScriptListChanged(
    std::vector<std::unique_ptr<Script>> scripts) {
  main_script_->ReportScriptsUpdateToListener(std::move(scripts));
}

void ScriptExecutor::WaitForDomOperation::RunChecks(
    base::OnceCallback<void(const ClientStatus&)> report_attempt_result) {
  // Reset state possibly left over from previous runs.
  element_check_result_ = ClientStatus();
  runnable_interrupts_.clear();
  batch_element_checker_ = std::make_unique<BatchElementChecker>();
  check_elements_.Run(batch_element_checker_.get(),
                      base::BindOnce(&WaitForDomOperation::OnElementCheckDone,
                                     base::Unretained(this)));
  if (allow_interrupt_) {
    for (const std::unique_ptr<Script>& interrupt :
         *main_script_->ordered_interrupts_) {
      if (ran_interrupts_.find(interrupt->handle.path) !=
          ran_interrupts_.end()) {
        continue;
      }

      interrupt->precondition->Check(
          delegate_->GetCurrentURL(), batch_element_checker_.get(),
          *delegate_->GetTriggerContext(), *main_script_->scripts_state_,
          base::BindOnce(&WaitForDomOperation::OnPreconditionCheckDone,
                         weak_ptr_factory_.GetWeakPtr(),
                         interrupt->handle.path));
    }
  }

  batch_element_checker_->AddAllDoneCallback(
      base::BindOnce(&WaitForDomOperation::OnAllChecksDone,
                     base::Unretained(this), std::move(report_attempt_result)));
  batch_element_checker_->Run(delegate_->GetWebController());
}

void ScriptExecutor::WaitForDomOperation::OnPreconditionCheckDone(
    const std::string& interrupt_path,
    bool precondition_match) {
  if (precondition_match)
    runnable_interrupts_.insert(interrupt_path);
}

void ScriptExecutor::WaitForDomOperation::OnElementCheckDone(
    const ClientStatus& element_status) {
  element_check_result_ = element_status;

  // Wait for all checks to run before reporting that the element was found to
  // the caller, so interrupts have a chance to run.
}

void ScriptExecutor::WaitForDomOperation::OnAllChecksDone(
    base::OnceCallback<void(const ClientStatus&)> report_attempt_result) {
  if (runnable_interrupts_.empty()) {
    // Since no interrupts fired, allow previously-run interrupts to be run
    // again in the next round. This is meant to give elements one round to
    // disappear and avoid the simplest form of loops. A round with interrupts
    // firing doesn't count as one round here, because an interrupt can run
    // quickly and return immediately, without waiting for
    // periodic_element_check_interval.
    ran_interrupts_.clear();
  } else {
    // We must go through runnable_interrupts_ to make sure priority order is
    // respected in case more than one interrupt is ready to run.
    for (const std::unique_ptr<Script>& interrupt :
         *main_script_->ordered_interrupts_) {
      const std::string& path = interrupt->handle.path;
      if (runnable_interrupts_.find(path) != runnable_interrupts_.end()) {
        RunInterrupt(path);
        return;
      }
    }
  }
  std::move(report_attempt_result).Run(element_check_result_);
}

void ScriptExecutor::WaitForDomOperation::RunInterrupt(
    const std::string& path) {
  batch_element_checker_.reset();
  SavePreInterruptState();
  ran_interrupts_.insert(path);
  interrupt_executor_ = std::make_unique<ScriptExecutor>(
      path, TriggerContext::Merge({main_script_->additional_context_.get()}),
      main_script_->last_global_payload_, main_script_->initial_script_payload_,
      /* listener= */ this, main_script_->scripts_state_, &no_interrupts_,
      delegate_);
  delegate_->EnterState(AutofillAssistantState::RUNNING);
  delegate_->SetUserActions(nullptr);
  interrupt_executor_->Run(
      main_script_->user_data_,
      base::BindOnce(&ScriptExecutor::WaitForDomOperation::OnInterruptDone,
                     base::Unretained(this)));
  // base::Unretained(this) is safe because interrupt_executor_ belongs to this
}

void ScriptExecutor::WaitForDomOperation::OnInterruptDone(
    const ScriptExecutor::Result& result) {
  interrupt_executor_.reset();
  if (!result.success || result.at_end != ScriptExecutor::CONTINUE) {
    RunCallbackWithResult(ClientStatus(INTERRUPT_FAILED), &result);
    return;
  }
  RestoreStatusMessage();
  RestorePreInterruptScroll();

  // Restart. We use the original wait time since the interruption could have
  // triggered any kind of actions, including actions that wait on the user. We
  // don't trust a previous element_found_ result, since it could have changed.
  Start();
}

void ScriptExecutor::WaitForDomOperation::RunCallback(
    const ClientStatus& element_status) {
  RunCallbackWithResult(element_status, nullptr);
}

void ScriptExecutor::WaitForDomOperation::RunCallbackWithResult(
    const ClientStatus& element_status,
    const ScriptExecutor::Result* result) {
  // stop element checking if one is still in progress
  batch_element_checker_.reset();
  retry_timer_.Cancel();
  if (!callback_)
    return;

  std::move(callback_).Run(element_status, result);
}

void ScriptExecutor::WaitForDomOperation::SavePreInterruptState() {
  if (saved_pre_interrupt_state_)
    return;

  pre_interrupt_status_ = delegate_->GetStatusMessage();
  saved_pre_interrupt_state_ = true;
}

void ScriptExecutor::WaitForDomOperation::RestoreStatusMessage() {
  if (!saved_pre_interrupt_state_)
    return;

  delegate_->SetStatusMessage(pre_interrupt_status_);
}

void ScriptExecutor::WaitForDomOperation::RestorePreInterruptScroll() {
  if (!saved_pre_interrupt_state_)
    return;

  if (!main_script_->last_focused_element_selector_.empty()) {
    delegate_->GetWebController()->FocusElement(
        main_script_->last_focused_element_selector_,
        main_script_->last_focused_element_top_padding_, base::DoNothing());
  }
}

ScriptExecutor::CurrentActionData::CurrentActionData() = default;
ScriptExecutor::CurrentActionData::~CurrentActionData() = default;
ScriptExecutor::CurrentActionData& ScriptExecutor::CurrentActionData::operator=(
    ScriptExecutor::CurrentActionData&& other) = default;

std::ostream& operator<<(std::ostream& out,
                         const ScriptExecutor::Result& result) {
  result.success ? out << "succeeded. " : out << "failed. ";
  out << "at_end = " << result.at_end;
  return out;
}

}  // namespace autofill_assistant
