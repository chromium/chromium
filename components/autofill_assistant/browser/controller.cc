// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/controller.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "components/autofill_assistant/browser/actions/collect_user_data_action.h"
#include "components/autofill_assistant/browser/controller_observer.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/service_impl.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/view_layout.pb.h"
#include "components/google/core/common/google_util.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace autofill_assistant {

namespace {

// The initial progress to set when autostarting and showing the "Loading..."
// message.
static constexpr int kAutostartInitialProgress = 5;

// Experiment for toggling the new progress bar.
const char kProgressBarExperiment[] = "4400697";

// Returns true if the state requires a UI to be shown.
//
// Note that the UI might be shown in RUNNING state, even if it doesn't require
// it.
bool StateNeedsUiInRegularScript(AutofillAssistantState state,
                                 bool browse_mode_invisible) {
  switch (state) {
    case AutofillAssistantState::PROMPT:
    case AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT:
    case AutofillAssistantState::MODAL_DIALOG:
    case AutofillAssistantState::STARTING:
      return true;

    case AutofillAssistantState::INACTIVE:
    case AutofillAssistantState::TRACKING:
    case AutofillAssistantState::STOPPED:
    case AutofillAssistantState::RUNNING:
      return false;

    case AutofillAssistantState::BROWSE:
      return browse_mode_invisible;
  }
}

// Same as |StateNeedsUiInRegularScript|, but does not show UI in STARTING
// state.
bool StateNeedsUiInLiteScript(AutofillAssistantState state) {
  switch (state) {
    case AutofillAssistantState::PROMPT:
    case AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT:
    case AutofillAssistantState::MODAL_DIALOG:
      return true;

    case AutofillAssistantState::STARTING:
    case AutofillAssistantState::BROWSE:
    case AutofillAssistantState::INACTIVE:
    case AutofillAssistantState::TRACKING:
    case AutofillAssistantState::STOPPED:
    case AutofillAssistantState::RUNNING:
      return false;
  }
}

// Check whether a domain is a subdomain of another domain.
bool IsSubdomainOf(const std::string& subdomain,
                   const std::string& parent_domain) {
  return base::EndsWith(base::StringPiece(subdomain),
                        base::StringPiece("." + parent_domain),
                        base::CompareCase::INSENSITIVE_ASCII);
}

// Check whether two URLs have the same domain.
bool HasSameDomainAs(const GURL& a, const GURL& b) {
  return a.host() == b.host();
}

// Check whether |subdomain| is a subdomain of a set of domains in |whitelist|.
bool IsInWhitelist(const std::string& subdomain,
                   const std::vector<std::string> whitelist) {
  const GURL subdomain_gurl = GURL(subdomain);
  for (const std::string& parent_domain : whitelist) {
    if (HasSameDomainAs(subdomain_gurl, GURL(parent_domain)) ||
        IsSubdomainOf(subdomain, parent_domain))
      return true;
  }
  return false;
}

}  // namespace

Controller::Controller(content::WebContents* web_contents,
                       Client* client,
                       const base::TickClock* tick_clock,
                       RuntimeManagerImpl* runtime_manager,
                       std::unique_ptr<Service> service)
    : content::WebContentsObserver(web_contents),
      client_(client),
      tick_clock_(tick_clock),
      runtime_manager_(runtime_manager),
      service_(service ? std::move(service)
                       : ServiceImpl::Create(web_contents->GetBrowserContext(),
                                             client_)),
      user_data_(std::make_unique<UserData>()),
      navigating_to_new_document_(web_contents->IsWaitingForResponse()) {
  user_model_.AddObserver(this);
}

Controller::~Controller() {
  user_model_.RemoveObserver(this);
}

const ClientSettings& Controller::GetSettings() {
  return settings_;
}

const GURL& Controller::GetCurrentURL() {
  const GURL& last_committed = web_contents()->GetLastCommittedURL();
  if (!last_committed.is_empty())
    return last_committed;

  return deeplink_url_;
}

const GURL& Controller::GetDeeplinkURL() {
  return deeplink_url_;
}

const GURL& Controller::GetScriptURL() {
  return script_url_;
}

Service* Controller::GetService() {
  return service_.get();
}

WebController* Controller::GetWebController() {
  if (!web_controller_) {
    web_controller_ =
        WebController::CreateForWebContents(web_contents(), &settings_);
  }
  return web_controller_.get();
}

const TriggerContext* Controller::GetTriggerContext() {
  DCHECK(trigger_context_);
  return trigger_context_.get();
}

autofill::PersonalDataManager* Controller::GetPersonalDataManager() {
  return client_->GetPersonalDataManager();
}

WebsiteLoginManager* Controller::GetWebsiteLoginManager() {
  return client_->GetWebsiteLoginManager();
}

content::WebContents* Controller::GetWebContents() {
  return web_contents();
}

std::string Controller::GetEmailAddressForAccessTokenAccount() {
  return client_->GetEmailAddressForAccessTokenAccount();
}

std::string Controller::GetLocale() {
  return client_->GetLocale();
}

void Controller::SetTouchableElementArea(const ElementAreaProto& area) {
  touchable_element_area()->SetFromProto(area);
}

void Controller::SetStatusMessage(const std::string& message) {
  status_message_ = message;
  for (ControllerObserver& observer : observers_) {
    observer.OnStatusMessageChanged(message);
  }
}

std::string Controller::GetStatusMessage() const {
  return status_message_;
}

void Controller::SetBubbleMessage(const std::string& message) {
  bubble_message_ = message;
  for (ControllerObserver& observer : observers_) {
    observer.OnBubbleMessageChanged(message);
  }
}

std::string Controller::GetBubbleMessage() const {
  return bubble_message_;
}

void Controller::SetDetails(std::unique_ptr<Details> details) {
  details_ = std::move(details);
  for (ControllerObserver& observer : observers_) {
    observer.OnDetailsChanged(details_.get());
  }
}

const Details* Controller::GetDetails() const {
  return details_.get();
}

int Controller::GetProgress() const {
  return progress_;
}

base::Optional<int> Controller::GetProgressActiveStep() const {
  return progress_active_step_;
}

base::Optional<ShowProgressBarProto::StepProgressBarConfiguration>
Controller::GetStepProgressBarConfiguration() const {
  return step_progress_bar_configuration_;
}

void Controller::SetInfoBox(const InfoBox& info_box) {
  if (!info_box_) {
    info_box_ = std::make_unique<InfoBox>();
  }
  *info_box_ = info_box;
  for (ControllerObserver& observer : observers_) {
    observer.OnInfoBoxChanged(info_box_.get());
  }
}

void Controller::ClearInfoBox() {
  info_box_.reset();
  for (ControllerObserver& observer : observers_) {
    observer.OnInfoBoxChanged(nullptr);
  }
}

const InfoBox* Controller::GetInfoBox() const {
  return info_box_.get();
}

void Controller::SetProgress(int progress) {
  // Progress can only increase.
  if (progress_ >= progress)
    return;

  progress_ = progress;
  for (ControllerObserver& observer : observers_) {
    observer.OnProgressChanged(progress);
  }
}

bool Controller::SetProgressActiveStepIdentifier(
    const std::string& active_step_identifier) {
  if (!step_progress_bar_configuration_.has_value()) {
    return false;
  }

  auto it = std::find_if(
      step_progress_bar_configuration_->annotated_step_icons().cbegin(),
      step_progress_bar_configuration_->annotated_step_icons().cend(),
      [&](const ShowProgressBarProto::StepProgressBarIcon& icon) {
        return icon.identifier() == active_step_identifier;
      });
  if (it == step_progress_bar_configuration_->annotated_step_icons().cend()) {
    return false;
  }

  SetProgressActiveStep(std::distance(
      step_progress_bar_configuration_->annotated_step_icons().cbegin(), it));
  return true;
}

