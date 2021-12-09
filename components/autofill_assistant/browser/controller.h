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
#include "components/autofill_assistant/browser/autofill_assistant_tts_controller.h"
#include "components/autofill_assistant/browser/basic_interactions.h"
#include "components/autofill_assistant/browser/bottom_sheet_state.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/element_area.h"
#include "components/autofill_assistant/browser/event_handler.h"
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

namespace autofill_assistant {
class ControllerTest;

// Autofill assistant controller controls autofill assistant action detection,
// display, execution and so on. The instance of this object self deletes when
// the web contents is being destroyed.
class Controller : public ScriptExecutorDelegate,
                   public virtual UiDelegate,
                   public ScriptTracker::Listener,
                   private content::WebContentsObserver,
                   public UserModel::Observer,
                   public AutofillAssistantTtsController::TtsEventDelegate {
 public:
  // |web_contents|, |client|, |tick_clock| and |runtime_manager| must remain
  // valid for the lifetime of the instance. Controller will take ownership of
  // |service| if specified, otherwise will create and own the default service.
  Controller(content::WebContents* web_contents,
             Client* client,
             const base::TickClock* tick_clock,
             base::WeakPtr<RuntimeManager> runtime_manager,
             std::unique_ptr<Service> service,
             std::unique_ptr<AutofillAssistantTtsController> tts_controller,
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

  // Returns true if the controller is in a state where UI is necessary.
  bool NeedsUI() const { return needs_ui_; }

  // Called when an accessibility service with "FEEDBACK_SPOKEN" feedback type
  // is enabled or disabled.
  void OnSpokenFeedbackAccessibilityServiceChanged(bool enabled);

  const std::vector<ScriptHandle>& GetDirectActionScripts() const;
  bool PerformDirectAction(int index, std::unique_ptr<TriggerContext> context);

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
  content::WebContents* GetWebContents() override;
  std::string GetEmailAddressForAccessTokenAccount() override;
  ukm::UkmRecorder* GetUkmRecorder() override;

  void SetTouchableElementArea(const ElementAreaProto& area) override;
  void SetStatusMessage(const std::string& message) override;
  std::string GetStatusMessage() const override;
  void SetBubbleMessage(const std::string& message) override;
  std::string GetBubbleMessage() const override;
  void SetTtsMessage(const std::string& message) override;
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
  void SetViewportMode(ViewportMode mode) override;
  void SetPeekMode(ConfigureBottomSheetProto::PeekMode peek_mode) override;
  void ExpandBottomSheet() override;
  void CollapseBottomSheet() override;
  void SetClientSettings(const ClientSettingsProto& client_settings) override;
  bool SetForm(
      std::unique_ptr<FormProto> form,
      base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
      base::OnceCallback<void(const ClientStatus&)> cancel_callback) override;
  void ExpectNavigation() override;
  bool IsNavigatingToNewDocument() override;
  bool HasNavigationError() override;
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
  void SetBrowseModeInvisible(bool invisible) override;
  bool ShouldShowWarning() override;
  void SetShowFeedbackChip(bool show_feedback_chip) override;
  ProcessedActionStatusDetailsProto& GetLogInfo() override;

  // Show the UI if it's not already shown. This is only meaningful while in
  // states where showing the UI is optional, such as RUNNING, in tracking mode.
  void RequireUI() override;
  void SetUiShown(bool shown) override;

  void AddNavigationListener(
      ScriptExecutorDelegate::NavigationListener* listener) override;
  void RemoveNavigationListener(
      ScriptExecutorDelegate::NavigationListener* listener) override;
  void AddListener(ScriptExecutorDelegate::Listener* listener) override;
  void RemoveListener(ScriptExecutorDelegate::Listener* listener) override;

  void SetExpandSheetForPromptAction(bool expand) override;
  void SetBrowseDomainsAllowlist(std::vector<std::string> domains) override;

  bool EnterState(AutofillAssistantState state) override;
  AutofillAssistantState GetState() override;
  void SetOverlayBehavior(
      ConfigureUiStateProto::OverlayBehavior overlay_behavior) override;
  void SetCollectUserDataOptions(CollectUserDataOptions* options) override;
  void SetLastSuccessfulUserDataOptions(std::unique_ptr<CollectUserDataOptions>
                                            collect_user_data_options) override;
  const CollectUserDataOptions* GetLastSuccessfulUserDataOptions()
      const override;
  void WriteUserData(
      base::OnceCallback<void(UserData*, UserData::FieldChange*)>) override;
  void OnScriptError(const std::string& error_message,
                     Metrics::DropOutReason reason);
  void OnNavigationShutdownOrError(const GURL& url,
                                   Metrics::DropOutReason reason);

  // Overrides autofill_assistant::UiDelegate:
  AutofillAssistantState GetState() const override;
  std::vector<Details> GetDetails() const override;
  const InfoBox* GetInfoBox() const override;
  int GetProgressActiveStep() const override;
  bool GetProgressVisible() const override;
  bool GetTtsButtonVisible() const override;
  TtsButtonState GetTtsButtonState() const override;
  bool GetProgressBarErrorState() const override;
  ShowProgressBarProto::StepProgressBarConfiguration
  GetStepProgressBarConfiguration() const override;
  const std::vector<UserAction>& GetUserActions() const override;
  bool PerformUserAction(int index) override;
  std::string GetDebugContext() override;
  const CollectUserDataOptions* GetCollectUserDataOptions() const override;
  const UserData* GetUserData() const override;
  void SetShippingAddress(std::unique_ptr<autofill::AutofillProfile> address,
                          UserDataEventType event_type) override;
  void SetContactInfo(std::unique_ptr<autofill::AutofillProfile> profile,
                      UserDataEventType event_type) override;
  void SetCreditCard(std::unique_ptr<autofill::CreditCard> card,
                     std::unique_ptr<autofill::AutofillProfile> billing_profile,
                     UserDataEventType event_type) override;
  void ReloadUserData(UserDataEventField event_field,
                      UserDataEventType event_type) override;
  void SetTermsAndConditions(
      TermsAndConditionsState terms_and_conditions) override;
  void SetLoginOption(const std::string& identifier) override;
  void OnTextLinkClicked(int link) override;
  void OnFormActionLinkClicked(int link) override;
  void OnTtsButtonClicked() override;
  void SetDateTimeRangeStartDate(
      const absl::optional<DateProto>& date) override;
  void SetDateTimeRangeStartTimeSlot(
      const absl::optional<int>& timeslot_index) override;
  void SetDateTimeRangeEndDate(const absl::optional<DateProto>& date) override;
  void SetDateTimeRangeEndTimeSlot(
      const absl::optional<int>& timeslot_index) override;
  void SetAdditionalValue(const std::string& client_memory_key,
                          const ValueProto& value) override;
  void GetTouchableArea(std::vector<RectF>* area) const override;
  void GetRestrictedArea(std::vector<RectF>* area) const override;
  void GetVisualViewport(RectF* visual_viewport) const override;
  void OnFatalError(const std::string& error_message,
                    bool show_feedback_chip,
                    Metrics::DropOutReason reason) override;
  void OnStop(const std::string& message,
              const std::string& button_label) override;
  void MaybeReportFirstCheckDone();
  ViewportMode GetViewportMode() override;
  ConfigureBottomSheetProto::PeekMode GetPeekMode() override;
  BottomSheetState GetBottomSheetState() override;
  void SetBottomSheetState(BottomSheetState state) override;
  bool IsTabSelected() override;
  void SetTabSelected(bool selected) override;
  void GetOverlayColors(OverlayColors* colors) const override;
  const ClientSettings& GetClientSettings() const override;
  const FormProto* GetForm() const override;
  const FormProto::Result* GetFormResult() const override;
  void SetCounterValue(int input_index, int counter_index, int value) override;
  void SetChoiceSelected(int input_index,
                         int choice_index,
                         bool selected) override;
  void AddObserver(ControllerObserver* observer) override;
  void RemoveObserver(const ControllerObserver* observer) override;
  void DispatchEvent(const EventHandler::EventKey& key) override;
  UserModel* GetUserModel() override;
  EventHandler* GetEventHandler() override;
  bool ShouldPromptActionExpandSheet() const override;
  BasicInteractions* GetBasicInteractions() override;
  const GenericUserInterfaceProto* GetGenericUiProto() const override;
  const GenericUserInterfaceProto* GetPersistentGenericUiProto() const override;
  bool ShouldShowOverlay() const override;
  bool ShouldSuppressKeyboard() const override;
  void SuppressKeyboard(bool suppress) override;
  void ShutdownIfNecessary() override;
  void OnKeyboardVisibilityChanged(bool visible) override;
  void OnInputTextFocusChanged(bool is_text_focused) override;

  // Overrides AutofillAssistantTtsController::TtsEventDelegate
  void OnTtsEvent(AutofillAssistantTtsController::TtsEventType event) override;

 private:
  friend ControllerTest;

  // A holder class which contains some details and, optionally, a timer that
  // will "enable" them later on.
  class DetailsHolder {
   public:
    DetailsHolder(std::unique_ptr<Details> details,
                  std::unique_ptr<base::OneShotTimer> timer);
    ~DetailsHolder();
    DetailsHolder(DetailsHolder&& other);
    DetailsHolder& operator=(DetailsHolder&& other);

    // The details held by this object.
    const Details& GetDetails() const;

    // Whether the details held by this object are visible. Will return false if
    // a timer was set and was not reached yet.
    bool CurrentlyVisible() const;

    // Enable the details held by this object so that they are shown (i.e.
    // CurrentlyVisible() returns true).
    //
    // In practice, this is called at most once when |timer_| is triggered.
    void Enable();

   private:
    std::unique_ptr<Details> details_;
    std::unique_ptr<base::OneShotTimer> timer_;
  };

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
                    const std::string& response);

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

  void UpdateCollectUserDataActions();
  void OnCollectUserDataContinueButtonClicked();
  void OnCollectUserDataAdditionalActionTriggered(int index);

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
  void DocumentAvailableInMainFrame(
      content::RenderFrameHost* render_frame_host) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;
  void WebContentsDestroyed() override;

  // Overrides autofill_assistant::UserModel::Observer:
  void OnValueChanged(const std::string& identifier,
                      const ValueProto& new_value) override;

  void OnTouchableAreaChanged(const RectF& visual_viewport,
                              const std::vector<RectF>& touchable_areas,
                              const std::vector<RectF>& restricted_areas);

  void SetOverlayColors(std::unique_ptr<OverlayColors> colors);
  void ReportNavigationStateChanged();
  void SetProfile(const std::string& key,
                  UserData::FieldChange field_change,
                  std::unique_ptr<autofill::AutofillProfile> profile);

  // Show the first "Opening..." message and enter START state.
  void ShowFirstMessageAndStart();

  // Clear out visible state and enter the stopped state.
  // If |show_feedback_chip| is true, a "Send feedback" chip will be added to
  // the bottom sheet.
  void EnterStoppedState(bool show_feedback_chip);

  void OnFeedbackChipClicked();

  ElementArea* touchable_element_area();
  ScriptTracker* script_tracker();
  bool allow_autostart() { return state_ == AutofillAssistantState::STARTING; }

  void RecordDropOutOrShutdown(Metrics::DropOutReason reason);
  void PerformDelayedShutdownIfNecessary();

  bool StateNeedsUI(AutofillAssistantState state);

  bool ShouldChipsBeVisible();
  bool ShouldUpdateChipVisibility();
  void SetVisibilityAndUpdateUserActions();

  void MakeDetailsVisible(size_t details_index);
  void NotifyDetailsChanged();

  // This represents the display strings locale to be used for the currently
  // executing set of actions. This locale is used in two ways currently:
  // 1. Locale of backend provided display strings, if available.
  // 2. TTS Controller uses this locale for playing TTS messages.
  std::string GetDisplayStringsLocale();
  void SetTtsButtonState(TtsButtonState state);

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
  bool can_recover_from_stopped_ = false;

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

  // Current status message, may be empty.
  std::string status_message_;

  // Current TTS message to be played, may be empty.
  std::string tts_message_;

  // Current bubble / tooltip message, may be empty.
  std::string bubble_message_;

  // Current details, may be empty.
  std::vector<DetailsHolder> details_;

  // Current info box, may be null.
  std::unique_ptr<InfoBox> info_box_;

  // Current state of the progress bar.
  bool progress_visible_ = true;
  bool progress_bar_error_state_ = false;
  ShowProgressBarProto::StepProgressBarConfiguration
      step_progress_bar_configuration_;
  int progress_active_step_ = 0;

  // Current set of user actions. May be null, but never empty.
  std::unique_ptr<std::vector<UserAction>> user_actions_;

  // Current set of direct actions.
  std::vector<ScriptHandle> direct_action_scripts_;

  // Current viewport mode.
  ViewportMode viewport_mode_ = ViewportMode::NO_RESIZE;

  // Current peek mode.
  ConfigureBottomSheetProto::PeekMode peek_mode_ =
      ConfigureBottomSheetProto::HANDLE;
  bool auto_change_peek_mode_ = false;

  // The latest bottom sheet state stored.
  BottomSheetState bottom_sheet_state_ = BottomSheetState::UNDEFINED;

  // Whether the tab associated with this controller is currently selected.
  bool tab_selected_ = true;

  std::unique_ptr<OverlayColors> overlay_colors_;

  // A copy of the most recently set user data options. Can be used to determine
  // which information was requested.
  std::unique_ptr<CollectUserDataOptions> last_collect_user_data_options_;
  raw_ptr<CollectUserDataOptions> collect_user_data_options_ = nullptr;
  UserData user_data_;

  std::unique_ptr<FormProto> form_;
  std::unique_ptr<FormProto::Result> form_result_;
  base::RepeatingCallback<void(const FormProto::Result*)>
      form_changed_callback_ = base::DoNothing();
  base::OnceCallback<void(const ClientStatus&)> form_cancel_callback_ =
      base::DoNothing();

  // Value for ScriptExecutorDelegate::IsNavigatingToNewDocument()
  bool navigating_to_new_document_ = false;

  // If this is set, the controller was still navigating during startup. Wait
  // for DidFinishNavigation and execute the callback.
  base::OnceClosure start_after_navigation_;

  // Value for ScriptExecutorDelegate::HasNavigationError()
  bool navigation_error_ = false;
  base::ObserverList<ScriptExecutorDelegate::NavigationListener>
      navigation_listeners_;

  base::ObserverList<ScriptExecutorDelegate::Listener> listeners_;

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

  EventHandler event_handler_;
  UserModel user_model_;
  BasicInteractions basic_interactions_{this, &settings_};

  bool expand_sheet_for_prompt_action_ = true;
  std::vector<std::string> browse_domains_allowlist_;
  bool browse_mode_invisible_ = false;
  bool is_keyboard_showing_ = false;
  bool is_focus_on_bottom_sheet_text_input_ = false;
  bool show_feedback_chip_on_graceful_shutdown_ = false;
  bool are_chips_visible_ = true;

  bool tts_enabled_ = false;
  std::unique_ptr<AutofillAssistantTtsController> tts_controller_;
  TtsButtonState tts_button_state_ = TtsButtonState::DEFAULT;

  // Only set during a ShowGenericUiAction.
  std::unique_ptr<GenericUserInterfaceProto> generic_user_interface_;

  std::unique_ptr<GenericUserInterfaceProto> persistent_generic_user_interface_;

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
