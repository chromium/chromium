// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_H_

#include <deque>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/full_card_requester.h"
#include "components/autofill_assistant/browser/info_box.h"
#include "components/autofill_assistant/browser/retry_timer.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/top_padding.h"
#include "components/autofill_assistant/browser/wait_for_dom_observer.h"
#include "components/autofill_assistant/browser/web/element_finder.h"

namespace autofill_assistant {
class ElementStore;
class UserModel;
class WaitForDocumentOperation;
class WebController;

// Class to execute an assistant script.
class ScriptExecutor : public ActionDelegate,
                       public ScriptExecutorDelegate::NavigationListener,
                       public ScriptExecutorDelegate::Listener {
 public:
  // Listens to events on ScriptExecutor.
  // TODO(b/806868): Make global_payload a part of callback instead of the
  // listener.
  class Listener {
   public:
    virtual ~Listener() = default;

    // Called when new server payloads are available.
    //
    // TODO(b/806868): Stop reporting the script payload once the server has
    // transitioned to global payloads.
    virtual void OnServerPayloadChanged(const std::string& global_payload,
                                        const std::string& script_payload) = 0;

    // Called when an update list of scripts is available.
    virtual void OnScriptListChanged(
        std::vector<std::unique_ptr<Script>> scripts) = 0;
  };

  // |delegate|, |listener|, |script_state| and |ordered_interrupts| should
  // outlive this object and should not be nullptr.
  ScriptExecutor(const std::string& script_path,
                 std::unique_ptr<TriggerContext> additional_context,
                 const std::string& global_payload,
                 const std::string& script_payload,
                 ScriptExecutor::Listener* listener,
                 std::map<std::string, ScriptStatusProto>* scripts_state,
                 const std::vector<std::unique_ptr<Script>>* ordered_interrupts,
                 ScriptExecutorDelegate* delegate);
  ~ScriptExecutor() override;

  // What should happen after the script has run.
  enum AtEnd {
    // Continue normally.
    CONTINUE = 0,

    // Shut down Autofill Assistant.
    SHUTDOWN,

    // Stop Autofill Assistant but keep showing the UI.
    SHUTDOWN_GRACEFULLY,

    // Shut down Autofill Assistant and CCT.
    CLOSE_CUSTOM_TAB,
  };

  // Contains the result of the Run operation.
  struct Result {
    bool success = false;
    AtEnd at_end = AtEnd::CONTINUE;
    std::unique_ptr<ElementAreaProto> touchable_element_area;

    Result();
    ~Result();

    friend std::ostream& operator<<(std::ostream& out, const Result& result);
  };

  using RunScriptCallback = base::OnceCallback<void(const Result&)>;
  void Run(const UserData* user_data, RunScriptCallback callback);

  const UserData* GetUserData() const override;
  UserModel* GetUserModel() override;

  // Override ScriptExecutorDelegate::NavigationListener
  void OnNavigationStateChanged() override;

  // Override ScriptExecutorDelegate::Listener
  void OnPause(const std::string& message,
               const std::string& button_label) override;

  // Override ActionDelegate:
  void RunElementChecks(BatchElementChecker* checker) override;
  void ShortWaitForElement(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback)
      override;
  void ShortWaitForElementWithSlowWarning(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback)
      override;
  void WaitForDom(
      base::TimeDelta max_wait_time,
      bool allow_interrupt,
      WaitForDomObserver* observer,
      base::RepeatingCallback<
          void(BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback)
      override;
  void WaitForDomWithSlowWarning(
      base::TimeDelta max_wait_time,
      bool allow_interrupt,
      WaitForDomObserver* observer,
      base::RepeatingCallback<
          void(BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback)
      override;
  void SetStatusMessage(const std::string& message) override;
  std::string GetStatusMessage() override;
  void SetBubbleMessage(const std::string& message) override;
  std::string GetBubbleMessage() override;
  void FindElement(const Selector& selector,
                   ElementFinder::Callback callback) const override;
  void FindAllElements(const Selector& selector,
                       ElementFinder::Callback callback) const override;
  void CollectUserData(
      CollectUserDataOptions* collect_user_data_options) override;
  void SetLastSuccessfulUserDataOptions(std::unique_ptr<CollectUserDataOptions>
                                            collect_user_data_options) override;
  const CollectUserDataOptions* GetLastSuccessfulUserDataOptions()
      const override;
  void WriteUserData(
      base::OnceCallback<void(UserData*, UserData::FieldChange*)>) override;
  void GetFullCard(const autofill::CreditCard* credit_card,
                   GetFullCardCallback callback) override;
  void Prompt(std::unique_ptr<std::vector<UserAction>> user_actions,
              bool disable_force_expand_sheet,
              base::OnceCallback<void()> end_on_navigation_callback,
              bool browse_mode,
              bool browse_mode_invisible) override;
  void CleanUpAfterPrompt() override;
  void SetBrowseDomainsAllowlist(std::vector<std::string> domains) override;
  void FillAddressForm(
      const autofill::AutofillProfile* profile,
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&)> callback) override;
  void FillCardForm(
      std::unique_ptr<autofill::CreditCard> card,
      const std::u16string& cvc,
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&)> callback) override;
  void RetrieveElementFormAndFieldData(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&,
                              const autofill::FormData& form_data,
                              const autofill::FormFieldData& field_data)>
          callback) override;
  void ScrollToElementPosition(
      const Selector& selector,
      const TopPadding& top_padding,
      std::unique_ptr<ElementFinder::Result> container,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback) override;
  void SetTouchableElementArea(
      const ElementAreaProto& touchable_element_area) override;
  void ExpectNavigation() override;
  bool ExpectedNavigationHasStarted() override;
  bool WaitForNavigation(base::OnceCallback<void(bool)> callback) override;
  void WaitForDocumentReadyState(
      base::TimeDelta max_wait_time,
      DocumentReadyState min_ready_state,
      const ElementFinder::Result& optional_frame_element,
      base::OnceCallback<void(const ClientStatus&,
                              DocumentReadyState,
                              base::TimeDelta)> callback) override;
  void WaitUntilDocumentIsInReadyState(
      base::TimeDelta max_wait_time,
      DocumentReadyState min_ready_state,
      const ElementFinder::Result& optional_frame_element,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback)
      override;