void Controller::SetProgressActiveStep(int active_step) {
  if (!step_progress_bar_configuration_.has_value()) {
    return;
  }

  // Default step progress bar has 2 steps.
  int max_step = std::max(
      2, step_progress_bar_configuration_->annotated_step_icons().size());

  int new_active_step = active_step;
  if (active_step < 0 || active_step > max_step) {
    new_active_step = max_step;
  }

  // Step can only increase.
  if (progress_active_step_.has_value() &&
      *progress_active_step_ >= new_active_step) {
    return;
  }

  progress_active_step_ = new_active_step;
  for (ControllerObserver& observer : observers_) {
    observer.OnProgressActiveStepChanged(new_active_step);
  }
}

void Controller::SetProgressVisible(bool visible) {
  if (progress_visible_ == visible)
    return;

  progress_visible_ = visible;
  for (ControllerObserver& observer : observers_) {
    observer.OnProgressVisibilityChanged(visible);
  }
}

bool Controller::GetProgressVisible() const {
  return progress_visible_;
}

void Controller::SetStepProgressBarConfiguration(
    const ShowProgressBarProto::StepProgressBarConfiguration& configuration) {
  step_progress_bar_configuration_ = configuration;
  if (!configuration.annotated_step_icons().empty() &&
      progress_active_step_.has_value() &&
      configuration.annotated_step_icons().size() < *progress_active_step_) {
    progress_active_step_ = configuration.annotated_step_icons().size();
  }
  for (ControllerObserver& observer : observers_) {
    observer.OnStepProgressBarConfigurationChanged(configuration);
    if (progress_active_step_.has_value()) {
      observer.OnProgressActiveStepChanged(*progress_active_step_);
    }
    observer.OnProgressBarErrorStateChanged(progress_bar_error_state_);
  }
}

void Controller::SetProgressBarErrorState(bool error) {
  if (progress_bar_error_state_ == error) {
    return;
  }

  progress_bar_error_state_ = error;
  for (ControllerObserver& observer : observers_) {
    observer.OnProgressBarErrorStateChanged(error);
  }
}

bool Controller::GetProgressBarErrorState() const {
  return progress_bar_error_state_;
}

const std::vector<UserAction>& Controller::GetUserActions() const {
  static const base::NoDestructor<std::vector<UserAction>> no_user_actions_;
  return user_actions_ ? *user_actions_ : *no_user_actions_;
}

void Controller::SetUserActions(
    std::unique_ptr<std::vector<UserAction>> user_actions) {
  if (user_actions) {
    SetDefaultChipType(user_actions.get());
  }
  user_actions_ = std::move(user_actions);
  for (ControllerObserver& observer : observers_) {
    observer.OnUserActionsChanged(GetUserActions());
  }
}

bool Controller::IsNavigatingToNewDocument() {
  return navigating_to_new_document_;
}

bool Controller::HasNavigationError() {
  return navigation_error_;
}

void Controller::RequireUI() {
  if (ui_shown_)
    return;

  needs_ui_ = true;
  client_->AttachUI();
}

void Controller::SetUiShown(bool shown) {
  ui_shown_ = shown;
  runtime_manager_->SetUIState(shown ? UIState::kShown : UIState::kNotShown);
}

void Controller::SetGenericUi(
    std::unique_ptr<GenericUserInterfaceProto> generic_ui,
    base::OnceCallback<void(const ClientStatus&)> end_action_callback,
    base::OnceCallback<void(const ClientStatus&)>
        view_inflation_finished_callback) {
  generic_user_interface_ = std::move(generic_ui);
  basic_interactions_.SetEndActionCallback(std::move(end_action_callback));
  basic_interactions_.SetViewInflationFinishedCallback(
      std::move(view_inflation_finished_callback));
  for (ControllerObserver& observer : observers_) {
    observer.OnGenericUserInterfaceChanged(generic_user_interface_.get());
  }
}

void Controller::ClearGenericUi() {
  generic_user_interface_.reset();
  basic_interactions_.ClearCallbacks();
  for (ControllerObserver& observer : observers_) {
    observer.OnGenericUserInterfaceChanged(nullptr);
  }
}

void Controller::SetBrowseModeInvisible(bool invisible) {
  browse_mode_invisible_ = invisible;
}

void Controller::AddNavigationListener(
    ScriptExecutorDelegate::NavigationListener* listener) {
  navigation_listeners_.AddObserver(listener);
}

void Controller::RemoveNavigationListener(
    ScriptExecutorDelegate::NavigationListener* listener) {
  navigation_listeners_.RemoveObserver(listener);
}

void Controller::AddListener(ScriptExecutorDelegate::Listener* listener) {
  listeners_.AddObserver(listener);
}

void Controller::RemoveListener(ScriptExecutorDelegate::Listener* listener) {
  listeners_.RemoveObserver(listener);
}

void Controller::SetExpandSheetForPromptAction(bool expand) {
  expand_sheet_for_prompt_action_ = expand;
}

void Controller::SetBrowseDomainsWhitelist(std::vector<std::string> domains) {
  browse_domains_whitelist_ = std::move(domains);
}

bool Controller::PerformUserActionWithContext(
    int index,
    std::unique_ptr<TriggerContext> context) {
  if (!user_actions_ || index < 0 ||
      static_cast<size_t>(index) >= user_actions_->size()) {
    NOTREACHED() << "Invalid user action index: " << index;
    return false;
  }

  if (!(*user_actions_)[index].enabled()) {
    NOTREACHED() << "Action at index " << index << " is disabled.";
    return false;
  }

  UserAction user_action = std::move((*user_actions_)[index]);
  SetUserActions(nullptr);
  user_action.Call(std::move(context));
  event_handler_.DispatchEvent(
      {EventProto::kOnUserActionCalled, user_action.identifier()});
  return true;
}

void Controller::SetViewportMode(ViewportMode mode) {
  if (mode == viewport_mode_)
    return;

  viewport_mode_ = mode;
  for (ControllerObserver& observer : observers_) {
    observer.OnViewportModeChanged(mode);
  }
}

void Controller::SetPeekMode(ConfigureBottomSheetProto::PeekMode peek_mode) {
  if (peek_mode == peek_mode_)
    return;

  peek_mode_ = peek_mode;
  for (ControllerObserver& observer : observers_) {
    observer.OnPeekModeChanged(peek_mode);
  }
}

void Controller::ExpandBottomSheet() {
  for (ControllerObserver& observer : observers_) {
    // TODO(crbug/806868): The interface here and in some of the other On*
    // events should be coming from the UI layer, not the controller. Or at
    // least be renamed to something like On*Requested.
    observer.OnExpandBottomSheet();
  }
}

void Controller::CollapseBottomSheet() {
  for (ControllerObserver& observer : observers_) {
    // TODO(crbug/806868): The interface here and in some of the other On*
    // events should be coming from the UI layer, not the controller. Or at
    // least be renamed to something like On*Requested.
    observer.OnCollapseBottomSheet();
  }
}

const FormProto* Controller::GetForm() const {
  return form_.get();
}

const FormProto::Result* Controller::GetFormResult() const {
  return form_result_.get();
}

