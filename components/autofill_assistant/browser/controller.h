// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/element_area.h"
#include "components/autofill_assistant/browser/execution_delegate.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/public/runtime_manager.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/script_tracker.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/suppress_keyboard_raii.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/ui_controller.h"
#include "components/autofill_assistant/browser/ui_delegate.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "components/autofill_assistant/content/browser/annotate_dom_model_service.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace base {
class TickClock;
}  // namespace base

namespace password_manager {
class PasswordChangeSuccessTracker;
}  // namespace password_manager

namespace autofill_assistant {
class ControllerTest;

// Autofill assistant controller controls autofill assistant action detection,
// display, execution and so on. The instance of this object self deletes when
// the web contents is being destroyed.
class Controller : public ScriptExecutorDelegate,
                   public ScriptTracker::Listener,
                   private content::WebContentsObserver,
                   public ExecutionDelegate {
 public:
  // |web_contents|, |client|, |tick_clock| and |script_executor_ui_delegate|
  // must remain valid for the lifetime of the instance. Controller will take
  // ownership of |service| if specified, otherwise will create and own the
  // default service.
  Controller(content::WebContents* web_contents,
             Client* client,
             const base::TickClock* tick_clock,
             base::WeakPtr<RuntimeManager> runtime_manager,
             std::unique_ptr<Service> service,
             ukm::UkmRecorder* ukm_recorder,
             AnnotateDomModelService* annotate_dom_model_service);

  Controller(const Controller&) = delete;
  Controller& operator=(const Controller&) = delete;

  ~Controller() override;

  // Let the controller know it should keep tracking script availability for the
  // current domain, to be ready to report any scripts as action.
  //
  // Activates the controller, if needed and runs it in the background, without
  // showing any UI until a script is started or Start() is called.
  //
  // Only the context of the first call to Track() is taken into account.
  //
  // If non-null |on_first_check_done| is called once the result of the first
  // check of script availability are in - whether they're positive or negative.
  void Track(std::unique_ptr<TriggerContext> trigger_context,
             base::OnceCallback<void()> on_first_check_done);

  // Returns true if we are in tracking mode and the first round of script
  // checks has been completed.
  bool HasRunFirstCheck() const;

  // Called when autofill assistant should start.
  //
  // This shows a UI, containing a progress bar, and executes the first
  // available autostartable script. Starts checking for scripts, if necessary.
  //
  // Start() does nothing if called more than once or if a script is already
  // running.
  //
  // Start() will overwrite any context previously set by Track().
  bool Start(const GURL& deeplink_url,
             std::unique_ptr<TriggerContext> trigger_context);

  const std::vector<ScriptHandle>& GetDirectActionScripts() const;
  bool PerformDirectAction(int index, std::unique_ptr<TriggerContext> context);
  base::Value GetDebugContext();

  // Overrides ScriptExecutorDelegate:
  const ClientSettings& GetSettings() override;
  const GURL& GetCurrentURL() override;
  const GURL& GetDeeplinkURL() override;
  const GURL& GetScriptURL() override;
  Service* GetService() override;
  WebController* GetWebController() override;
  const TriggerContext* GetTriggerContext() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  WebsiteLoginManager* GetWebsiteLoginManager() override;
  password_manager::PasswordChangeSuccessTracker*
  GetPasswordChangeSuccessTracker() override;
  content::WebContents* GetWebContents() override;
  std::string GetEmailAddressForAccessTokenAccount() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  void SetTouchableElementArea(const ElementAreaProto& area) override;
  void SetViewportMode(ViewportMode mode) override;
  void SetClientSettings(const ClientSettingsProto& client_settings) override;
  void ExpectNavigation() override;
  bool IsNavigatingToNewDocument() override;
  bool HasNavigationError() override;
  void SetBrowseModeInvisible(bool invisible) override;
  bool ShouldShowWarning() override;
  ProcessedActionStatusDetailsProto& GetLogInfo() override;

  // Show the UI if it's not already shown. This is only meaningful while in
  // states where showing the UI is optional, such as RUNNING, in tracking mode.
  void RequireUI() override;

  void AddNavigationListener(
      ScriptExecutorDelegate::NavigationListener* listener) override;
  void RemoveNavigationListener(
      ScriptExecutorDelegate::NavigationListener* listener) override;

  void SetBrowseDomainsAllowlist(std::vector<std::string> domains) override;

  bool EnterState(AutofillAssistantState state) override;
  AutofillAssistantState GetState() const override;
  void SetOverlayBehavior(
      ConfigureUiStateProto::OverlayBehavior overlay_behavior) override;
  void WriteUserData(
      base::OnceCallback<void(UserData*, UserDataFieldChange*)>) override;
  void OnScriptError(const std::string& error_message,
                     Metrics::DropOutReason reason);
  void OnNavigationShutdownOrError(const GURL& url,
                                   Metrics::DropOutReason reason);

  // Overrides ExecutionDelegate:
  bool NeedsUI() const override;
  void GetVisualViewport(RectF* visual_viewport) const override;
  ViewportMode GetViewportMode() override;
  bool IsTabSelected() override;
  void SetTabSelected(bool selected) override;
  void GetOverlayColors(OverlayColors* colors) const override;
  bool ShouldSuppressKeyboard() const override;
  void SuppressKeyboard(bool suppress) override;
  UserData* GetUserData() override;
  UserModel* GetUserModel() override;
  bool ShouldShowOverlay() const override;
  const ClientSettings& GetClientSettings() const override;
  void ShutdownIfNecessary() override;
  void NotifyUserDataChange(UserDataFieldChange field_change) override;
  void GetTouchableArea(std::vector<RectF>* area) const override;
  void GetRestrictedArea(std::vector<RectF>* area) const override;
  void OnFatalError(const std::string& error_message,
                    Metrics::DropOutReason reason) override;
  void SetUiShown(bool shown) override;

  void AddObserver(ControllerObserver* observer) override;
  void RemoveObserver(const ControllerObserver* observer) override;

 private:
  friend ControllerTest;

  void SetWebControllerForTest(std::unique_ptr<WebController> web_controller);

  // Called when the committed URL has or might have changed.
  void OnUrlChange();

  // Returns true if the controller should keep checking for scripts according
  // to the current state.
  bool ShouldCheckScripts();

  // If the controller's state requires scripts to be checked, check them
  // once right now and schedule regular checks. Otherwise, do nothing.
  void GetOrCheckScripts();

  void OnGetScripts(const GURL& url,
                    int http_status,
                    const std::string& response,
                    const ServiceRequestSender::ResponseInfo& response_info);

  // Execute |script_path| and, if execution succeeds, enter |end_state| and
  // call |on_success|.
  void ExecuteScript(const std::string& script_path,
                     const std::string& start_message,
                     bool needs_ui,
                     std::unique_ptr<TriggerContext> context,
                     AutofillAssistantState end_state);
  void OnScriptExecuted(const std::string& script_path,
                        AutofillAssistantState end_state,
                        const ScriptExecutor::Result& result);

  // Check script preconditions every few seconds for a certain number of times.
  // If checks are already running, StartPeriodicScriptChecks resets the count.
  //
  // TODO(crbug.com/806868): Find a better solution. This is a brute-force
  // solution that reacts slowly to changes.
  void StartPeriodicScriptChecks();
  void StopPeriodicScriptChecks();
  void OnPeriodicScriptCheck();

  void MaybeReportFirstCheckDone();

  // Runs autostart scripts from |runnable_scripts|, if the conditions are
  // right. Nothing happens if an empty vector is passed.
  // If none of the scripts is autostartable or too many are, it stops the
  // execution with an error.
  void MaybeAutostartScript(const std::vector<ScriptHandle>& runnable_scripts);

  void DisableAutostart();

  void InitFromParameters();

  // Called when a script is selected.
  void OnScriptSelected(const ScriptHandle& handle,
                        std::unique_ptr<TriggerContext> context);

  // Overrides ScriptTracker::Listener:
  void OnNoRunnableScriptsForPage() override;
  void OnRunnableScriptsChanged(
      const std::vector<ScriptHandle>& runnable_scripts) override;

  // Overrides content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryMainDocumentElementAvailable() override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;
  void WebContentsDestroyed() override;

  void OnTouchableAreaChanged(const RectF& visual_viewport,
                              const std::vector<RectF>& touchable_areas,
                              const std::vector<RectF>& restricted_areas);

  void SetOverlayColors(std::unique_ptr<OverlayColors> colors);
  void ReportNavigationStateChanged();
  void SetProfile(const std::string& key,
                  UserDataFieldChange field_change,
                  std::unique_ptr<autofill::AutofillProfile> profile);

  // Show the first "Opening..." message and enter START state.
  void ShowFirstMessageAndStart();

  // Clear out visible state and enter the stopped state.
  void EnterStoppedState();

  // Configure the UI for the stopped state, clearing out visible state except
  // for the message and possibly the "Send feedback" chip.
  void SetStoppedUI();

  // Notifies observers and shuts down.
  void Shutdown(Metrics::DropOutReason reason);

  ElementArea* touchable_element_area();
  ScriptTracker* script_tracker();
  bool allow_autostart() { return state_ == AutofillAssistantState::STARTING; }

  void RecordDropOutOrShutdown(Metrics::DropOutReason reason);
  void PerformDelayedShutdownIfNecessary();

  bool StateNeedsUI(AutofillAssistantState state);

  // Resets the controller to the initial state.
  void ResetState();
  void SetDirectActionScripts(
      const std::vector<ScriptHandle>& direct_action_scripts);

  ClientSettings settings_;
  const raw_ptr<Client> client_;
  const raw_ptr<const base::TickClock> tick_clock_;
  base::WeakPtr<RuntimeManager> runtime_manager_;

  // Lazily instantiate in GetWebController().
  std::unique_ptr<WebController> web_controller_;

  // An instance to suppress keyboard. If this is not nullptr, the keyboard
  // is suppressed.
  std::unique_ptr<SuppressKeyboardRAII> suppress_keyboard_raii_;

  // Lazily instantiate in GetService().
  std::unique_ptr<Service> service_;
  std::unique_ptr<TriggerContext> trigger_context_;

  AutofillAssistantState state_ = AutofillAssistantState::INACTIVE;

  // The URL passed to Start(). Used only as long as there's no committed URL.
  // Note that this is the deeplink passed by a caller.
  GURL deeplink_url_;

  // The last URL the controller requested scripts from. Note that this is
  // reported to the backend in an initial get action request.
  GURL script_url_;

  // Whether a task for periodic checks is scheduled.
  bool periodic_script_check_scheduled_ = false;

  // Number of remaining periodic checks.
  int periodic_script_check_count_ = 0;

  // Run this script if no scripts become autostartable after
  // absolute_autostart_timeout.
  //
  // Ignored unless |allow_autostart_| is true.
  std::string autostart_timeout_script_path_;

  // How long to wait for an autostartable script before failing.
  base::TimeDelta autostart_timeout_;

  // Ticks at which we'll have reached |autostart_timeout_|.
  base::TimeTicks absolute_autostart_timeout_;

  // Area of the screen that corresponds to the current set of touchable
  // elements.
  // Lazily instantiate in touchable_element_area().
  std::unique_ptr<ElementArea> touchable_element_area_;

  // Current set of direct actions.
  std::vector<ScriptHandle> direct_action_scripts_;

  // Current viewport mode.
  ViewportMode viewport_mode_ = ViewportMode::NO_RESIZE;

  // Whether the tab associated with this controller is currently selected.
  bool tab_selected_ = true;

  std::unique_ptr<OverlayColors> overlay_colors_;

  UserData user_data_;

  // Value for ScriptExecutorDelegate::IsNavigatingToNewDocument()
  bool navigating_to_new_document_ = false;

  // If this is set, the controller was still navigating during startup. Wait
  // for DidFinishNavigation and execute the callback.
  base::OnceClosure start_after_navigation_;

  // Value for ScriptExecutorDelegate::HasNavigationError()
  bool navigation_error_ = false;
  base::ObserverList<ScriptExecutorDelegate::NavigationListener>
      navigation_listeners_;

  // The next DidStartNavigation will not cause an error.
  bool expect_navigation_ = false;

  // Tracks scripts and script execution. It's kept at the end, as it tend to
  // depend on everything the controller support, through script and script
  // actions.
  // Lazily instantiate in script_tracker().
  std::unique_ptr<ScriptTracker> script_tracker_;

  base::ObserverList<ControllerObserver> observers_;

  // If true, the controller is supposed to stay up and running in the
  // background even without UI, keeping track of scripts.
  //
  // This has two main effects:
  // - the controllers stays alive even after a fatal error, just so it can
  // immediately report that no actions are available on that website.
  // - scripts error are not considered fatal errors. The controller reverts
  // to TRACKING mode.
  //
  // This is set by Track().
  bool tracking_ = false;

  // Whether the controller is in a state in which a UI should be shown.
  bool needs_ui_ = false;

  // Whether UI is shown.
  bool ui_shown_ = false;

  // True once the controller has run the first set of scripts and have either
  // declared it invalid - and entered stopped state - or have processed its
  // result - and updated the state and set of available actions.
  bool has_run_first_check_ = false;

  // Whether the overlay should be set according to state or always hidden.
  ConfigureUiStateProto::OverlayBehavior overlay_behavior_ =
      ConfigureUiStateProto::DEFAULT;

  // Callbacks to call when |has_run_first_check_| becomes true.
  std::vector<base::OnceCallback<void()>> on_has_run_first_check_;

  // If set, the controller entered the STOPPED state but shutdown was delayed
  // until the browser has left the |script_url_.host()| for which the decision
  // was taken.
  absl::optional<Metrics::DropOutReason> delayed_shutdown_reason_;

  UserModel user_model_;

  std::vector<std::string> browse_domains_allowlist_;
  bool browse_mode_invisible_ = false;

  // Log information about action execution. Gets reset at the start of every
  // action and attached to the action result on completion.
  ProcessedActionStatusDetailsProto log_info_;

  raw_ptr<ukm::UkmRecorder> ukm_recorder_;

  // If instantiated, will start delivering the required model for annotating
  // DOM nodes. May be nullptr.
  const raw_ptr<AnnotateDomModelService> annotate_dom_model_service_;

  base::WeakPtrFactory<Controller> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_