  void LoadURL(const GURL& url) override;
  void Shutdown(bool show_feedback_chip) override;
  void Close() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  WebsiteLoginManager* GetWebsiteLoginManager() const override;
  content::WebContents* GetWebContents() override;
  ElementStore* GetElementStore() const override;
  WebController* GetWebController() const override;
  std::string GetEmailAddressForAccessTokenAccount() override;
  std::string GetLocale() override;
  void SetDetails(std::unique_ptr<Details> details,
                  base::TimeDelta delay) override;
  void AppendDetails(std::unique_ptr<Details> details,
                     base::TimeDelta delay) override;
  void ClearInfoBox() override;
  void SetInfoBox(const InfoBox& info_box) override;
  void SetProgress(int progress) override;
  bool SetProgressActiveStepIdentifier(
      const std::string& active_step_identifier) override;
  void SetProgressActiveStep(int active_step) override;
  void SetProgressVisible(bool visible) override;
  void SetProgressBarErrorState(bool error) override;
  void SetStepProgressBarConfiguration(
      const ShowProgressBarProto::StepProgressBarConfiguration& configuration)
      override;
  void SetViewportMode(ViewportMode mode) override;
  ViewportMode GetViewportMode() override;
  void SetPeekMode(ConfigureBottomSheetProto::PeekMode peek_mode) override;
  ConfigureBottomSheetProto::PeekMode GetPeekMode() override;
  void ExpandBottomSheet() override;
  void CollapseBottomSheet() override;
  void WaitForWindowHeightChange(
      base::OnceCallback<void(const ClientStatus&)> callback) override;
  const ClientSettings& GetSettings() const override;
  bool SetForm(
      std::unique_ptr<FormProto> form,
      base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
      base::OnceCallback<void(const ClientStatus&)> cancel_callback) override;
  void RequireUI() override;
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
  void SetOverlayBehavior(
      ConfigureUiStateProto::OverlayBehavior overlay_behavior) override;
  void MaybeShowSlowWebsiteWarning(
      base::OnceCallback<void(bool)> callback) override;
  void MaybeShowSlowConnectionWarning() override;
  void DispatchJsEvent(
      base::OnceCallback<void(const ClientStatus&)> callback) const override;
  base::WeakPtr<ActionDelegate> GetWeakPtr() const override;