bool Controller::SetForm(
    std::unique_ptr<FormProto> form,
    base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
    base::OnceCallback<void(const ClientStatus&)> cancel_callback) {
  form_.reset();
  form_result_.reset();
  form_changed_callback_ = base::DoNothing();
  form_cancel_callback_ = base::DoNothing::Once<const ClientStatus&>();

  if (!form) {
    for (ControllerObserver& observer : observers_) {
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
        // Intentionally no default case to make compilation fail if a new value
        // was added to the enum but not to this list.
    }
  }

  // Form is valid.
  form_ = std::move(form);
  form_result_ = std::move(form_result);
  form_changed_callback_ = changed_callback;
  form_cancel_callback_ = std::move(cancel_callback);

  // Call the callback with initial result.
  form_changed_callback_.Run(form_result_.get());

  for (ControllerObserver& observer : observers_) {
    observer.OnFormChanged(form_.get(), form_result_.get());
  }
  return true;
}

void Controller::SetCounterValue(int input_index,
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

void Controller::SetChoiceSelected(int input_index,
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

UserModel* Controller::GetUserModel() {
  return &user_model_;
}

EventHandler* Controller::GetEventHandler() {
  return &event_handler_;
}

bool Controller::ShouldPromptActionExpandSheet() const {
  return expand_sheet_for_prompt_action_;
}

BasicInteractions* Controller::GetBasicInteractions() {
  return &basic_interactions_;
}

const GenericUserInterfaceProto* Controller::GetGenericUiProto() const {
  return generic_user_interface_.get();
}

void Controller::AddObserver(ControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void Controller::RemoveObserver(const ControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void Controller::DispatchEvent(const EventHandler::EventKey& key) {
  event_handler_.DispatchEvent(key);
}

ViewportMode Controller::GetViewportMode() {
  return viewport_mode_;
}

ConfigureBottomSheetProto::PeekMode Controller::GetPeekMode() {
  return peek_mode_;
}

BottomSheetState Controller::GetBottomSheetState() {
  return bottom_sheet_state_;
}

void Controller::SetBottomSheetState(BottomSheetState state) {
  bottom_sheet_state_ = state;
}

bool Controller::IsTabSelected() {
  return tab_selected_;
}

void Controller::SetTabSelected(bool selected) {
  tab_selected_ = selected;
}

void Controller::SetOverlayColors(std::unique_ptr<OverlayColors> colors) {
  overlay_colors_ = std::move(colors);
  if (overlay_colors_) {
    for (ControllerObserver& observer : observers_) {
      observer.OnOverlayColorsChanged(*overlay_colors_);
    }
  } else {
    OverlayColors default_colors;
    for (ControllerObserver& observer : observers_) {
      observer.OnOverlayColorsChanged(default_colors);
    }
  }
}

void Controller::GetOverlayColors(OverlayColors* colors) const {
  if (!overlay_colors_)
    return;
  *colors = *overlay_colors_;
}

const ClientSettings& Controller::GetClientSettings() const {
  return settings_;
}

void Controller::ShutdownIfNecessary() {
  if (!tracking_) {
    // We expect the DropOutReason to be already reported when we reach this
    // point and therefore the reason we pass here in the argument should be
    // ignored.
    client_->Shutdown(Metrics::DropOutReason::UI_CLOSED_UNEXPECTEDLY);
  }
}

void Controller::ReportNavigationStateChanged() {
  for (auto& listener : navigation_listeners_) {
    listener.OnNavigationStateChanged();
  }
}

void Controller::EnterStoppedState() {
  if (script_tracker_)
    script_tracker_->StopScript();

  ClearInfoBox();
  SetDetails(nullptr);
  SetUserActions(nullptr);
  SetCollectUserDataOptions(nullptr);
  SetForm(nullptr, base::DoNothing(), base::DoNothing());
  EnterState(AutofillAssistantState::STOPPED);
}

bool Controller::EnterState(AutofillAssistantState state) {
  if (state_ == state)
    return false;

  VLOG(2) << __func__ << ": " << state_ << " -> " << state;

  // The only valid way of leaving the STOPPED state is to go back to tracking
  // mode - or going back to RUNNING if it was a recoverable STOPPED state.
  DCHECK(
      state_ != AutofillAssistantState::STOPPED ||
      (state == AutofillAssistantState::TRACKING && tracking_) ||
      (state == AutofillAssistantState::RUNNING && can_recover_from_stopped_));
  if (state_ == AutofillAssistantState::STOPPED) {
    can_recover_from_stopped_ = false;
  }
  state_ = state;

  for (ControllerObserver& observer : observers_) {
    observer.OnStateChanged(state);
  }

  if (!ui_shown_ && StateNeedsUI(state)) {
    RequireUI();
  } else if (needs_ui_ && state == AutofillAssistantState::TRACKING) {
    needs_ui_ = false;
  } else if (browse_mode_invisible_ && ui_shown_ &&
             state == AutofillAssistantState::BROWSE) {
    needs_ui_ = false;
    client_->DestroyUI();
  }

  if (ShouldCheckScripts()) {
    GetOrCheckScripts();
  } else {
    StopPeriodicScriptChecks();
  }
  return true;
}

void Controller::SetOverlayBehavior(
    ConfigureUiStateProto::OverlayBehavior overlay_behavior) {
  overlay_behavior_ = overlay_behavior;
  for (ControllerObserver& observer : observers_) {
    observer.OnShouldShowOverlayChanged(ShouldShowOverlay());
  }
}

void Controller::SetWebControllerForTest(
    std::unique_ptr<WebController> web_controller) {
  web_controller_ = std::move(web_controller);
}

void Controller::OnUrlChange() {
  if (state_ == AutofillAssistantState::STOPPED) {
    PerformDelayedShutdownIfNecessary();
    return;
  }
  user_model_.SetCurrentURL(GetCurrentURL());
  GetOrCheckScripts();
}

bool Controller::ShouldCheckScripts() {
  return state_ == AutofillAssistantState::TRACKING ||
         state_ == AutofillAssistantState::STARTING ||
         state_ == AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT ||
         ((state_ == AutofillAssistantState::PROMPT ||
           state_ == AutofillAssistantState::BROWSE) &&
          (!script_tracker_ || !script_tracker_->running()));
}

void Controller::GetOrCheckScripts() {
  if (!ShouldCheckScripts())
    return;

  const GURL& url = GetCurrentURL();
  if (!HasSameDomainAs(script_url_, url)) {
    StopPeriodicScriptChecks();
    script_url_ = url;
#ifdef NDEBUG
    VLOG(2) << "GetScripts for <redacted>";
#else
    VLOG(2) << "GetScripts for " << script_url_.host();
#endif

    GetService()->GetScriptsForUrl(
        url, *trigger_context_,
        base::BindOnce(&Controller::OnGetScripts, base::Unretained(this), url));
  } else {
    script_tracker()->CheckScripts();
    StartPeriodicScriptChecks();
  }
}

void Controller::StartPeriodicScriptChecks() {
  periodic_script_check_count_ = settings_.periodic_script_check_count;
  // If periodic checks are running, setting periodic_script_check_count_ keeps
  // them running longer.
  if (periodic_script_check_scheduled_)
    return;
  periodic_script_check_scheduled_ = true;
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&Controller::OnPeriodicScriptCheck,
                     weak_ptr_factory_.GetWeakPtr()),
      settings_.periodic_script_check_interval);
}

void Controller::StopPeriodicScriptChecks() {
  periodic_script_check_count_ = 0;
}

void Controller::OnPeriodicScriptCheck() {
  if (periodic_script_check_count_ > 0) {
    periodic_script_check_count_--;
  }

  if (periodic_script_check_count_ <= 0 && !allow_autostart()) {
    DCHECK_EQ(0, periodic_script_check_count_);
    periodic_script_check_scheduled_ = false;
    return;
  }

  if (allow_autostart() && !autostart_timeout_script_path_.empty() &&
      tick_clock_->NowTicks() >= absolute_autostart_timeout_) {
    VLOG(1) << __func__ << " giving up waiting on autostart.";
    std::string script_path = autostart_timeout_script_path_;
    autostart_timeout_script_path_.clear();
    periodic_script_check_scheduled_ = false;
    ExecuteScript(script_path, /* start_message= */ "", /* needs_ui= */ false,
                  TriggerContext::CreateEmpty(), state_);
    return;
  }

  script_tracker()->CheckScripts();
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&Controller::OnPeriodicScriptCheck,
                     weak_ptr_factory_.GetWeakPtr()),
      settings_.periodic_script_check_interval);
}

