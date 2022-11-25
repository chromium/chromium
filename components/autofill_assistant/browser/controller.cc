// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/controller.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "components/autofill_assistant/browser/controller_observer.h"
#include "components/autofill_assistant/browser/display_strings_util.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/service/service_impl.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/url_utils.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/view_layout.pb.h"
#include "components/google/core/common/google_util.h"
#include "components/password_manager/core/browser/password_change_success_tracker_impl.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace autofill_assistant {

Controller::Controller(content::WebContents* web_contents,
                       Client* client,
                       const base::TickClock* tick_clock,
                       base::WeakPtr<RuntimeManager> runtime_manager,
                       std::unique_ptr<Service> service,
                       std::unique_ptr<WebController> web_controller,
                       ukm::UkmRecorder* ukm_recorder)
    : content::WebContentsObserver(web_contents),
      client_(client),
      tick_clock_(tick_clock),
      runtime_manager_(runtime_manager),
      service_(service ? std::move(service)
                       : ServiceImpl::Create(web_contents->GetBrowserContext(),
                                             client_)),
      web_controller_(std::move(web_controller)),
      navigating_to_new_document_(web_contents->IsWaitingForResponse()),
      ukm_recorder_(ukm_recorder) {}

Controller::~Controller() {
  // Record failure, iff an earlier call didn't already record.
  MaybeRecordFlowFinishedMetrics(Metrics::FlowFinishedState::DESTROYED);
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

WebController* Controller::GetWebController() {
  if (!web_controller_) {
    web_controller_ = WebController::CreateForWebContents(
        web_contents(), &user_data_, &log_info_,
        base::FeatureList::IsEnabled(
            autofill_assistant::features::
                kAutofillAssistantFullJsSnippetStackTraces));
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

password_manager::PasswordChangeSuccessTracker*
Controller::GetPasswordChangeSuccessTracker() {
  return client_->GetPasswordChangeSuccessTracker();
}

content::WebContents* Controller::GetWebContents() {
  return web_contents();
}

const std::string Controller::GetLocale() {
  return client_->GetLocale();
}

void Controller::SetJsFlowLibrary(const std::string& js_flow_library) {
  if (js_flow_library.empty()) {
    return;
  }

  GetJsFlowDevtoolsWrapper()->SetJsFlowLibrary(js_flow_library);
  service_->UpdateJsFlowLibraryLoaded(!js_flow_library.empty());
}

JsFlowDevtoolsWrapper* Controller::GetJsFlowDevtoolsWrapper() {
  if (!js_flow_devtools_wrapper_) {
    js_flow_devtools_wrapper_ = std::make_unique<JsFlowDevtoolsWrapper>(
        GetWebContents()->GetBrowserContext());
  }

  return js_flow_devtools_wrapper_.get();
}

std::string Controller::GetEmailAddressForAccessTokenAccount() {
  return client_->GetEmailAddressForAccessTokenAccount();
}

ukm::UkmRecorder* Controller::GetUkmRecorder() {
  return ukm_recorder_;
}

void Controller::SetTouchableElementArea(const ElementAreaProto& area) {
  touchable_element_area()->SetFromProto(area);
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
  if (trigger_context_ && trigger_context_->GetIsExternallyTriggered()) {
    // UIState can only be modified by non-headless runs. For headless runs, the
    // client is in charge of keeping the UIState value updated.
    return;
  }
  ui_shown_ = shown;
  if (runtime_manager_) {
    // By default, browsing features are suppressed during `UIState::kShown`.
    // Therefore set a special state if no suppression is desired.
    if (shown && trigger_context_ &&
        !trigger_context_->GetSuppressBrowsingFeatures()) {
      runtime_manager_->SetUIState(
          UIState::kShownWithoutBrowsingFeatureSuppression);
    } else {
      runtime_manager_->SetUIState(shown ? UIState::kShown
                                         : UIState::kNotShown);
    }
  }

  for (ControllerObserver& observer : observers_) {
    observer.OnUiShownChanged(shown);
  }
}

void Controller::SetBrowseModeInvisible(bool invisible) {
  browse_mode_invisible_ = invisible;
}

bool Controller::ShouldShowWarning() {
  return state_ == AutofillAssistantState::RUNNING ||
         state_ == AutofillAssistantState::PROMPT;
}

ProcessedActionStatusDetailsProto& Controller::GetLogInfo() {
  return log_info_;
}

bool Controller::MustUseBackendData() const {
  return client_->MustUseBackendData();
}

Client* Controller::GetClient() const {
  return client_.get();
}

bool Controller::IsXmlSigned(const std::string& xml_string) const {
  return client_->IsXmlSigned(xml_string);
}

const std::vector<std::string> Controller::ExtractValuesFromSingleTagXml(
    const std::string& xml_string,
    const std::vector<std::string>& keys) const {
  return client_->ExtractValuesFromSingleTagXml(xml_string, keys);
}

void Controller::AddNavigationListener(
    ScriptExecutorDelegate::NavigationListener* listener) {
  navigation_listeners_.AddObserver(listener);
}

void Controller::RemoveNavigationListener(
    ScriptExecutorDelegate::NavigationListener* listener) {
  navigation_listeners_.RemoveObserver(listener);
}

void Controller::SetBrowseDomainsAllowlist(std::vector<std::string> domains) {
  browse_domains_allowlist_ = std::move(domains);
}

void Controller::SetViewportMode(ViewportMode mode) {
  if (mode == viewport_mode_)
    return;

  viewport_mode_ = mode;
  for (ControllerObserver& observer : observers_) {
    observer.OnViewportModeChanged(mode);
  }
}

void Controller::SetClientSettings(const ClientSettingsProto& client_settings) {
  settings_.UpdateFromProto(client_settings);
  for (ControllerObserver& observer : observers_) {
    observer.OnClientSettingsChanged(settings_);
  }
}

UserModel* Controller::GetUserModel() {
  return &user_model_;
}

UserData* Controller::GetUserData() {
  return &user_data_;
}

void Controller::AddObserver(ControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void Controller::RemoveObserver(const ControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

ViewportMode Controller::GetViewportMode() {
  return viewport_mode_;
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

void Controller::SetOverlayBehavior(
    ConfigureUiStateProto::OverlayBehavior overlay_behavior) {
  overlay_behavior_ = overlay_behavior;
  for (ControllerObserver& observer : observers_) {
    observer.OnShouldShowOverlayChanged(ShouldShowOverlay());
  }
}

bool Controller::ShouldShowOverlay() const {
  return overlay_behavior_ == ConfigureUiStateProto::DEFAULT;
}

const ClientSettings& Controller::GetClientSettings() const {
  return settings_;
}

void Controller::ShutdownIfNecessary() {
  // We expect the DropOutReason to be already reported when we reach this
  // point and therefore the reason we pass here in the argument should be
  // ignored.
  Shutdown(Metrics::DropOutReason::UI_CLOSED_UNEXPECTEDLY);
}

void Controller::EnterBrowseModeForShutdown() {
  browse_mode_invisible_ = true;
  EnterState(AutofillAssistantState::BROWSE);
}

void Controller::ReportNavigationStateChanged() {
  for (auto& listener : navigation_listeners_) {
    listener.OnNavigationStateChanged();
  }
}

void Controller::EnterStoppedState() {
  if (script_tracker_)
    script_tracker_->StopScript();
  SetStoppedUI();
  EnterState(AutofillAssistantState::STOPPED);

  // Record failure, iff an earlier call didn't already record.
  MaybeRecordFlowFinishedMetrics(Metrics::FlowFinishedState::FAILURE);
}

void Controller::SetStoppedUI() {
  for (ControllerObserver& observer : observers_) {
    observer.OnStop();
  }
}

bool Controller::EnterState(AutofillAssistantState state) {
  if (state_ == state)
    return false;

  VLOG(2) << __func__ << ": " << state_ << " -> " << state;

  // Leaving the STOPPED state is not allowed.
  DCHECK(state_ != AutofillAssistantState::STOPPED);
  state_ = state;

  bool should_suppress_keyboard = ShouldSuppressKeyboard();
  SuppressKeyboard(should_suppress_keyboard);
  for (ControllerObserver& observer : observers_) {
    observer.OnKeyboardSuppressionStateChanged(should_suppress_keyboard);
    observer.OnStateChanged(state);
  }

  if (!ui_shown_ && StateNeedsUI(state)) {
    RequireUI();
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

void Controller::SetWebControllerForTest(
    std::unique_ptr<WebController> web_controller) {
  web_controller_ = std::move(web_controller);
}

void Controller::OnUrlChange() {
  user_model_.SetCurrentURL(GetCurrentURL());
  GetOrCheckScripts();
}

bool Controller::ShouldCheckScripts() {
  return state_ == AutofillAssistantState::STARTING ||
         ((state_ == AutofillAssistantState::PROMPT ||
           state_ == AutofillAssistantState::BROWSE) &&
          (!script_tracker_ || !script_tracker_->running()));
}

void Controller::GetOrCheckScripts() {
  if (!ShouldCheckScripts())
    return;

  const GURL& url = GetCurrentURL();
  if (script_url_.host() != url.host()) {
    StopPeriodicScriptChecks();
    script_url_ = url;
#ifdef NDEBUG
    VLOG(2) << "GetScripts for <redacted>";
#else
    VLOG(2) << "GetScripts for " << script_url_.host();
#endif
    MaybeUpdateClientContextAndGetScriptsForUrl(url);
  } else {
    script_tracker()->CheckScripts();
    StartPeriodicScriptChecks();
  }
}

void Controller::MaybeUpdateClientContextAndGetScriptsForUrl(const GURL& url) {
  DCHECK(trigger_context_);
  GetScriptsForUrl(url);
}

void Controller::GetScriptsForUrl(const GURL& url) {
  service_->GetScriptsForUrl(
      url, *trigger_context_,
      base::BindOnce(&Controller::OnGetScripts, weak_ptr_factory_.GetWeakPtr(),
                     url));
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

  if (periodic_script_check_count_ <= 0 &&
      (!allow_autostart() || autostart_timeout_script_path_.empty())) {
    DCHECK_EQ(0, periodic_script_check_count_);
    periodic_script_check_scheduled_ = false;

    if (allow_autostart()) {
      OnNoRunnableScriptsForPage();
    }
    return;
  }

  if (allow_autostart() && !autostart_timeout_script_path_.empty() &&
      tick_clock_->NowTicks() >= absolute_autostart_timeout_) {
    VLOG(1) << __func__ << " giving up waiting on autostart.";
    std::string script_path = autostart_timeout_script_path_;
    autostart_timeout_script_path_.clear();
    periodic_script_check_scheduled_ = false;
    ExecuteScript(script_path, /* start_message= */ "", /* needs_ui= */ false,
                  std::make_unique<TriggerContext>(), state_);
    return;
  }

  script_tracker()->CheckScripts();
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&Controller::OnPeriodicScriptCheck,
                     weak_ptr_factory_.GetWeakPtr()),
      settings_.periodic_script_check_interval);
}

void Controller::OnGetScripts(
    const GURL& url,
    int http_status,
    const std::string& response,
    const ServiceRequestSender::ResponseInfo& response_info) {
  if (state_ == AutofillAssistantState::STOPPED)
    return;

  // If the domain of the current URL changed since the request was sent, the
  // response is not relevant anymore and can be safely discarded.
  if (script_url_.host() != url.host())
    return;

  if (http_status != net::HTTP_OK) {
#ifdef NDEBUG
    VLOG(1) << "Failed to get assistant scripts for <redacted>, http-status="
            << http_status;
#else
    VLOG(1) << "Failed to get assistant scripts for " << script_url_.host()
            << ", http-status=" << http_status;
#endif
    OnFatalError(
        GetDisplayStringUTF8(ClientSettingsProto::DEFAULT_ERROR, GetSettings()),
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
    OnFatalError(
        GetDisplayStringUTF8(ClientSettingsProto::DEFAULT_ERROR, GetSettings()),
        Metrics::DropOutReason::GET_SCRIPTS_UNPARSABLE);
    return;
  }

  if (response_proto.has_client_settings()) {
    SetClientSettings(response_proto.client_settings());
  }
  if (response_proto.has_script_store_config()) {
    service_->SetScriptStoreConfig(response_proto.script_store_config());
  }
  std::vector<std::unique_ptr<Script>> scripts;
  for (const auto& script_proto : response_proto.scripts()) {
    ProtocolUtils::AddScript(script_proto, &scripts);
  }

  autostart_timeout_script_path_ =
      response_proto.script_timeout_error().script_path();
  autostart_timeout_ =
      base::Milliseconds(response_proto.script_timeout_error().timeout_ms());
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

  if (needs_ui) {
    RequireUI();
  }
  EnterState(AutofillAssistantState::RUNNING);

  for (ControllerObserver& observer : observers_) {
    observer.OnExecuteScript(start_message);
  }

  touchable_element_area()->Clear();
  // Runnable scripts will be checked and reported if necessary after executing
  // the script.
  script_tracker_->ClearRunnableScripts();

  script_tracker()->ExecuteScript(
      script_path, &user_data_, std::move(context),
      base::BindOnce(&Controller::OnScriptExecuted,
                     // script_tracker_ is owned by Controller.
                     base::Unretained(this), script_path, end_state));
}

void Controller::OnScriptExecuted(const std::string& script_path,
                                  AutofillAssistantState end_state,
                                  const ScriptExecutor::Result& result) {
  MaybeRecordFlowFinishedMetrics(result.success
                                     ? Metrics::FlowFinishedState::SUCCESS
                                     : Metrics::FlowFinishedState::FAILURE);

  if (!result.success) {
#ifdef NDEBUG
    VLOG(1) << "Failed to execute script";
#else
    DVLOG(1) << "Failed to execute script " << script_path;
#endif

    OnScriptError(
        GetDisplayStringUTF8(ClientSettingsProto::DEFAULT_ERROR, GetSettings()),
        Metrics::DropOutReason::SCRIPT_FAILED);
    return;
  }

  switch (result.at_end) {
    case ScriptExecutor::SHUTDOWN:
      Shutdown(Metrics::DropOutReason::SCRIPT_SHUTDOWN);
      return;
    case ScriptExecutor::SHUTDOWN_GRACEFULLY:
      EnterStoppedState();
      RecordDropOutOrShutdown(Metrics::DropOutReason::SCRIPT_SHUTDOWN);
      return;
    case ScriptExecutor::CLOSE_CUSTOM_TAB:
      for (ControllerObserver& observer : observers_) {
        observer.CloseCustomTab();
      }
      Shutdown(Metrics::DropOutReason::CUSTOM_TAB_CLOSED);
      return;
    case ScriptExecutor::CONTINUE:
      // Do nothing.
      break;
    default:
      VLOG(1) << "Unexpected value for at_end: " << result.at_end;
      break;
  }
  EnterState(end_state);
}

void Controller::MaybeAutostartScript(
    const std::vector<ScriptHandle>& runnable_scripts) {
  // We are still waiting for preconditions to match.
  if (runnable_scripts.empty())
    return;

  int autostart_index = -1;
  for (size_t i = 0; i < runnable_scripts.size(); i++) {
    if (runnable_scripts[i].autostart) {
      if (autostart_index != -1) {
        OnScriptError(GetDisplayStringUTF8(ClientSettingsProto::DEFAULT_ERROR,
                                           GetSettings()),
                      Metrics::DropOutReason::MULTIPLE_AUTOSTARTABLE_SCRIPTS);
        return;
      }
      autostart_index = i;
    }
  }

  if (autostart_index == -1) {
    return;
  }

  // Copying the strings is necessary, as ExecuteScript will invalidate
  // runnable_scripts by calling ScriptTracker::ClearRunnableScripts.
  //
  // TODO(b/138367403): Cleanup this dangerous issue.
  std::string path = runnable_scripts[autostart_index].path;
  std::string start_message = runnable_scripts[autostart_index].start_message;
  bool needs_ui = runnable_scripts[autostart_index].needs_ui;
  // TODO(b/204037940): remove prompt state after script execution.
  ExecuteScript(path, start_message, needs_ui,
                std::make_unique<TriggerContext>(),
                AutofillAssistantState::PROMPT);
}

void Controller::InitFromParameters() {
  trigger_context_->GetScriptParameters().WriteToUserData(&user_data_);

  const absl::optional<std::string> overlay_color =
      trigger_context_->GetScriptParameters().GetOverlayColors();
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

  const absl::optional<std::string> password_change_username =
      trigger_context_->GetScriptParameters().GetPasswordChangeUsername();
  if (password_change_username) {
    DCHECK(GetDeeplinkURL().is_valid());  // |deeplink_url_| must be set.
    user_data_.selected_login_.emplace(
        GetDeeplinkURL().DeprecatedGetOriginAsURL(), *password_change_username);

    // We only start password change success tracking here if the run was
    // started from the Google Password Manager. The other cases are
    // handled directly in the UI.
    if (trigger_context_->GetScriptParameters().GetCaller().value_or(0) ==
        static_cast<int>(
            Metrics::AutofillAssistantCaller::GOOGLE_PASSWORD_MANAGER)) {
      GetPasswordChangeSuccessTracker()->OnChangePasswordFlowStarted(
          user_data_.selected_login_->origin,
          user_data_.selected_login_->username,
          password_manager::PasswordChangeSuccessTracker::StartEvent::
              kAutomatedFlow,
          password_manager::PasswordChangeSuccessTracker::EntryPoint::
              kLeakCheckInSettings);
    }
  }

  user_model_.SetCurrentURL(GetCurrentURL());

  service_->SetDisableRpcSigning(
      trigger_context_->GetScriptParameters().GetDisableRpcSigning());
}

bool Controller::Start(const GURL& deeplink_url,
                       std::unique_ptr<TriggerContext> trigger_context) {
  if (state_ != AutofillAssistantState::INACTIVE) {
    return false;
  }

  trigger_context_ = std::move(trigger_context);
  deeplink_url_ = deeplink_url;
  InitFromParameters();

  // Shut down immediately if the certificate is insecure.
  if (ShutdownIfCertificateInsecure()) {
    return false;
  }

  if (IsNavigatingToNewDocument()) {
    start_after_navigation_ = base::BindOnce(
        &Controller::ShowFirstMessageAndStart, weak_ptr_factory_.GetWeakPtr());
  } else {
    ShowFirstMessageAndStart();
  }
  return true;
}

bool Controller::NeedsUI() const {
  return needs_ui_;
}

void Controller::ShowFirstMessageAndStart() {
  for (ControllerObserver& observer : observers_) {
    observer.OnStart(*GetTriggerContext());
  }
  EnterState(AutofillAssistantState::STARTING);
}

void Controller::Shutdown(Metrics::DropOutReason reason) {
  client_->Shutdown(reason);
}

AutofillAssistantState Controller::GetState() const {
  return state_;
}

bool Controller::ShouldSuppressKeyboard() const {
  // Return early if keyboard suppression is turned off.
  if (trigger_context_ && !trigger_context_->GetSuppressBrowsingFeatures()) {
    return false;
  }

  switch (state_) {
    case AutofillAssistantState::STARTING:
    case AutofillAssistantState::RUNNING:
      return true;
    case AutofillAssistantState::PROMPT:
    case AutofillAssistantState::BROWSE:
    case AutofillAssistantState::MODAL_DIALOG:
    case AutofillAssistantState::STOPPED:
    case AutofillAssistantState::INACTIVE:
      return false;
  }
}

base::Value Controller::GetDebugContext() {
  base::Value::Dict dict;

  if (trigger_context_) {
    base::Value::List parameters_js;
    for (const auto& parameter :
         trigger_context_->GetScriptParameters().ToProto()) {
      base::Value::Dict parameter_js;
      parameter_js.Set(parameter.name(), parameter.value());
      parameters_js.Append(std::move(parameter_js));
    }
    dict.Set("parameters", std::move(parameters_js));
  }
  dict.Set("scripts", script_tracker()->GetDebugContext());

  return base::Value(std::move(dict));
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

  RequireUI();
  for (ControllerObserver& observer : observers_) {
    observer.OnError(error_message, reason);
  }
  EnterStoppedState();

  RecordDropOutOrShutdown(reason);
}

void Controller::OnFatalError(const std::string& error_message,
                              Metrics::DropOutReason reason) {
  LOG(ERROR) << "Autofill Assistant has encountered a fatal error and is "
                "shutting down, reason="
             << reason;
  if (state_ == AutofillAssistantState::STOPPED)
    return;

  for (ControllerObserver& observer : observers_) {
    observer.OnError(error_message, reason);
  }
  EnterStoppedState();

  RecordDropOutOrShutdown(reason);
}

void Controller::RecordDropOutOrShutdown(Metrics::DropOutReason reason) {
  // If there is an UI, we wait for it to be closed before shutting down (the UI
  // will call |ShutdownIfNecessary|).
  if (client_->HasHadUI()) {
    // We report right away to make sure we don't lose this reason if the client
    // is unexpectedly destroyed while the error message is showing (for example
    // if the tab is closed).
    client_->RecordDropOut(reason);
  } else {
    Shutdown(reason);
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
      OnScriptError(GetDisplayStringUTF8(ClientSettingsProto::DEFAULT_ERROR,
                                         GetSettings()),
                    Metrics::DropOutReason::NO_INITIAL_SCRIPTS);
      break;

    case AutofillAssistantState::PROMPT:
      // The user has navigated to a page that has no scripts or the scripts
      // have reached a state from which they cannot recover through a DOM
      // change.
      OnScriptError(
          GetDisplayStringUTF8(ClientSettingsProto::GIVE_UP, GetSettings()),
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
  // Script selection is disabled when a script is already running. We will
  // check again and maybe update when the current script has finished.
  if (script_tracker()->running())
    return;

  if (state_ == AutofillAssistantState::STARTING) {
    MaybeAutostartScript(runnable_scripts);
  }
  // In other states we ignore the script update.
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

void Controller::OnNavigationShutdownOrError(const GURL& url,
                                             Metrics::DropOutReason reason) {
  if (google_util::IsGoogleDomainUrl(
          url, google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS)) {
    Shutdown(reason);
  } else {
    OnScriptError(
        GetDisplayStringUTF8(ClientSettingsProto::GIVE_UP, GetSettings()),
        reason);
  }
}

void Controller::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
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

  bool is_user_initiated_or_back_forward =
      !navigation_handle->IsRendererInitiated() ||
      navigation_handle->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK;

  if (state_ == AutofillAssistantState::STOPPED &&
      is_user_initiated_or_back_forward &&
      !navigation_handle->WasServerRedirect()) {
    ShutdownIfNecessary();
    return;
  }

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
  // Everything else, such as going back to a previous page (whether
  // user-initiated or javascript-initiated), or refreshing the page is
  // considered an end condition. If going back to a previous page is required,
  // consider using the BROWSE state instead.
  if (state_ == AutofillAssistantState::PROMPT &&
      web_contents()->GetLastCommittedURL().is_valid() &&
      !navigation_handle->WasServerRedirect() &&
      is_user_initiated_or_back_forward) {
    OnNavigationShutdownOrError(navigation_handle->GetURL(),
                                Metrics::DropOutReason::NAVIGATION);
    return;
  }

  // When in RUNNING state, all renderer initiated navigation except
  // back/forward is allowed, user initiated navigation will cause an error.
  if (state_ == AutofillAssistantState::RUNNING &&
      !navigation_handle->WasServerRedirect() &&
      is_user_initiated_or_back_forward) {
    OnNavigationShutdownOrError(
        navigation_handle->GetURL(),
        Metrics::DropOutReason::NAVIGATION_WHILE_RUNNING);
    return;
  }

  // Note that BROWSE state end conditions are in DidFinishNavigation, in order
  // to be able to properly evaluate the committed url.
}

void Controller::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  // Ensure that the connection is still secure.
  switch (GetState()) {
    case AutofillAssistantState::STARTING:
    case AutofillAssistantState::RUNNING:
    case AutofillAssistantState::PROMPT:
    case AutofillAssistantState::BROWSE:
    case AutofillAssistantState::MODAL_DIALOG:
      if (ShutdownIfCertificateInsecure()) {
        return;
      }
      break;
    case AutofillAssistantState::STOPPED:
    case AutofillAssistantState::INACTIVE:
      break;
  }

  // TODO(b/159871774): Rethink how we handle navigation events. The early
  // return here may prevent us from updating |navigating_to_new_document_|.
  if (navigation_handle->IsSameDocument() || !IsNavigatingToNewDocument()) {
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
  // supported. If the new URL is on a Google property, destroy the UI
  // immediately, without showing an error.
  if (state_ == AutofillAssistantState::BROWSE) {
    if (!url_utils::IsInDomainOrSubDomain(GetCurrentURL(), script_url_) &&
        !url_utils::IsInDomainOrSubDomain(GetCurrentURL(),
                                          browse_domains_allowlist_)) {
      OnNavigationShutdownOrError(
          web_contents()->GetLastCommittedURL(),
          Metrics::DropOutReason::DOMAIN_CHANGE_DURING_BROWSE_MODE);
    }
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

void Controller::PrimaryMainDocumentElementAvailable() {
  OnUrlChange();
}

void Controller::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  Shutdown(Metrics::DropOutReason::RENDER_PROCESS_GONE);
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

void Controller::WebContentsDestroyed() {
  suppress_keyboard_raii_.reset();
  // Record failure, iff an earlier call didn't already record.
  MaybeRecordFlowFinishedMetrics(Metrics::FlowFinishedState::DESTROYED);
}

bool Controller::ShutdownIfCertificateInsecure() {
  if (!web_contents()->GetLastCommittedURL().SchemeIsCryptographic()) {
    return false;
  }

  switch (client_->GetSecurityLevel()) {
    case security_state::SecurityLevel::SECURE:
    case security_state::SecurityLevel::SECURE_WITH_POLICY_INSTALLED_CERT:
      return false;
    case security_state::SecurityLevel::NONE:
    case security_state::SecurityLevel::DANGEROUS:
    case security_state::SecurityLevel::WARNING:
    case security_state::SecurityLevel::SECURITY_LEVEL_COUNT:
      OnFatalError(GetDisplayStringUTF8(ClientSettingsProto::DEFAULT_ERROR,
                                        GetSettings()),
                   Metrics::DropOutReason::CERTIFICATE_ERROR);
      return true;
  }
}

void Controller::SuppressKeyboard(bool suppress) {
  if (suppress) {
    if (!suppress_keyboard_raii_) {
      suppress_keyboard_raii_ =
          std::make_unique<SuppressKeyboardRAII>(web_contents());
    }
  } else {
    suppress_keyboard_raii_.reset();
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

void Controller::WriteUserData(
    base::OnceCallback<void(UserData*, UserDataFieldChange*)> write_callback) {
  UserDataFieldChange field_change = UserDataFieldChange::NONE;
  std::move(write_callback).Run(&user_data_, &field_change);
  if (field_change == UserDataFieldChange::NONE) {
    return;
  }
  NotifyUserDataChange(field_change);
}

void Controller::NotifyUserDataChange(UserDataFieldChange field_change) {
  for (ControllerObserver& observer : observers_) {
    observer.OnUserDataChanged(user_data_, field_change);
  }
}

bool Controller::StateNeedsUI(AutofillAssistantState state) {
  // Note that the UI might be shown in RUNNING state, even if it doesn't
  // require it.
  switch (state) {
    case AutofillAssistantState::PROMPT:
    case AutofillAssistantState::MODAL_DIALOG:
    case AutofillAssistantState::STARTING:
      return true;

    case AutofillAssistantState::INACTIVE:
    case AutofillAssistantState::STOPPED:
    case AutofillAssistantState::RUNNING:
      return false;

    case AutofillAssistantState::BROWSE:
      return browse_mode_invisible_;
  }
}

ElementArea* Controller::touchable_element_area() {
  if (!touchable_element_area_) {
    touchable_element_area_ =
        std::make_unique<ElementArea>(&settings_, GetWebController());
    touchable_element_area_->SetOnUpdate(base::BindRepeating(
        &Controller::OnTouchableAreaChanged, weak_ptr_factory_.GetWeakPtr()));
  }
  return touchable_element_area_.get();
}

ScriptTracker* Controller::script_tracker() {
  if (!script_tracker_) {
    DCHECK(client_->GetScriptExecutorUiDelegate());
    script_tracker_ = std::make_unique<ScriptTracker>(
        /* delegate= */ this,
        /* service= */ service_.get(),
        /* ui_delegate= */ client_->GetScriptExecutorUiDelegate(),
        /* listener= */ this);
  }
  return script_tracker_.get();
}

void Controller::OnActionsResponseReceived(
    const RoundtripNetworkStats& network_stats) {
  accumulated_network_stats_.set_num_roundtrips(
      accumulated_network_stats_.num_roundtrips() +
      network_stats.num_roundtrips());
  accumulated_network_stats_.set_roundtrip_encoded_body_size_bytes(
      accumulated_network_stats_.roundtrip_encoded_body_size_bytes() +
      network_stats.roundtrip_encoded_body_size_bytes());
  accumulated_network_stats_.set_roundtrip_decoded_body_size_bytes(
      accumulated_network_stats_.roundtrip_decoded_body_size_bytes() +
      network_stats.roundtrip_decoded_body_size_bytes());
  for (const auto& action_network_stats : network_stats.action_stats()) {
    *accumulated_network_stats_.add_action_stats() = action_network_stats;
  }
}

void Controller::MaybeRecordFlowFinishedMetrics(
    Metrics::FlowFinishedState state) {
  if (accumulated_network_stats_.num_roundtrips() == 0 || !web_contents()) {
    return;
  }

  Metrics::RecordFlowFinished(
      ukm_recorder_,
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId(), state,
      accumulated_network_stats_);

  // Reset network stats. Subsequent calls to this method should be ignored,
  // unless a new run was started in the meantime.
  accumulated_network_stats_ = RoundtripNetworkStats();
}

}  // namespace autofill_assistant