 private:
  // Helper for WaitForElementVisible that keeps track of the state required to
  // run interrupts while waiting for a specific element.
  class WaitForDomOperation : public ScriptExecutor::Listener,
                              ScriptExecutorDelegate::NavigationListener {
   public:
    // Let the caller know about either the result of looking for the element or
    // of an abnormal result from an interrupt.
    //
    // If the given result is non-null, it should be forwarded as the result of
    // the main script.
    using Callback = base::OnceCallback<void(const ClientStatus&,
                                             const ScriptExecutor::Result*,
                                             base::TimeDelta)>;

    using WarningCallback =
        base::OnceCallback<void(base::OnceCallback<void(bool)>)>;

    // |main_script_| must not be null and outlive this instance.
    WaitForDomOperation(
        ScriptExecutor* main_script,
        ScriptExecutorDelegate* delegate,
        base::TimeDelta max_wait_time,
        bool allow_interrupt,
        WaitForDomObserver* observer,
        base::RepeatingCallback<
            void(BatchElementChecker*,
                 base::OnceCallback<void(const ClientStatus&)>)> check_elements,
        WaitForDomOperation::Callback callback);
    ~WaitForDomOperation() override;

    void Run();
    void Terminate();
    void SetTimeoutWarningCallback(WarningCallback timeout_warning);

   private:
    void Start();
    void Pause();
    void Continue();

    // Implements ScriptExecutorDelegate::NavigationListener
    void OnNavigationStateChanged() override;

    // Implements ScriptExecutor::Listener
    void OnServerPayloadChanged(const std::string& global_payload,
                                const std::string& script_payload) override;
    void OnScriptListChanged(
        std::vector<std::unique_ptr<Script>> scripts) override;

    void RunChecks(
        base::OnceCallback<void(const ClientStatus&)> report_attempt_result);
    void OnPreconditionCheckDone(const std::string& interrupt_path,
                                 bool precondition_match);
    void OnElementCheckDone(const ClientStatus&);
    void OnAllChecksDone(
        base::OnceCallback<void(const ClientStatus&)> report_attempt_result);
    void RunInterrupt(const std::string& path);
    void OnInterruptDone(const ScriptExecutor::Result& result);
    void RunCallback(const ClientStatus& element_status);
    void RunCallbackWithResult(const ClientStatus& element_status,
                               const ScriptExecutor::Result* result);
    void SetSlowWarningStatus(bool was_shown);

    // Saves the current state and sets save_pre_interrupt_state_.
    void SavePreInterruptState();

    // Restores the UI states as found by SavePreInterruptState.
    void RestoreStatusMessage();

    // if save_pre_interrupt_state_ is set, attempt to scroll the page back to
    // the original area.
    void RestorePreInterruptScroll();

    void TimeoutWarning();

    ScriptExecutor* main_script_;
    ScriptExecutorDelegate* delegate_;
    const base::TimeDelta max_wait_time_;
    const bool allow_interrupt_;
    WaitForDomObserver* observer_;
    base::RepeatingCallback<void(BatchElementChecker*,
                                 base::OnceCallback<void(const ClientStatus&)>)>
        check_elements_;
    WaitForDomOperation::Callback callback_;
    base::OnceCallback<void(base::OnceCallback<void(bool)>)> warning_callback_;
    std::unique_ptr<base::OneShotTimer> warning_timer_;
    base::TimeDelta timeout_warning_delay_;

    SlowWarningStatus warning_status_ = NO_WARNING;

    std::unique_ptr<BatchElementChecker> batch_element_checker_;

    // Path of interrupts from |ordered_interrupts_| that have been found
    // runnable.
    std::set<std::string> runnable_interrupts_;
    ClientStatus element_check_result_;

    // An empty vector of interrupts that can be passed to interrupt_executor_
    // and outlives it. Interrupts must not run interrupts.
    const std::vector<std::unique_ptr<Script>> no_interrupts_;
    Stopwatch wait_time_stopwatch_;
    Stopwatch period_stopwatch_;
    base::TimeDelta wait_time_total_;

    // The interrupt that's currently running.
    std::unique_ptr<ScriptExecutor> interrupt_executor_;

    // If true, pre-interrupt state was saved already. This happens just before
    // the first interrupt.
    bool saved_pre_interrupt_state_ = false;

    // The status message that was displayed when the interrupt started.
    std::string pre_interrupt_status_;

    // Paths of the interrupts that were just run. These interrupts are
    // prevented from firing for one round.
    std::set<std::string> ran_interrupts_;

    RetryTimer retry_timer_;

    base::WeakPtrFactory<WaitForDomOperation> weak_ptr_factory_{this};

    DISALLOW_COPY_AND_ASSIGN(WaitForDomOperation);
  };