void Controller::OnGetScripts(const GURL& url,
                              bool result,
                              const std::string& response) {
  if (state_ == AutofillAssistantState::STOPPED)
    return;

  // If the domain of the current URL changed since the request was sent, the
  // response is not relevant anymore and can be safely discarded.
  if (!HasSameDomainAs(script_url_, url))
    return;

  if (!result) {
#ifdef NDEBUG
    VLOG(1) << "Failed to get assistant scripts for <redacted>";
#else
    VLOG(1) << "Failed to get assistant scripts for " << script_url_.host();
#endif
    OnFatalError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR),
                 Metrics::DropOutReason::GET_SCRIPTS_FAILED);
    return;
  }

  SupportsScriptResponseProto response_proto;
  if (!response_proto.ParseFromString(response)) {
#ifdef NDEBUG
    VLOG(2) << __func__ << " from <redacted> returned unparseable response";
#else
    VLOG(2) << __func__ << " from " << script_url_.host() << " returned "
            << "unparseable response";
#endif
    OnFatalError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR),
                 Metrics::DropOutReason::GET_SCRIPTS_UNPARSABLE);
    return;
  }
  if (response_proto.has_client_settings()) {
    settings_.UpdateFromProto(response_proto.client_settings());
    for (ControllerObserver& observer : observers_) {
      observer.OnClientSettingsChanged(settings_);
    }
  }

  std::vector<std::unique_ptr<Script>> scripts;
  for (const auto& script_proto : response_proto.scripts()) {
    ProtocolUtils::AddScript(script_proto, &scripts);
  }

  autostart_timeout_script_path_ =
      response_proto.script_timeout_error().script_path();
  autostart_timeout_ = base::TimeDelta::FromMilliseconds(
      response_proto.script_timeout_error().timeout_ms());
  if (allow_autostart())
    absolute_autostart_timeout_ = tick_clock_->NowTicks() + autostart_timeout_;

#ifdef NDEBUG
  VLOG(2) << __func__ << " from <redacted> returned " << scripts.size()
          << " scripts";
#else
  VLOG(2) << __func__ << " from " << script_url_.host() << " returned "
          << scripts.size() << " scripts";
#endif

  if (VLOG_IS_ON(3)) {
    for (const auto& script : scripts) {
      // Strip domain from beginning if possible (redundant with log above).
      auto pos = script->handle.path.find(script_url_.host());
      if (pos == 0) {
        DVLOG(3) << "\t"
                 << script->handle.path.substr(script_url_.host().length());
      } else {
        DVLOG(3) << "\t" << script->handle.path;
      }
    }
  }

  if (scripts.empty()) {
    script_tracker()->SetScripts({});

    if (state_ == AutofillAssistantState::TRACKING) {
      OnFatalError(
          l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR),
          Metrics::DropOutReason::NO_SCRIPTS);
      return;
    }
    OnNoRunnableScriptsForPage();
  }

  script_tracker()->SetScripts(std::move(scripts));
  GetOrCheckScripts();
}

void Controller::ExecuteScript(const std::string& script_path,
                               const std::string& start_message,
                               bool needs_ui,
                               std::unique_ptr<TriggerContext> context,
                               AutofillAssistantState end_state) {
  DCHECK(!script_tracker()->running());

  if (!start_message.empty())
    SetStatusMessage(start_message);

  EnterState(AutofillAssistantState::RUNNING);
  if (needs_ui)
    RequireUI();

  touchable_element_area()->Clear();

  // Runnable scripts will be checked and reported if necessary after executing
  // the script.
  script_tracker_->ClearRunnableScripts();
  SetUserActions(nullptr);
  // TODO(crbug.com/806868): Consider making ClearRunnableScripts part of
  // ExecuteScripts to simplify the controller.
  script_tracker()->ExecuteScript(
      script_path, user_data_.get(), std::move(context),
      base::BindOnce(&Controller::OnScriptExecuted,
                     // script_tracker_ is owned by Controller.
                     base::Unretained(this), script_path, end_state));
}

void Controller::OnScriptExecuted(const std::string& script_path,
                                  AutofillAssistantState end_state,
                                  const ScriptExecutor::Result& result) {
  if (!result.success) {
#ifdef NDEBUG
    VLOG(1) << "Failed to execute script";
#else
    DVLOG(1) << "Failed to execute script " << script_path;
#endif

    OnScriptError(
        l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR),
        Metrics::DropOutReason::SCRIPT_FAILED);
    return;
  }

  if (result.touchable_element_area) {
    touchable_element_area()->SetFromProto(*result.touchable_element_area);
  }

  switch (result.at_end) {
    case ScriptExecutor::SHUTDOWN:
      if (!tracking_) {
        client_->Shutdown(Metrics::DropOutReason::SCRIPT_SHUTDOWN);
        return;
      }
      end_state = AutofillAssistantState::TRACKING;
      break;

    case ScriptExecutor::SHUTDOWN_GRACEFULLY:
      if (!tracking_) {
        EnterStoppedState();
        RecordDropOutOrShutdown(Metrics::DropOutReason::SCRIPT_SHUTDOWN);
        return;
      }
      end_state = AutofillAssistantState::TRACKING;
      break;

    case ScriptExecutor::CLOSE_CUSTOM_TAB:
      for (ControllerObserver& observer : observers_) {
        observer.CloseCustomTab();
      }
      if (!tracking_) {
        client_->Shutdown(Metrics::DropOutReason::CUSTOM_TAB_CLOSED);
        return;
      }
      end_state = AutofillAssistantState::TRACKING;
      return;

    case ScriptExecutor::CONTINUE:
      break;

    default:
      VLOG(1) << "Unexpected value for at_end: " << result.at_end;
      break;
  }
  EnterState(end_state);
}

bool Controller::MaybeAutostartScript(
    const std::vector<ScriptHandle>& runnable_scripts) {
  // Under specific conditions, we can directly run a non-interrupt script
  // without first displaying it. This is meant to work only at the very
  // beginning, when no scripts have run, and only if there's exactly one
  // autostartable script.
  if (!allow_autostart())
    return false;

  int autostart_index = -1;
  for (size_t i = 0; i < runnable_scripts.size(); i++) {
    if (runnable_scripts[i].autostart) {
      if (autostart_index != -1) {
        // To many autostartable scripts.
        return false;
      }
      autostart_index = i;
    }
  }

  if (autostart_index == -1)
    return false;

  // Copying the strings is necessary, as ExecuteScript will invalidate
  // runnable_scripts by calling ScriptTracker::ClearRunnableScripts.
  //
  // TODO(b/138367403): Cleanup this dangerous issue.
  std::string path = runnable_scripts[autostart_index].path;
  std::string start_message = runnable_scripts[autostart_index].start_message;
  bool needs_ui = runnable_scripts[autostart_index].needs_ui;
  ExecuteScript(path, start_message, needs_ui, TriggerContext::CreateEmpty(),
                AutofillAssistantState::PROMPT);
  return true;
}

