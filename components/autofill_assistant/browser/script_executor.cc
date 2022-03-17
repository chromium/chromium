// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_executor.h"

#include <cstdio>
#include <ostream>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/action_delegate_util.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/full_card_requester.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/wait_for_document_operation.h"
#include "components/autofill_assistant/browser/wait_for_dom_operation.h"
#include "components/autofill_assistant/browser/web/element_action_util.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "components/strings/grit/components_strings.h"
#include "net/http/http_status_code.h"
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
    const std::vector<std::unique_ptr<Script>>* ordered_interrupts,
    ScriptExecutorDelegate* delegate,
    ScriptExecutorUiDelegate* ui_delegate)
    : script_path_(script_path),
      additional_context_(std::move(additional_context)),
      last_global_payload_(global_payload),
      initial_script_payload_(script_payload),
      last_script_payload_(script_payload),
      listener_(listener),
      delegate_(delegate),
      ui_delegate_(ui_delegate),
      ordered_interrupts_(ordered_interrupts),
      element_store_(
          std::make_unique<ElementStore>(delegate->GetWebContents())) {
  DCHECK(delegate_);
  DCHECK(ui_delegate_);
  DCHECK(ordered_interrupts_);
}

ScriptExecutor::~ScriptExecutor() {
  delegate_->RemoveNavigationListener(this);
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

  DCHECK(user_data);
  user_data_ = user_data;

  delegate_->AddNavigationListener(this);

  callback_ = std::move(callback);
  DCHECK(delegate_->GetService());

#ifdef NDEBUG
  VLOG(2) << "GetActions for (redacted)";
#else
  VLOG(2) << "GetActions for " << delegate_->GetCurrentURL().host();
#endif

  delegate_->GetService()->GetActions(
      script_path_, delegate_->GetScriptURL(),
      TriggerContext(
          {delegate_->GetTriggerContext(), additional_context_.get()}),
      last_global_payload_, last_script_payload_,
      base::BindOnce(&ScriptExecutor::OnGetActions,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

const UserData* ScriptExecutor::GetUserData() const {
  DCHECK(user_data_);
  return user_data_;
}

UserModel* ScriptExecutor::GetUserModel() const {
  return delegate_->GetUserModel();
}

void ScriptExecutor::OnNavigationStateChanged() {
  Action::ActionData* current_action_data =
      current_action_ != nullptr ? &current_action_->GetActionData() : nullptr;
  if (current_action_data) {
    NavigationInfoProto& navigation_info = current_action_data->navigation_info;
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
      // Early return since the current action may have ended by now.
      return;

    case ExpectedNavigationStep::DONE:
      // nothing to do
      break;
  }

  // Potentially terminate an ongoing prompt action.
  if (current_action_data) {
    if (current_action_data->navigation_info.ended() &&
        current_action_data->end_prompt_on_navigation_callback) {
      std::move(current_action_data->end_prompt_on_navigation_callback).Run();
    }
  }
}

void ScriptExecutor::RunElementChecks(BatchElementChecker* checker) {
  return checker->Run(delegate_->GetWebController());
}

void ScriptExecutor::ShortWaitForElement(
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback) {
  if (!current_action_) {
    NOTREACHED() << "must not be called outside of actions";
    return;
  }
  Action::ActionData& current_action_data = current_action_->GetActionData();
  current_action_data.wait_for_dom = std::make_unique<WaitForDomOperation>(
      this, delegate_, ui_delegate_,
      delegate_->GetSettings().short_wait_for_element_deadline,
      /* allow_observer_mode */ true,
      /* allow_interrupt= */ false, /* observer= */ nullptr,
      base::BindRepeating(&ScriptExecutor::CheckElementConditionMatches,
                          weak_ptr_factory_.GetWeakPtr(), selector),
      base::BindOnce(&ScriptExecutor::OnShortWaitForElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  current_action_data.wait_for_dom->Run();
}

void ScriptExecutor::ShortWaitForElementWithSlowWarning(
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback) {
  if (!current_action_) {
    NOTREACHED() << "must not be called outside of actions";
    return;
  }
  Action::ActionData& current_action_data = current_action_->GetActionData();
  current_action_data.wait_for_dom = std::make_unique<WaitForDomOperation>(
      this, delegate_, ui_delegate_,
      delegate_->GetSettings().short_wait_for_element_deadline,
      /* allow_observer_mode */ true,
      /* allow_interrupt= */ false, /* observer= */ nullptr,
      base::BindRepeating(&ScriptExecutor::CheckElementConditionMatches,
                          weak_ptr_factory_.GetWeakPtr(), selector),
      base::BindOnce(&ScriptExecutor::OnShortWaitForElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  current_action_data.wait_for_dom->SetTimeoutWarningCallback(
      base::BindOnce(&ScriptExecutor::MaybeShowSlowWebsiteWarning,
                     weak_ptr_factory_.GetWeakPtr()));
  current_action_data.wait_for_dom->Run();
}

void ScriptExecutor::WaitForDom(
    base::TimeDelta max_wait_time,
    bool allow_observer_mode,
    bool allow_interrupt,
    WaitForDomObserver* observer,
    base::RepeatingCallback<void(BatchElementChecker*,
                                 base::OnceCallback<void(const ClientStatus&)>)>
        check_elements,
    base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback) {
  if (!current_action_) {
    NOTREACHED() << "must not be called outside of actions";
    return;
  }
  Action::ActionData& current_action_data = current_action_->GetActionData();
  current_action_data.wait_for_dom = std::make_unique<WaitForDomOperation>(
      this, delegate_, ui_delegate_, max_wait_time, allow_observer_mode,
      allow_interrupt, observer, check_elements,
      base::BindOnce(&ScriptExecutor::OnWaitForElementVisibleWithInterrupts,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  current_action_data.wait_for_dom->Run();
}

void ScriptExecutor::WaitForDomWithSlowWarning(
    base::TimeDelta max_wait_time,
    bool allow_interrupt,
    WaitForDomObserver* observer,
    base::RepeatingCallback<void(BatchElementChecker*,
                                 base::OnceCallback<void(const ClientStatus&)>)>
        check_elements,
    base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback) {
  if (!current_action_) {
    NOTREACHED() << "must not be called outside of actions";
    return;
  }
  Action::ActionData& current_action_data = current_action_->GetActionData();
  current_action_data.wait_for_dom = std::make_unique<WaitForDomOperation>(
      this, delegate_, ui_delegate_, max_wait_time,
      /* allow_observer_mode= */ true, allow_interrupt, observer,
      check_elements,
      base::BindOnce(&ScriptExecutor::OnWaitForElementVisibleWithInterrupts,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  current_action_data.wait_for_dom->SetTimeoutWarningCallback(
      base::BindOnce(&ScriptExecutor::MaybeShowSlowWebsiteWarning,
                     weak_ptr_factory_.GetWeakPtr()));
  current_action_data.wait_for_dom->Run();
}

void ScriptExecutor::SetStatusMessage(const std::string& message) {
  ui_delegate_->SetStatusMessage(message);
}

std::string ScriptExecutor::GetStatusMessage() const {
  return ui_delegate_->GetStatusMessage();
}

void ScriptExecutor::SetBubbleMessage(const std::string& message) {
  ui_delegate_->SetBubbleMessage(message);
}

std::string ScriptExecutor::GetBubbleMessage() const {
  return ui_delegate_->GetBubbleMessage();
}

void ScriptExecutor::SetTtsMessage(const std::string& message) {
  ui_delegate_->SetTtsMessage(message);
}

TtsButtonState ScriptExecutor::GetTtsButtonState() const {
  return ui_delegate_->GetTtsButtonState();
}

void ScriptExecutor::MaybePlayTtsMessage() {
  ui_delegate_->MaybePlayTtsMessage();
}

void ScriptExecutor::FindElement(const Selector& selector,
                                 ElementFinder::Callback callback) const {
  VLOG(3) << __func__ << " " << selector;
  delegate_->GetWebController()->FindElement(selector, /* strict_mode= */ true,
                                             std::move(callback));
}

void ScriptExecutor::FindAllElements(const Selector& selector,
                                     ElementFinder::Callback callback) const {
  VLOG(3) << __func__ << " " << selector;
  delegate_->GetWebController()->FindAllElements(selector, std::move(callback));
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
  ui_delegate_->SetCollectUserDataOptions(collect_user_data_options);
  delegate_->EnterState(AutofillAssistantState::PROMPT);
}

void ScriptExecutor::SetLastSuccessfulUserDataOptions(
    std::unique_ptr<CollectUserDataOptions> collect_user_data_options) {
  ui_delegate_->SetLastSuccessfulUserDataOptions(
      std::move(collect_user_data_options));
}

const CollectUserDataOptions* ScriptExecutor::GetLastSuccessfulUserDataOptions()
    const {
  return ui_delegate_->GetLastSuccessfulUserDataOptions();
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
  ui_delegate_->SetUserActions(nullptr);
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

void ScriptExecutor::GetFullCard(const autofill::CreditCard* credit_card,
                                 GetFullCardCallback callback) {
  DCHECK(credit_card);

  // User might be asked to provide the cvc.
  delegate_->EnterState(AutofillAssistantState::MODAL_DIALOG);

  std::unique_ptr<FullCardRequester> full_card_requester =
      std::make_unique<FullCardRequester>();
  FullCardRequester* full_card_requester_ptr = full_card_requester.get();
  full_card_requester_ptr->GetFullCard(
      GetWebContents(), credit_card,
      base::BindOnce(&ScriptExecutor::OnGetFullCard,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(full_card_requester), std::move(callback)));
}

void ScriptExecutor::OnGetFullCard(
    std::unique_ptr<FullCardRequester> full_card_requester,
    GetFullCardCallback callback,
    const ClientStatus& status,
    std::unique_ptr<autofill::CreditCard> card,
    const std::u16string& cvc) {
  delegate_->EnterState(AutofillAssistantState::RUNNING);
  std::move(callback).Run(status, std::move(card), cvc);
}

void ScriptExecutor::Prompt(
    std::unique_ptr<std::vector<UserAction>> user_actions,
    bool disable_force_expand_sheet,
    base::OnceCallback<void()> end_on_navigation_callback,
    bool browse_mode,
    bool browse_mode_invisible) {
  if (!current_action_) {
    NOTREACHED() << "must not be called outside of actions";
    return;
  }

  // First communicate to the delegate that prompt actions should or should not
  // expand the sheet intitially.
  ui_delegate_->SetExpandSheetForPromptAction(!disable_force_expand_sheet);
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
      current_action_->GetActionData().end_prompt_on_navigation_callback =
          std::move(end_on_navigation_callback);
    }
  }

  if (user_actions != nullptr) {
    ui_delegate_->SetUserActions(std::move(user_actions));
  }
}

void ScriptExecutor::CleanUpAfterPrompt() {
  ui_delegate_->SetUserActions(nullptr);
  // Mark touchable_elements_ as consumed, so that it won't affect the next
  // prompt or the end of the script.
  touchable_element_area_.reset();

  delegate_->ClearTouchableElementArea();
  ui_delegate_->SetExpandSheetForPromptAction(true);
  delegate_->SetBrowseModeInvisible(false);
  delegate_->EnterState(AutofillAssistantState::RUNNING);
}

void ScriptExecutor::SetBrowseDomainsAllowlist(
    std::vector<std::string> domains) {
  delegate_->SetBrowseDomainsAllowlist(std::move(domains));
}

void ScriptExecutor::RetrieveElementFormAndFieldData(
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&,
                            const autofill::FormData&,
                            const autofill::FormFieldData&)> callback) {
  delegate_->GetWebController()->RetrieveElementFormAndFieldData(
      selector, std::move(callback));
}

void ScriptExecutor::StoreScrolledToElement(
    const ElementFinder::Result& element) {
  last_focused_element_ = element.dom_object;
}

void ScriptExecutor::SetTouchableElementArea(
    const ElementAreaProto& touchable_element_area) {
  touchable_element_area_ =
      std::make_unique<ElementAreaProto>(touchable_element_area);
}

bool ScriptExecutor::SetProgressActiveStepIdentifier(
    const std::string& active_step_identifier) {
  return ui_delegate_->SetProgressActiveStepIdentifier(active_step_identifier);
}

void ScriptExecutor::SetProgressActiveStep(int active_step) {
  ui_delegate_->SetProgressActiveStep(active_step);
}

void ScriptExecutor::SetProgressVisible(bool visible) {
  ui_delegate_->SetProgressVisible(visible);
}

void ScriptExecutor::SetProgressBarErrorState(bool error) {
  ui_delegate_->SetProgressBarErrorState(error);
}

void ScriptExecutor::SetStepProgressBarConfiguration(
    const ShowProgressBarProto::StepProgressBarConfiguration& configuration) {
  ui_delegate_->SetStepProgressBarConfiguration(configuration);
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

void ScriptExecutor::WaitForDocumentReadyState(
    base::TimeDelta max_wait_time,
    DocumentReadyState min_ready_state,
    const ElementFinder::Result& optional_frame_element,
    base::OnceCallback<void(const ClientStatus&,
                            DocumentReadyState,
                            base::TimeDelta)> callback) {
  if (!current_action_) {
    NOTREACHED() << "must not be called outside of actions";
    return;
  }
  Action::ActionData& current_action_data = current_action_->GetActionData();
  current_action_data.wait_for_document =
      std::make_unique<WaitForDocumentOperation>(
          delegate_, max_wait_time, min_ready_state, optional_frame_element,
          std::move(callback));
  current_action_data.wait_for_document->Run();
}

void ScriptExecutor::WaitUntilDocumentIsInReadyState(
    base::TimeDelta max_wait_time,
    DocumentReadyState min_ready_state,
    const ElementFinder::Result& optional_frame_element,
    base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback) {
  WaitForDocumentReadyState(
      max_wait_time, min_ready_state, optional_frame_element,
      base::BindOnce(&ScriptExecutor::OnWaitForDocumentReadyState,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ScriptExecutor::OnWaitForDocumentReadyState(
    base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback,
    const ClientStatus& status,
    DocumentReadyState ready_state,
    base::TimeDelta wait_time) {
  std::move(callback).Run(status, wait_time);
}

void ScriptExecutor::LoadURL(const GURL& url) {
  delegate_->GetWebController()->LoadURL(url);
}

void ScriptExecutor::Shutdown(bool show_feedback_chip) {
  // The following handles the case where scripts end with tell + stop
  // differently from just stop. TODO(b/806868): Make that difference explicit:
  // add an optional message to stop and update the scripts to use that.
  if (previous_action_type_ == ActionProto::kTell) {
    ui_delegate_->SetShowFeedbackChip(show_feedback_chip);
    at_end_ = SHUTDOWN_GRACEFULLY;
  } else {
    at_end_ = SHUTDOWN;
  }

  // Clear remaining actions, if any. If there is a current action, it is
  // allowed to finish and add its status to |processed_actions_|.
  actions_.resize(processed_actions_.size() +
                  (current_action_ != nullptr ? 1 : 0));
}

void ScriptExecutor::Close() {
  at_end_ = CLOSE_CUSTOM_TAB;
  should_stop_script_ = true;
}

autofill::PersonalDataManager* ScriptExecutor::GetPersonalDataManager() const {
  return delegate_->GetPersonalDataManager();
}

WebsiteLoginManager* ScriptExecutor::GetWebsiteLoginManager() const {
  return delegate_->GetWebsiteLoginManager();
}

password_manager::PasswordChangeSuccessTracker*
ScriptExecutor::GetPasswordChangeSuccessTracker() const {
  return delegate_->GetPasswordChangeSuccessTracker();
}

content::WebContents* ScriptExecutor::GetWebContents() const {
  return delegate_->GetWebContents();
}

ElementStore* ScriptExecutor::GetElementStore() const {
  return element_store_.get();
}

WebController* ScriptExecutor::GetWebController() const {
  return delegate_->GetWebController();
}

std::string ScriptExecutor::GetEmailAddressForAccessTokenAccount() const {
  return delegate_->GetEmailAddressForAccessTokenAccount();
}

ukm::UkmRecorder* ScriptExecutor::GetUkmRecorder() const {
  return delegate_->GetUkmRecorder();
}

void ScriptExecutor::SetDetails(std::unique_ptr<Details> details,
                                base::TimeDelta delay) {
  return ui_delegate_->SetDetails(std::move(details), delay);
}

void ScriptExecutor::AppendDetails(std::unique_ptr<Details> details,
                                   base::TimeDelta delay) {
  return ui_delegate_->AppendDetails(std::move(details), delay);
}

void ScriptExecutor::ClearInfoBox() {
  ui_delegate_->ClearInfoBox();
}

void ScriptExecutor::SetInfoBox(const InfoBox& info_box) {
  ui_delegate_->SetInfoBox(info_box);
}

void ScriptExecutor::SetViewportMode(ViewportMode mode) {
  delegate_->SetViewportMode(mode);
}

ViewportMode ScriptExecutor::GetViewportMode() const {
  return delegate_->GetViewportMode();
}

void ScriptExecutor::SetPeekMode(
    ConfigureBottomSheetProto::PeekMode peek_mode) {
  ui_delegate_->SetPeekMode(peek_mode);
}

ConfigureBottomSheetProto::PeekMode ScriptExecutor::GetPeekMode() const {
  return ui_delegate_->GetPeekMode();
}

void ScriptExecutor::ExpandBottomSheet() {
  return ui_delegate_->ExpandBottomSheet();
}

void ScriptExecutor::CollapseBottomSheet() {
  return ui_delegate_->CollapseBottomSheet();
}

void ScriptExecutor::WaitForWindowHeightChange(
    base::OnceCallback<void(const ClientStatus&)> callback) {
  delegate_->GetWebController()->WaitForWindowHeightChange(std::move(callback));
}

const ClientSettings& ScriptExecutor::GetSettings() const {
  return delegate_->GetSettings();
}

void ScriptExecutor::SetClientSettings(
    const ClientSettingsProto& client_settings) {
  return delegate_->SetClientSettings(client_settings);
}

bool ScriptExecutor::SetForm(
    std::unique_ptr<FormProto> form,
    base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
    base::OnceCallback<void(const ClientStatus&)> cancel_callback) {
  return ui_delegate_->SetForm(std::move(form), std::move(changed_callback),
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
  ui_delegate_->SetGenericUi(std::move(generic_ui),
                             std::move(end_action_callback),
                             std::move(view_inflation_finished_callback));
}

void ScriptExecutor::SetPersistentGenericUi(
    std::unique_ptr<GenericUserInterfaceProto> generic_ui,
    base::OnceCallback<void(const ClientStatus&)>
        view_inflation_finished_callback) {
  ui_delegate_->SetPersistentGenericUi(
      std::move(generic_ui), std::move(view_inflation_finished_callback));
}

void ScriptExecutor::ClearGenericUi() {
  ui_delegate_->ClearGenericUi();
}

void ScriptExecutor::ClearPersistentGenericUi() {
  ui_delegate_->ClearPersistentGenericUi();
}

void ScriptExecutor::SetOverlayBehavior(
    ConfigureUiStateProto::OverlayBehavior overlay_behavior) {
  delegate_->SetOverlayBehavior(overlay_behavior);
}

void ScriptExecutor::MaybeShowSlowWebsiteWarning(
    base::OnceCallback<void(bool)> callback) {
  bool should_show_warning =
      !delegate_->GetSettings().only_show_website_warning_once ||
      !website_warning_already_shown_;
  // MaybeShowSlowWarning is only called if should_sown_warning is true.
  bool warning_was_shown =
      should_show_warning &&
      MaybeShowSlowWarning(
          delegate_->GetSettings().slow_website_message,
          delegate_->GetSettings().enable_slow_website_warnings);
  website_warning_already_shown_ |= warning_was_shown;
  if (callback) {
    std::move(callback).Run(warning_was_shown);
  }
}

void ScriptExecutor::MaybeShowSlowConnectionWarning() {
  bool should_show_warning =
      !delegate_->GetSettings().only_show_connection_warning_once ||
      !connection_warning_already_shown_;
  base::TimeDelta delay = base::Milliseconds(0);
  // MaybeShowSlowWarning is only called if should_sown_warning is true.
  bool warning_was_shown =
      should_show_warning &&
      MaybeShowSlowWarning(
          delegate_->GetSettings().slow_connection_message,
          delegate_->GetSettings().enable_slow_connection_warnings);
  if (warning_was_shown) {
    delay = delegate_->GetSettings().minimum_warning_duration;
  }
  connection_warning_already_shown_ |= warning_was_shown;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ScriptExecutor::ProcessNextAction,
                     weak_ptr_factory_.GetWeakPtr()),
      delay);
}

bool ScriptExecutor::MaybeShowSlowWarning(const std::string& message,
                                          bool enabled) {
  if (message.empty() || !enabled || !delegate_->ShouldShowWarning()) {
    return false;
  }

  if (delegate_->GetSettings().only_show_warning_once &&
      (connection_warning_already_shown_ || website_warning_already_shown_)) {
    return false;
  }

  const std::string& previous_message = GetStatusMessage();
  switch (delegate_->GetSettings().message_mode) {
    case ClientSettingsProto::SlowWarningSettings::CONCATENATE:
      if (previous_message.find(message) == std::string::npos) {
        SetStatusMessage(previous_message + message);
      }
      break;
    case ClientSettingsProto::SlowWarningSettings::REPLACE:
      SetStatusMessage(message);
      break;
    case ClientSettingsProto::SlowWarningSettings::UNKNOWN:
      return false;
  }

  return true;
}

base::WeakPtr<ActionDelegate> ScriptExecutor::GetWeakPtr() const {
  return weak_ptr_factory_.GetWeakPtr();
}

void ScriptExecutor::OnGetActions(base::TimeTicks start_time,
                                  int http_status,
                                  const std::string& response) {
  VLOG(2) << __func__ << " http-status=" << http_status;
  batch_start_time_ = base::TimeTicks::Now();
  const base::TimeDelta& roundtrip_duration = batch_start_time_ - start_time;
  // Doesn't trigger when the script is completed.
  roundtrip_timing_stats_.set_roundtrip_time_ms(
      roundtrip_duration.InMilliseconds());
  bool success =
      http_status == net::HTTP_OK && ProcessNextActionResponse(response);
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

  if (roundtrip_duration < delegate_->GetSettings().slow_roundtrip_threshold) {
    consecutive_slow_roundtrip_counter_ = 0;
  } else {
    consecutive_slow_roundtrip_counter_++;
  }

  if (!actions_.empty()) {
    if (consecutive_slow_roundtrip_counter_ >=
        delegate_->GetSettings().max_consecutive_slow_roundtrips) {
      consecutive_slow_roundtrip_counter_ = 0;
      MaybeShowSlowConnectionWarning();
    } else {
      ProcessNextAction();
    }
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
      this, response, &run_id_, &last_global_payload_, &last_script_payload_,
      &actions_, &scripts, &should_update_scripts);
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
    SetDetails(nullptr, base::TimeDelta());
    ClearPersistentGenericUi();
    should_clean_contextual_ui_on_finish_ = false;
  }

  Result result;
  result.success = success;
  result.at_end = at_end_;
  if (touchable_element_area_) {
    SetTouchableElementArea(*touchable_element_area_);
  }

  RunCallbackWithResult(result);
}

void ScriptExecutor::RunCallbackWithResult(const Result& result) {
  DCHECK(callback_);
  std::move(callback_).Run(result);
}

void ScriptExecutor::ProcessNextAction() {
  // We could get into a strange situation if ProcessNextAction is called before
  // the action was reported as processed, which should not happen. In that case
  // we could have more |processed_actions| than |actions_|.
  if (actions_.size() <= processed_actions_.size()) {
    DCHECK_EQ(actions_.size(), processed_actions_.size());
    VLOG(2) << __func__ << ", get more actions";
    GetNextActions();
    return;
  }

  current_action_ = actions_[processed_actions_.size()].get();
  should_clean_contextual_ui_on_finish_ =
      current_action_->proto().clean_contextual_ui();
  int delay_ms = current_action_->proto().action_delay_ms();
  if (delay_ms > 0) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ScriptExecutor::ProcessCurrentAction,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Milliseconds(delay_ms));
  } else {
    ProcessCurrentAction();
  }
}

void ScriptExecutor::ProcessCurrentAction() {
  DCHECK(current_action_);
  VLOG(2) << "Begin action: " << *current_action_;

  current_action_->GetActionData().navigation_info.set_has_error(
      delegate_->HasNavigationError());

  current_action_->ProcessAction(
      base::BindOnce(&ScriptExecutor::OnProcessedAction,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void ScriptExecutor::GetNextActions() {
  base::TimeTicks get_next_actions_start = base::TimeTicks::Now();
  roundtrip_timing_stats_.set_client_time_ms(
      (get_next_actions_start - batch_start_time_).InMilliseconds());
  VLOG(2) << "Batch timing stats";
  VLOG(2) << "Roundtrip time: " << roundtrip_timing_stats_.roundtrip_time_ms();
  VLOG(2) << "Client execution time: "
          << roundtrip_timing_stats_.client_time_ms();
  delegate_->GetService()->GetNextActions(
      TriggerContext(
          {delegate_->GetTriggerContext(), additional_context_.get()}),
      last_global_payload_, last_script_payload_, processed_actions_,
      roundtrip_timing_stats_,
      base::BindOnce(&ScriptExecutor::OnGetActions,
                     weak_ptr_factory_.GetWeakPtr(), get_next_actions_start));
}

void ScriptExecutor::OnProcessedAction(
    base::TimeTicks start_time,
    std::unique_ptr<ProcessedActionProto> processed_action_proto) {
  DCHECK(current_action_);
  base::TimeDelta run_time = base::TimeTicks::Now() - start_time;
  previous_action_type_ = processed_action_proto->action().action_info_case();
  processed_actions_.emplace_back(*processed_action_proto);

#ifdef NDEBUG
  VLOG(2) << "Action completed";
#else
  VLOG(2) << "Requested delay ms: "
          << processed_action_proto->timing_stats().delay_ms();
  VLOG(2) << "Active time ms: "
          << processed_action_proto->timing_stats().active_time_ms();
  VLOG(2) << "Wait time ms: "
          << processed_action_proto->timing_stats().wait_time_ms();
  VLOG(2) << "Run time: " << run_time;
#endif

  auto& processed_action = processed_actions_.back();
  processed_action.set_run_time_ms(run_time.InMilliseconds());
  *processed_action.mutable_navigation_info() =
      current_action_->GetActionData().navigation_info;

  if (processed_action.status() != ProcessedActionStatusProto::ACTION_APPLIED) {
    VLOG(1) << "Action failed: " << processed_action.status();
    // Remove unexecuted actions, this will cause the |ProcessNextActions| call
    // to immediately ask for new actions.
    actions_.resize(processed_actions_.size());
  }
  current_action_ = nullptr;
  ProcessNextAction();
}

void ScriptExecutor::CheckElementConditionMatches(
    const Selector& selector,
    BatchElementChecker* checker,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  ElementConditionProto condition;
  *condition.mutable_match() = selector.proto;
  condition.set_require_unique_element(false);
  checker->AddElementConditionCheck(
      condition,
      base::BindOnce(&ScriptExecutor::CheckElementMatchesCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ScriptExecutor::CheckElementMatchesCallback(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status,
    const std::vector<std::string>& ignored_payloads,
    const std::vector<std::string>& ignored_tags,
    const base::flat_map<std::string, DomObjectFrameStack>& ignored_elements) {
  std::move(callback).Run(status);
}

void ScriptExecutor::OnShortWaitForElement(
    base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback,
    const ClientStatus& element_status,
    const Result* interrupt_result,
    base::TimeDelta wait_time) {
  // Interrupts cannot run, so should never be reported.
  DCHECK(!interrupt_result);

  std::move(callback).Run(element_status, wait_time);
}

void ScriptExecutor::OnWaitForElementVisibleWithInterrupts(
    base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback,
    const ClientStatus& element_status,
    const Result* interrupt_result,
    base::TimeDelta wait_time) {
  if (interrupt_result) {
    if (!interrupt_result->success) {
      std::move(callback).Run(ClientStatus(INTERRUPT_FAILED), wait_time);
      return;
    }
    if (interrupt_result->at_end != CONTINUE) {
      at_end_ = interrupt_result->at_end;
      should_stop_script_ = true;
      std::move(callback).Run(ClientStatus(MANUAL_FALLBACK), wait_time);
      return;
    }
  }
  std::move(callback).Run(element_status, wait_time);
}

std::ostream& operator<<(std::ostream& out,
                         const ScriptExecutor::Result& result) {
  result.success ? out << "succeeded. " : out << "failed. ";
  out << "at_end = " << result.at_end;
  return out;
}

ProcessedActionStatusDetailsProto& ScriptExecutor::GetLogInfo() {
  return delegate_->GetLogInfo();
}

void ScriptExecutor::RequestUserData(
    const CollectUserDataOptions& options,
    base::OnceCallback<void(bool, const GetUserDataResponseProto&)> callback) {
  auto* service = delegate_->GetService();
  DCHECK(service);

  service->GetUserData(
      options, run_id_,
      base::BindOnce(&ScriptExecutor::OnRequestUserData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ScriptExecutor::OnRequestUserData(
    base::OnceCallback<void(bool, const GetUserDataResponseProto&)> callback,
    int http_status,
    const std::string& response) {
  if (http_status != net::HTTP_OK) {
    std::move(callback).Run(false, GetUserDataResponseProto());
    return;
  }

  GetUserDataResponseProto response_proto;
  bool success = response_proto.ParseFromString(response);
  std::move(callback).Run(success, response_proto);
}

}  // namespace autofill_assistant