  void OnGetActions(base::TimeTicks start_time,
                    int http_status,
                    const std::string& response);
  bool ProcessNextActionResponse(const std::string& response);
  void ReportPayloadsToListener();
  void ReportScriptsUpdateToListener(
      std::vector<std::unique_ptr<Script>> scripts);
  void RunCallback(bool success);
  void RunCallbackWithResult(const Result& result);
  void ProcessNextAction();
  void ProcessAction(Action* action);
  void GetNextActions();
  void OnProcessedAction(base::TimeTicks start_time,
                         std::unique_ptr<ProcessedActionProto> action);
  void CheckElementMatches(
      const Selector& selector,
      BatchElementChecker* checker,
      base::OnceCallback<void(const ClientStatus&)> callback);
  void CheckElementMatchesCallback(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& status,
      const ElementFinder::Result& ignored_element);
  void OnShortWaitForElement(
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback,
      const ClientStatus& element_status,
      const Result* interrupt_result,
      base::TimeDelta wait_time);
  void OnWaitForElementVisibleWithInterrupts(
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback,
      const ClientStatus& element_status,
      const Result* interrupt_result,
      base::TimeDelta wait_time);
  void OnGetUserData(
      base::OnceCallback<void(UserData*, const UserModel*)> callback,
      UserData* user_data,
      const UserModel* user_model);
  void OnAdditionalActionTriggered(
      base::OnceCallback<void(int, UserData*, const UserModel*)> callback,
      int index,
      UserData* user_data,
      const UserModel* user_model);
  void OnTermsAndConditionsLinkClicked(
      base::OnceCallback<void(int, UserData*, const UserModel*)> callback,
      int link,
      UserData* user_data,
      const UserModel* user_model);
  void OnGetFullCard(std::unique_ptr<FullCardRequester> full_card_requester,
                     GetFullCardCallback callback,
                     const ClientStatus& status,
                     std::unique_ptr<autofill::CreditCard> card,
                     const std::u16string& cvc);
  void OnChosen(UserAction::Callback callback,
                std::unique_ptr<TriggerContext> context);
  void OnWaitForDocumentReadyState(
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback,
      const ClientStatus& status,
      DocumentReadyState ready_state,
      base::TimeDelta wait_time);
  void OnResume();

  // Maybe shows the message specified in a callout, depending on the current
  // state and client settings.
  bool MaybeShowSlowWarning(const std::string& message, bool enabled);

  const std::string script_path_;
  std::unique_ptr<TriggerContext> additional_context_;
  std::string last_global_payload_;
  const std::string initial_script_payload_;
  std::string last_script_payload_;
  ScriptExecutor::Listener* const listener_;
  ScriptExecutorDelegate* const delegate_;
  // Set of interrupts that might run during wait for dom or prompt action with
  // allow_interrupt. Sorted by priority; an interrupt that appears on the
  // vector first should run first. Note that the content of this vector can
  // change while the script is running, as a result of OnScriptListChanged
  // being called.
  const std::vector<std::unique_ptr<Script>>* const ordered_interrupts_;
  std::map<std::string, ScriptStatusProto>* const scripts_state_;
  RunScriptCallback callback_;
  std::vector<std::unique_ptr<Action>> actions_;
  std::vector<ProcessedActionProto> processed_actions_;
  AtEnd at_end_ = CONTINUE;
  bool should_stop_script_ = false;
  bool should_clean_contextual_ui_on_finish_ = false;
  ActionProto::ActionInfoCase previous_action_type_ =
      ActionProto::ACTION_INFO_NOT_SET;
  Selector last_focused_element_selector_;
  TopPadding last_focused_element_top_padding_;
  std::unique_ptr<ElementAreaProto> touchable_element_area_;

  // Steps towards the requirements for calling |on_expected_navigation_done_|
  // to be fulfilled.
  enum class ExpectedNavigationStep {
    // No navigation is expected.
    UNEXPECTED = 0,
    // Navigation start is expected.
    EXPECTED,
    // Navigation has started, end is expected.
    STARTED,
    // Expected navigation has ended.
    DONE
  };
  ExpectedNavigationStep expected_navigation_step_ =
      ExpectedNavigationStep::UNEXPECTED;

  // Callback called the next time |expected_navigation_step_| becomes DONE.
  base::OnceCallback<void(bool)> on_expected_navigation_done_;

  // Data only relevant to the currently running action. It is cleared before an
  // action is run.
  struct CurrentActionData {
    CurrentActionData();
    ~CurrentActionData();
    CurrentActionData& operator=(CurrentActionData&& other);

    // Navigation information relevant to the current action.
    NavigationInfoProto navigation_info;

    std::unique_ptr<WaitForDomOperation> wait_for_dom;
    std::unique_ptr<WaitForDocumentOperation> wait_for_document;

    // Set to true when a direct action was used to trigger a UserAction within
    // a prompt. This is reported to the backend.
    bool direct_action = false;

    // This callback is set when a navigation event should terminate an ongoing
    // prompt action. Only a prompt action will set a valid callback here.
    base::OnceCallback<void()> end_prompt_on_navigation_callback;
  };
  CurrentActionData current_action_data_;
  absl::optional<size_t> current_action_index_;

  const UserData* user_data_ = nullptr;

  bool is_paused_ = false;
  std::string last_status_message_;

  base::TimeTicks batch_start_time_;
  RoundtripTimingStats roundtrip_timing_stats_;

  bool connection_warning_already_shown_ = false;
  bool website_warning_already_shown_ = false;
  int consecutive_slow_roundtrip_counter_ = 0;

  base::WeakPtrFactory<ScriptExecutor> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ScriptExecutor);
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_H_