void Controller::InitFromParameters() {
  auto details = std::make_unique<Details>();
  if (details->UpdateFromParameters(*trigger_context_))
    SetDetails(std::move(details));

  const base::Optional<std::string> overlay_color =
      trigger_context_->GetOverlayColors();
  if (overlay_color) {
    std::unique_ptr<OverlayColors> colors = std::make_unique<OverlayColors>();
    std::vector<std::string> color_strings =
        base::SplitString(overlay_color.value(), ":", base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_ALL);
    if (color_strings.size() > 0) {
      colors->background = color_strings[0];
    }
    if (color_strings.size() > 1) {
      colors->highlight_border = color_strings[1];
    }
    // Ignore other colors, to allow future versions of the client to support
    // setting more colors.

    SetOverlayColors(std::move(colors));
  }
  const base::Optional<std::string> password_change_username =
      trigger_context_->GetPasswordChangeUsername();
  if (password_change_username) {
    DCHECK(
        GetCurrentURL().is_valid());  // At least |deeplink_url_| must be set.
    user_data_->selected_login_.emplace(GetCurrentURL().GetOrigin(),
                                        *password_change_username);
  }

  if (trigger_context_->HasExperimentId(kProgressBarExperiment)) {
    ShowProgressBarProto::StepProgressBarConfiguration mock_configuration;
    mock_configuration.set_use_step_progress_bar(true);
    SetStepProgressBarConfiguration(mock_configuration);
  }

  user_model_.SetCurrentURL(GetCurrentURL());
}

void Controller::Track(std::unique_ptr<TriggerContext> trigger_context,
                       base::OnceCallback<void()> on_first_check_done) {
  tracking_ = true;

  if (state_ == AutofillAssistantState::INACTIVE) {
    trigger_context_ = std::move(trigger_context);
    InitFromParameters();
    EnterState(AutofillAssistantState::TRACKING);
  }

  if (on_first_check_done) {
    if (has_run_first_check_) {
      std::move(on_first_check_done).Run();
    } else {
      on_has_run_first_check_.emplace_back(std::move(on_first_check_done));
    }
  }
}

bool Controller::HasRunFirstCheck() const {
  return tracking_ && has_run_first_check_;
}

bool Controller::Start(const GURL& deeplink_url,
                       std::unique_ptr<TriggerContext> trigger_context) {
  if (state_ != AutofillAssistantState::INACTIVE &&
      state_ != AutofillAssistantState::TRACKING) {
    return false;
  }

  trigger_context_ = std::move(trigger_context);
  deeplink_url_ = deeplink_url;
  InitFromParameters();

  // Force a re-evaluation of the script, to get a chance to autostart.
  if (state_ == AutofillAssistantState::TRACKING)
    script_tracker_->ClearRunnableScripts();

  if (IsNavigatingToNewDocument()) {
    start_after_navigation_ = base::BindOnce(
        &Controller::ShowFirstMessageAndStart, weak_ptr_factory_.GetWeakPtr());
  } else {
    ShowFirstMessageAndStart();
  }
  return true;
}

void Controller::ShowFirstMessageAndStart() {
  // Only show default status message if necessary. Scripts started by lite
  // scripts that also showed the onboarding do not show the loading message.
  if (status_message_.empty() &&
      !(GetTriggerContext()->is_onboarding_shown() &&
        GetTriggerContext()->WasStartedByTriggerScript())) {
    SetStatusMessage(
        l10n_util::GetStringFUTF8(IDS_AUTOFILL_ASSISTANT_LOADING,
                                  base::UTF8ToUTF16(GetCurrentURL().host())));
  }
  if (step_progress_bar_configuration_.has_value() &&
      step_progress_bar_configuration_->use_step_progress_bar()) {
    SetProgressActiveStep(0);
  } else {
    SetProgress(kAutostartInitialProgress);
  }
  EnterState(AutofillAssistantState::STARTING);
}

AutofillAssistantState Controller::GetState() const {
  return state_;
}

bool Controller::ShouldShowOverlay() const {
  return overlay_behavior_ == ConfigureUiStateProto::DEFAULT;
}

void Controller::OnScriptSelected(const ScriptHandle& handle,
                                  std::unique_ptr<TriggerContext> context) {
  ExecuteScript(handle.path, handle.start_message, handle.needs_ui,
                std::move(context),
                state_ == AutofillAssistantState::TRACKING
                    ? AutofillAssistantState::TRACKING
                    : AutofillAssistantState::PROMPT);
}

void Controller::OnUserInteractionInsideTouchableArea() {
  GetOrCheckScripts();
}

std::string Controller::GetDebugContext() {
  base::Value dict(base::Value::Type::DICTIONARY);

  dict.SetKey("status", base::Value(status_message_));
  if (trigger_context_) {
    std::vector<base::Value> parameters_js;
    for (const auto& parameter : trigger_context_->GetParameters()) {
      base::Value parameter_js = base::Value(base::Value::Type::DICTIONARY);
      parameter_js.SetKey(parameter.first, base::Value(parameter.second));
      parameters_js.push_back(std::move(parameter_js));
    }
    dict.SetKey("parameters", base::Value(parameters_js));
  }
  dict.SetKey("scripts", script_tracker()->GetDebugContext());

  if (details_)
    dict.SetKey("details", details_->GetDebugContext());

  std::string output_js;
  base::JSONWriter::Write(dict, &output_js);
  return output_js;
}

const CollectUserDataOptions* Controller::GetCollectUserDataOptions() const {
  return collect_user_data_options_;
}

const UserData* Controller::GetUserData() const {
  return user_data_.get();
}

void Controller::OnCollectUserDataContinueButtonClicked() {
  if (!collect_user_data_options_ || !user_data_)
    return;

  auto callback = std::move(collect_user_data_options_->confirm_callback);

  SetCollectUserDataOptions(nullptr);
  std::move(callback).Run(user_data_.get(), &user_model_);
}

void Controller::OnCollectUserDataAdditionalActionTriggered(int index) {
  if (!collect_user_data_options_)
    return;

  auto callback =
      std::move(collect_user_data_options_->additional_actions_callback);
  SetCollectUserDataOptions(nullptr);
  std::move(callback).Run(index, user_data_.get(), &user_model_);
}

void Controller::OnTextLinkClicked(int link) {
  if (!user_data_)
    return;

  auto callback = std::move(collect_user_data_options_->terms_link_callback);
  SetCollectUserDataOptions(nullptr);
  std::move(callback).Run(link, user_data_.get(), &user_model_);
}

void Controller::OnFormActionLinkClicked(int link) {
  if (form_cancel_callback_ && form_result_ != nullptr) {
    form_result_->set_link(link);
    form_changed_callback_.Run(form_result_.get());
    std::move(form_cancel_callback_).Run(ClientStatus(ACTION_APPLIED));
  }
}

void Controller::SetDateTimeRangeStartDate(
    const base::Optional<DateProto>& date) {
  if (!user_data_)
    return;

  if (user_data_->date_time_range_start_date_.has_value() && date.has_value() &&
      CollectUserDataAction::CompareDates(
          *user_data_->date_time_range_start_date_, *date) == 0) {
    return;
  }

  user_data_->date_time_range_start_date_ = date;
  for (ControllerObserver& observer : observers_) {
    observer.OnUserDataChanged(user_data_.get(),
                               UserData::FieldChange::DATE_TIME_RANGE_START);
  }

  if (CollectUserDataAction::SanitizeDateTimeRange(
          &user_data_->date_time_range_start_date_,
          &user_data_->date_time_range_start_timeslot_,
          &user_data_->date_time_range_end_date_,
          &user_data_->date_time_range_end_timeslot_,
          *collect_user_data_options_,
          /* change_start = */ false)) {
    for (ControllerObserver& observer : observers_) {
      observer.OnUserDataChanged(user_data_.get(),
                                 UserData::FieldChange::DATE_TIME_RANGE_END);
    }
  }

  UpdateCollectUserDataActions();
}

void Controller::SetDateTimeRangeStartTimeSlot(
    const base::Optional<int>& timeslot_index) {
  if (!user_data_)
    return;

  if (user_data_->date_time_range_start_timeslot_.has_value() &&
      timeslot_index.has_value() &&
      *user_data_->date_time_range_start_timeslot_ == *timeslot_index) {
    return;
  }

  user_data_->date_time_range_start_timeslot_ = timeslot_index;
  for (ControllerObserver& observer : observers_) {
    observer.OnUserDataChanged(user_data_.get(),
                               UserData::FieldChange::DATE_TIME_RANGE_START);
  }

  if (CollectUserDataAction::SanitizeDateTimeRange(
          &user_data_->date_time_range_start_date_,
          &user_data_->date_time_range_start_timeslot_,
          &user_data_->date_time_range_end_date_,
          &user_data_->date_time_range_end_timeslot_,
          *collect_user_data_options_,
          /* change_start = */ false)) {
    for (ControllerObserver& observer : observers_) {
      observer.OnUserDataChanged(user_data_.get(),
                                 UserData::FieldChange::DATE_TIME_RANGE_END);
    }
  }

  UpdateCollectUserDataActions();
}

void Controller::SetDateTimeRangeEndDate(
    const base::Optional<DateProto>& date) {
  if (!user_data_)
    return;

  if (user_data_->date_time_range_end_date_.has_value() && date.has_value() &&
      CollectUserDataAction::CompareDates(
          *user_data_->date_time_range_end_date_, *date) == 0) {
    return;
  }

  user_data_->date_time_range_end_date_ = date;
  for (ControllerObserver& observer : observers_) {
    observer.OnUserDataChanged(user_data_.get(),
                               UserData::FieldChange::DATE_TIME_RANGE_END);
  }

  if (CollectUserDataAction::SanitizeDateTimeRange(
          &user_data_->date_time_range_start_date_,
          &user_data_->date_time_range_start_timeslot_,
          &user_data_->date_time_range_end_date_,
          &user_data_->date_time_range_end_timeslot_,
          *collect_user_data_options_,
          /* change_start = */ true)) {
    for (ControllerObserver& observer : observers_) {
      observer.OnUserDataChanged(user_data_.get(),
                                 UserData::FieldChange::DATE_TIME_RANGE_START);
    }
  }

  UpdateCollectUserDataActions();
}

void Controller::SetDateTimeRangeEndTimeSlot(
    const base::Optional<int>& timeslot_index) {
  if (!user_data_)
    return;

  if (user_data_->date_time_range_end_timeslot_.has_value() &&
      timeslot_index.has_value() &&
      *user_data_->date_time_range_end_timeslot_ == *timeslot_index) {
    return;
  }

  user_data_->date_time_range_end_timeslot_ = timeslot_index;
  for (ControllerObserver& observer : observers_) {
    observer.OnUserDataChanged(user_data_.get(),
                               UserData::FieldChange::DATE_TIME_RANGE_END);
  }

  if (CollectUserDataAction::SanitizeDateTimeRange(
          &user_data_->date_time_range_start_date_,
          &user_data_->date_time_range_start_timeslot_,
          &user_data_->date_time_range_end_date_,
          &user_data_->date_time_range_end_timeslot_,
          *collect_user_data_options_,
          /* change_start = */ true)) {
    for (ControllerObserver& observer : observers_) {
      observer.OnUserDataChanged(user_data_.get(),
                                 UserData::FieldChange::DATE_TIME_RANGE_START);
    }
  }

  UpdateCollectUserDataActions();
}

void Controller::SetAdditionalValue(const std::string& client_memory_key,
                                    const ValueProto& value) {
  if (!user_data_)
    return;
  auto it = user_data_->additional_values_.find(client_memory_key);
  if (it == user_data_->additional_values_.end()) {
    NOTREACHED() << client_memory_key << " not found";
    return;
  }
  it->second = value;
  UpdateCollectUserDataActions();
  for (ControllerObserver& observer : observers_) {
    observer.OnUserDataChanged(user_data_.get(),
                               UserData::FieldChange::ADDITIONAL_VALUES);
  }
}

void Controller::SetShippingAddress(
    std::unique_ptr<autofill::AutofillProfile> address) {
  if (collect_user_data_options_ == nullptr) {
    return;
  }

  DCHECK(!collect_user_data_options_->shipping_address_name.empty());
  SetProfile(collect_user_data_options_->shipping_address_name,
             UserData::FieldChange::SHIPPING_ADDRESS, std::move(address));
}

void Controller::SetContactInfo(
    std::unique_ptr<autofill::AutofillProfile> profile) {
  if (collect_user_data_options_ == nullptr) {
    return;
  }

  DCHECK(!collect_user_data_options_->contact_details_name.empty());
  SetProfile(collect_user_data_options_->contact_details_name,
             UserData::FieldChange::CONTACT_PROFILE, std::move(profile));
}

void Controller::SetCreditCard(
    std::unique_ptr<autofill::CreditCard> card,
    std::unique_ptr<autofill::AutofillProfile> billing_profile) {
  if (user_data_ == nullptr || collect_user_data_options_ == nullptr) {
    return;
  }

  DCHECK(!collect_user_data_options_->billing_address_name.empty());

  user_data_->selected_card_ = std::move(card);
  for (ControllerObserver& observer : observers_) {
    observer.OnUserDataChanged(user_data_.get(), UserData::FieldChange::CARD);
  }
  SetProfile(collect_user_data_options_->billing_address_name,
             UserData::FieldChange::BILLING_ADDRESS,
             std::move(billing_profile));
}

void Controller::SetProfile(
    const std::string& key,
    UserData::FieldChange field_change,
    std::unique_ptr<autofill::AutofillProfile> profile) {
  if (user_data_ == nullptr) {
    return;
  }

  auto it = user_data_->selected_addresses_.find(key);
  if (it != user_data_->selected_addresses_.end()) {
    user_data_->selected_addresses_.erase(it);
  }
  if (profile != nullptr) {
    user_data_->selected_addresses_.emplace(key, std::move(profile));
  }

  for (ControllerObserver& observer : observers_) {
    observer.OnUserDataChanged(user_data_.get(), field_change);
  }
  UpdateCollectUserDataActions();
}

void Controller::SetTermsAndConditions(
    TermsAndConditionsState terms_and_conditions) {
  if (!user_data_)
    return;

  user_data_->terms_and_conditions_ = terms_and_conditions;
  UpdateCollectUserDataActions();
  for (ControllerObserver& observer : observers_) {
    observer.OnUserDataChanged(user_data_.get(),
                               UserData::FieldChange::TERMS_AND_CONDITIONS);
  }
}

void Controller::SetLoginOption(std::string identifier) {
  if (!user_data_ || !collect_user_data_options_)
    return;

  user_data_->login_choice_identifier_.assign(identifier);
  UpdateCollectUserDataActions();
  for (ControllerObserver& observer : observers_) {
    observer.OnUserDataChanged(user_data_.get(),
                               UserData::FieldChange::LOGIN_CHOICE);
  }
}

void Controller::UpdateCollectUserDataActions() {
  // TODO(crbug.com/806868): This method uses #SetUserActions(), which means
  // that updating the PR action buttons will also clear the suggestions. We
  // should update the action buttons only if there are use cases of PR +
  // suggestions.
  if (!collect_user_data_options_ || !user_data_) {
    SetUserActions(nullptr);
    return;
  }

  bool confirm_button_enabled = CollectUserDataAction::IsUserDataComplete(
      *user_data_, user_model_, *collect_user_data_options_);

  UserAction confirm(collect_user_data_options_->confirm_action);
  confirm.SetEnabled(confirm_button_enabled);
  if (confirm_button_enabled) {
    confirm.SetCallback(
        base::BindOnce(&Controller::OnCollectUserDataContinueButtonClicked,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  auto user_actions = std::make_unique<std::vector<UserAction>>();
  user_actions->emplace_back(std::move(confirm));

  // Add additional actions.
  for (size_t i = 0; i < collect_user_data_options_->additional_actions.size();
       ++i) {
    auto action = collect_user_data_options_->additional_actions[i];
    user_actions->push_back({action});
    user_actions->back().SetCallback(
        base::BindOnce(&Controller::OnCollectUserDataAdditionalActionTriggered,
                       weak_ptr_factory_.GetWeakPtr(), i));
  }

  SetUserActions(std::move(user_actions));
}

void Controller::GetTouchableArea(std::vector<RectF>* area) const {
  if (touchable_element_area_)
    touchable_element_area_->GetTouchableRectangles(area);
}

void Controller::GetRestrictedArea(std::vector<RectF>* area) const {
  if (touchable_element_area_)
    touchable_element_area_->GetRestrictedRectangles(area);
}

void Controller::GetVisualViewport(RectF* visual_viewport) const {
  if (touchable_element_area_)
    touchable_element_area_->GetVisualViewport(visual_viewport);
}

void Controller::OnScriptError(const std::string& error_message,
                               Metrics::DropOutReason reason) {
  if (state_ == AutofillAssistantState::STOPPED)
    return;

  // For lite scripts, don't attach the UI on error, and don't show an error
  // while shutting down.
  if (!IsRunningLiteScript()) {
    RequireUI();
    SetStatusMessage(error_message);
    SetProgressBarErrorState(true);
  }

  EnterStoppedState();

  if (tracking_) {
    EnterState(AutofillAssistantState::TRACKING);
    return;
  }

  RecordDropOutOrShutdown(reason);
}

void Controller::OnFatalError(const std::string& error_message,
                              Metrics::DropOutReason reason) {
  LOG(ERROR) << "Autofill Assistant has encountered a fatal error and is "
                "shutting down, reason="
             << reason;
  if (state_ == AutofillAssistantState::STOPPED)
    return;

  SetStatusMessage(error_message);
  SetProgressBarErrorState(true);
  EnterStoppedState();

  // If we haven't managed to check the set of scripts yet at this point, we
  // never will.
  MaybeReportFirstCheckDone();

  if (tracking_ && HasSameDomainAs(script_url_, GetCurrentURL())) {
    // When tracking the controller should stays until the browser has navigated
    // away from the last domain that was checked to be able to tell callers
    // that the set of user actions is empty.
    delayed_shutdown_reason_ = reason;
    return;
  }

  RecordDropOutOrShutdown(reason);
}

void Controller::RecordDropOutOrShutdown(Metrics::DropOutReason reason) {
  // If there is an UI, we wait for it to be closed before shutting down (the UI
  // will call |ShutdownIfNecessary|).
  // Lite scripts go away immediately, even if UI is currently being shown.
  if (client_->HasHadUI() && !IsRunningLiteScript()) {
    // We report right away to make sure we don't lose this reason if the client
    // is unexpectedly destroyed while the error message is showing (for example
    // if the tab is closed).
    client_->RecordDropOut(reason);
  } else {
    client_->Shutdown(reason);
  }
}

void Controller::OnStop(const std::string& message,
                        const std::string& button_label) {
  DCHECK(state_ != AutofillAssistantState::STOPPED);

  can_recover_from_stopped_ = true;
  for (auto& listener : listeners_) {
    listener.OnPause(message, button_label);
  }
}

void Controller::PerformDelayedShutdownIfNecessary() {
  if (delayed_shutdown_reason_ &&
      !HasSameDomainAs(script_url_, GetCurrentURL())) {
    Metrics::DropOutReason reason = delayed_shutdown_reason_.value();
    delayed_shutdown_reason_ = base::nullopt;
    tracking_ = false;
    client_->Shutdown(reason);
  }
}

void Controller::MaybeReportFirstCheckDone() {
  if (has_run_first_check_)
    return;

  has_run_first_check_ = true;

  while (!on_has_run_first_check_.empty()) {
    std::move(on_has_run_first_check_.back()).Run();
    on_has_run_first_check_.pop_back();
  }
}

void Controller::OnNoRunnableScriptsForPage() {
  if (script_tracker()->running())
    return;

  switch (state_) {
    case AutofillAssistantState::STARTING:
      // We're still waiting for the set of initial scripts, but either didn't
      // get any scripts or didn't get scripts that could possibly become
      // runnable with a DOM change.
      OnScriptError(
          l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR),
          Metrics::DropOutReason::NO_INITIAL_SCRIPTS);
      break;

    case AutofillAssistantState::PROMPT:
    case AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT:
      // The user has navigated to a page that has no scripts or the scripts
      // have reached a state from which they cannot recover through a DOM
      // change.
      OnScriptError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_GIVE_UP),
                    Metrics::DropOutReason::NO_SCRIPTS);
      break;

    default:
      // Always having a set of scripts to potentially run is not required in
      // other states, for example in BROWSE state.
      break;
  }
}

void Controller::OnRunnableScriptsChanged(
    const std::vector<ScriptHandle>& runnable_scripts) {
  base::ScopedClosureRunner report_first_check;
  if (!has_run_first_check_) {
    // Only report first check done once we're done processing the given set of
    // scripts - whatever the outcome - so callers can see that outcome in the
    // state of the controller.
    report_first_check.ReplaceClosure(
        base::BindOnce(&Controller::MaybeReportFirstCheckDone,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Script selection is disabled when a script is already running. We will
  // check again and maybe update when the current script has finished.
  if (script_tracker()->running() || state_ == AutofillAssistantState::STOPPED)
    return;

  if (MaybeAutostartScript(runnable_scripts)) {
    return;
  }

  // Show the initial prompt if available.
  for (const auto& script : runnable_scripts) {
    // runnable_scripts is ordered by priority.
    if (!script.initial_prompt.empty()) {
      SetStatusMessage(script.initial_prompt);
      break;
    }
  }

  // Update the set of user actions to report.
  auto user_actions = std::make_unique<std::vector<UserAction>>();
  for (const auto& script : runnable_scripts) {
    UserAction user_action;
    user_action.chip() = script.chip;
    user_action.direct_action() = script.direct_action;
    if (!user_action.has_triggers())
      continue;

    user_action.SetCallback(base::BindOnce(
        &Controller::OnScriptSelected, weak_ptr_factory_.GetWeakPtr(), script));
    user_actions->emplace_back(std::move(user_action));
  }

  // Change state, if necessary.
  switch (state_) {
    case AutofillAssistantState::TRACKING:
    case AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT:
    case AutofillAssistantState::PROMPT:
    case AutofillAssistantState::BROWSE:
      // Don't change state
      break;

    case AutofillAssistantState::STARTING:
      if (!user_actions->empty())
        EnterState(AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT);
      break;

    default:
      if (!user_actions->empty())
        EnterState(AutofillAssistantState::PROMPT);
  }
  SetUserActions(std::move(user_actions));
}

void Controller::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                               const GURL& validated_url) {
  // validated_url might not be the page URL. Ignore it and always check the
  // last committed url.
  OnUrlChange();
}

void Controller::ExpectNavigation() {
  expect_navigation_ = true;
}

void Controller::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!navigating_to_new_document_) {
    navigating_to_new_document_ = true;
    ReportNavigationStateChanged();
  }

  // The navigation is expected, do not check for errors below.
  if (expect_navigation_) {
    expect_navigation_ = false;
    return;
  }

  if (state_ == AutofillAssistantState::STOPPED &&
      !navigation_handle->IsRendererInitiated() &&
      !navigation_handle->WasServerRedirect()) {
    if (can_recover_from_stopped_) {
      // Usually when in STOPPED (e.g. through |OnScriptError|) the
      // |DropOutReason| has been recorded. In the case of a recoverable stop,
      // e.g. with the back button, this is not the case. Record the reason as
      // |NAVIGATION| here.
      client_->Shutdown(Metrics::DropOutReason::NAVIGATION);
      return;
    }

    ShutdownIfNecessary();
    return;
  }

  // In lite scripts, navigations are allowed (the lite script will fail if the
  // trigger condition stops being true).
  //
  // In regular scripts, the following types of navigations are allowed for the
  // main frame, when in PROMPT state:
  //  - first-time URL load
  //  - script-directed navigation, while a script is running unless
  //    there's a touchable area.
  //  - server redirections, which might happen outside of a script, but
  //    because of a load triggered by a previously-running script.
  //  - same-document modifications, which might happen automatically
  //  - javascript-initiated navigation or refresh
  //  - navigation by clicking on a link
  //  In the last two cases, autofill assistant might still give up later on if
  //  it discovers that the new page has no scripts.
  //
  // Everything else, such as going back to a previous page, or refreshing the
  // page is considered an end condition. If going back to a previous page is
  // required, consider using the BROWSE state instead.
  if (!IsRunningLiteScript() && state_ == AutofillAssistantState::PROMPT &&
      web_contents()->GetLastCommittedURL().is_valid() &&
      !navigation_handle->WasServerRedirect() &&
      !navigation_handle->IsRendererInitiated()) {
    OnScriptError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_GIVE_UP),
                  Metrics::DropOutReason::NAVIGATION);
    return;
  }

  // When in RUNNING state, all renderer initiated navigation is allowed,
  // user initiated navigation will cause an error.
  if (state_ == AutofillAssistantState::RUNNING &&
      !navigation_handle->WasServerRedirect() &&
      !navigation_handle->IsRendererInitiated()) {
    OnScriptError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_GIVE_UP),
                  Metrics::DropOutReason::NAVIGATION_WHILE_RUNNING);
    return;
  }

  // Note that BROWSE state end conditions are in DidFinishNavigation, in order
  // to be able to properly evaluate the committed url.
}

void Controller::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(b/159871774): Rethink how we handle navigation events. The early
  // return here may prevent us from updating |navigating_to_new_document_|.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted() || !IsNavigatingToNewDocument()) {
    return;
  }

  bool is_successful =
      !navigation_handle->IsErrorPage() &&
      navigation_handle->GetNetErrorCode() == net::OK &&
      navigation_handle->GetResponseHeaders() &&
      (navigation_handle->GetResponseHeaders()->response_code() / 100) == 2;
  navigation_error_ = !is_successful;
  navigating_to_new_document_ = false;

  // When in BROWSE state, stop autofill assistant if the user navigates away
  // from the original assisted domain. Subdomains of the original domain are
  // supported.
  if (state_ == AutofillAssistantState::BROWSE) {
    auto current_host = web_contents()->GetLastCommittedURL().host();
    auto script_host = script_url_.host();
    if (current_host != script_host &&
        !IsSubdomainOf(current_host, script_host) &&
        !IsInWhitelist(current_host, browse_domains_whitelist_)) {
      OnScriptError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_GIVE_UP),
                    Metrics::DropOutReason::DOMAIN_CHANGE_DURING_BROWSE_MODE);
    }
  }
  // When in STOPPED state, entered by an unexpected DidStartNavigation or
  // domain change while in BROWSE state (above), and the new URL is on a
  // Google property, destroy the UI immediately.
  if (state_ == AutofillAssistantState::STOPPED &&
      google_util::IsGoogleDomainUrl(
          web_contents()->GetLastCommittedURL(), google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS)) {
    client_->DestroyUI();
  }

  if (start_after_navigation_) {
    std::move(start_after_navigation_).Run();
  } else {
    ReportNavigationStateChanged();

    if (is_successful) {
      OnUrlChange();
    }
  }
}

void Controller::DocumentAvailableInMainFrame() {
  OnUrlChange();
}

void Controller::RenderProcessGone(base::TerminationStatus status) {
  client_->Shutdown(Metrics::DropOutReason::RENDER_PROCESS_GONE);
}

void Controller::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  if (NeedsUI() &&
      base::FeatureList::IsEnabled(features::kAutofillAssistantChromeEntry)) {
    // Show UI again when re-focused in case the web contents moved activity.
    // This is only enabled when tab-switching is enabled.
    client_->AttachUI();
  }
}

void Controller::OnValueChanged(const std::string& identifier,
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

void Controller::OnTouchableAreaChanged(
    const RectF& visual_viewport,
    const std::vector<RectF>& touchable_areas,
    const std::vector<RectF>& restricted_areas) {
  for (ControllerObserver& observer : observers_) {
    observer.OnTouchableAreaChanged(visual_viewport, touchable_areas,
                                    restricted_areas);
  }
}

void Controller::SetCollectUserDataOptions(CollectUserDataOptions* options) {
  DCHECK(!options ||
         (options->confirm_callback && options->additional_actions_callback &&
          options->terms_link_callback));

  if (collect_user_data_options_ == nullptr && options == nullptr)
    return;

  collect_user_data_options_ = options;
  UpdateCollectUserDataActions();
  for (ControllerObserver& observer : observers_) {
    observer.OnCollectUserDataOptionsChanged(collect_user_data_options_);
    observer.OnUserDataChanged(user_data_.get(), UserData::FieldChange::ALL);
  }
}

void Controller::SetLastSuccessfulUserDataOptions(
    std::unique_ptr<CollectUserDataOptions> collect_user_data_options) {
  last_collect_user_data_options_ = std::move(collect_user_data_options);
}

const CollectUserDataOptions* Controller::GetLastSuccessfulUserDataOptions()
    const {
  return last_collect_user_data_options_.get();
}

void Controller::WriteUserData(
    base::OnceCallback<void(UserData*, UserData::FieldChange*)>
        write_callback) {
  UserData::FieldChange field_change = UserData::FieldChange::NONE;
  std::move(write_callback).Run(user_data_.get(), &field_change);
  if (field_change == UserData::FieldChange::NONE) {
    return;
  }
  for (ControllerObserver& observer : observers_) {
    observer.OnUserDataChanged(user_data_.get(), field_change);
  }
  UpdateCollectUserDataActions();
}

bool Controller::StateNeedsUI(AutofillAssistantState state) {
  if (IsRunningLiteScript()) {
    return StateNeedsUiInLiteScript(state);
  }
  return StateNeedsUiInRegularScript(state, browse_mode_invisible_);
}

bool Controller::IsRunningLiteScript() const {
  return service_ ? service_->IsLiteService() : false;
}

ElementArea* Controller::touchable_element_area() {
  if (!touchable_element_area_) {
    touchable_element_area_ = std::make_unique<ElementArea>(this);
    touchable_element_area_->SetOnUpdate(base::BindRepeating(
        &Controller::OnTouchableAreaChanged, weak_ptr_factory_.GetWeakPtr()));
  }
  return touchable_element_area_.get();
}

ScriptTracker* Controller::script_tracker() {
  if (!script_tracker_) {
    script_tracker_ = std::make_unique<ScriptTracker>(/* delegate= */ this,
                                                      /* listener= */ this);
  }
  return script_tracker_.get();
}

}  // namespace autofill_assistant
