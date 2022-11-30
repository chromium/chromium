// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_H_

#include <deque>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/full_card_requester.h"
#include "components/autofill_assistant/browser/info_box.h"
#include "components/autofill_assistant/browser/retry_timer.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/script_executor_ui_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/service_request_sender.h"
#include "components/autofill_assistant/browser/top_padding.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/wait_for_dom_observer.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace autofill_assistant {
class ElementFinderResult;
class ElementStore;
class UserModel;
class WaitForDomOperation;
class WebController;

// Class to execute an assistant script.
class ScriptExecutor : public ActionDelegate,
                       public ScriptExecutorDelegate::NavigationListener {
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
                 const std::vector<std::unique_ptr<Script>>* ordered_interrupts,
                 ScriptExecutorDelegate* delegate,
                 ScriptExecutorUiDelegate* ui_delegate,
                 bool is_interrupt_executor);

  ScriptExecutor(const ScriptExecutor&) = delete;
  ScriptExecutor& operator=(const ScriptExecutor&) = delete;

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
  UserData* GetMutableUserData() const override;
  UserModel* GetUserModel() const override;

  // Override ScriptExecutorDelegate::NavigationListener
  void OnNavigationStateChanged() override;

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
      bool allow_observer_mode,
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
  std::string GetStatusMessage() const override;
  void SetBubbleMessage(const std::string& message) override;
  std::string GetBubbleMessage() const override;
  void SetTtsMessage(const std::string& message) override;
  TtsButtonState GetTtsButtonState() const override;
  void MaybePlayTtsMessage() override;
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
      base::OnceCallback<void(UserData*, UserDataFieldChange*)>) override;
  void GetFullCard(const autofill::CreditCard* credit_card,
                   GetFullCardCallback callback) override;
  void Prompt(std::unique_ptr<std::vector<UserAction>> user_actions,
              bool disable_force_expand_sheet,
              base::OnceCallback<void()> end_on_navigation_callback,
              bool browse_mode,
              bool browse_mode_invisible) override;
  void CleanUpAfterPrompt(bool consume_touchable_area = true) override;
  void SetBrowseDomainsAllowlist(std::vector<std::string> domains) override;
  void RetrieveElementFormAndFieldData(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&,
                              content::RenderFrameHost* rfh,
                              const autofill::FormData& form_data,
                              const autofill::FormFieldData& field_data)>
          callback) override;
  void StoreScrolledToElement(const ElementFinderResult& element) override;
  void SetTouchableElementArea(
      const ElementAreaProto& touchable_element_area) override;
  void ExpectNavigation() override;
  bool ExpectedNavigationHasStarted() override;
  bool WaitForNavigation(base::OnceCallback<void(bool)> callback) override;
  void WaitForDocumentReadyState(
      base::TimeDelta max_wait_time,
      DocumentReadyState min_ready_state,
      const ElementFinderResult& optional_frame_element,
      base::OnceCallback<void(const ClientStatus&,
                              DocumentReadyState,
                              base::TimeDelta)> callback) override;
  void WaitUntilDocumentIsInReadyState(
      base::TimeDelta max_wait_time,
      DocumentReadyState min_ready_state,
      const ElementFinderResult& optional_frame_element,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback)
      override;

  void LoadURL(const GURL& url) override;
  void Shutdown(bool show_feedback_chip) override;
  void Close() override;
  autofill::PersonalDataManager* GetPersonalDataManager() const override;
  WebsiteLoginManager* GetWebsiteLoginManager() const override;
  password_manager::PasswordChangeSuccessTracker*
  GetPasswordChangeSuccessTracker() const override;
  content::WebContents* GetWebContents() const override;
  JsFlowDevtoolsWrapper* GetJsFlowDevtoolsWrapper() const override;
  ElementStore* GetElementStore() const override;
  WebController* GetWebController() const override;
  std::string GetEmailAddressForAccessTokenAccount() const override;
  ukm::UkmRecorder* GetUkmRecorder() const override;
  void SetDetails(std::unique_ptr<Details> details,
                  base::TimeDelta delay) override;
  void AppendDetails(std::unique_ptr<Details> details,
                     base::TimeDelta delay) override;
  void ClearInfoBox() override;
  void SetInfoBox(const InfoBox& info_box) override;
  bool SetProgressActiveStepIdentifier(
      const std::string& active_step_identifier) override;
  void SetProgressActiveStep(int active_step) override;
  void SetProgressVisible(bool visible) override;
  void SetProgressBarErrorState(bool error) override;
  void SetStepProgressBarConfiguration(
      const ShowProgressBarProto::StepProgressBarConfiguration& configuration)
      override;
  void SetViewportMode(ViewportMode mode) override;
  ViewportMode GetViewportMode() const override;
  void SetPeekMode(ConfigureBottomSheetProto::PeekMode peek_mode) override;
  ConfigureBottomSheetProto::PeekMode GetPeekMode() const override;
  void ExpandBottomSheet() override;
  void CollapseBottomSheet() override;
  void WaitForWindowHeightChange(
      base::OnceCallback<void(const ClientStatus&)> callback) override;
  const ClientSettings& GetSettings() const override;
  void SetClientSettings(const ClientSettingsProto& client_settings) override;
  bool SetForm(
      std::unique_ptr<FormProto> form,
      base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
      base::OnceCallback<void(const ClientStatus&)> cancel_callback) override;
  void ShowQrCodeScanUi(
      std::unique_ptr<PromptQrCodeScanProto> qr_code_scan,
      base::OnceCallback<void(const ClientStatus&,
                              const absl::optional<ValueProto>&)> callback)
      override;
  void ClearQrCodeScanUi() override;
  void RequireUI() override;
  void SetGenericUi(
      std::unique_ptr<GenericUserInterfaceProto> generic_ui,
      base::OnceCallback<void(const ClientStatus&)> end_action_callback,
      base::OnceCallback<void(const ClientStatus&)>
          view_inflation_finished_callback,
      base::RepeatingCallback<void(const RequestBackendDataProto&)>
          request_backend_data_callback,
      base::RepeatingCallback<void(const ShowAccountScreenProto&)>
          show_account_screen_callback) override;
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
  base::WeakPtr<ActionDelegate> GetWeakPtr() override;
  ProcessedActionStatusDetailsProto& GetLogInfo() override;
  void RequestUserData(
      const CollectUserDataOptions& options,
      base::OnceCallback<void(bool, const GetUserDataResponseProto&)> callback)
      override;
  void ShowAccountScreen(const ShowAccountScreenProto& proto,
                         const std::string& email_address) override;
  void SetCollectUserDataUiState(bool loading,
                                 UserDataEventField event_field) override;
  bool SupportsExternalActions() override;
  void RequestExternalAction(
      const ExternalActionProto& external_action,
      base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>
          start_dom_checks_callback,
      base::OnceCallback<void(const external::Result& result)>
          end_action_callback) override;
  bool MustUseBackendData() const override;
  void MaybeSetPreviousAction(
      const ProcessedActionProto& processed_action) override;
  absl::optional<std::string> GetIntent() const override;
  const std::string GetLocale() const override;
  bool IsXmlSigned(const std::string& xml_string) const override;
  const std::vector<std::string> ExtractValuesFromSingleTagXml(
      const std::string& xml_string,
      const std::vector<std::string>& keys) const override;
  void ReportProgress(const std::string& payload,
                      base::OnceCallback<void(bool)> callback) override;

 private:
  // TODO(b/220079189): remove this friend declaration.
  friend class WaitForDomOperation;

  void OnGetActions(base::TimeTicks start_time,
                    int http_status,
                    const std::string& response,
                    const ServiceRequestSender::ResponseInfo& response_info);
  bool ProcessNextActionResponse(
      const std::string& response,
      const ServiceRequestSender::ResponseInfo& response_info);
  void ReportPayloadsToListener();
  void ReportScriptsUpdateToListener(
      std::vector<std::unique_ptr<Script>> scripts);
  void RunCallback(bool success);
  void RunCallbackWithResult(const Result& result);
  void ProcessNextAction();
  void ProcessCurrentAction();
  void GetNextActions();
  void OnProcessedAction(base::TimeTicks start_time,
                         std::unique_ptr<ProcessedActionProto> action);
  void CheckElementConditionMatches(
      const Selector& selector,
      BatchElementChecker* checker,
      base::OnceCallback<void(const ClientStatus&)> callback);
  void CheckElementMatchesCallback(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& status,
      const std::vector<std::string>& ignored_payloads,
      const std::vector<std::string>& ignored_tags,
      const base::flat_map<std::string, DomObjectFrameStack>& ignored_elements);
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
  void OnWaitForDocumentReadyState(
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback,
      const ClientStatus& status,
      DocumentReadyState ready_state,
      base::TimeDelta wait_time);
  void OnRequestUserData(
      base::OnceCallback<void(bool, const GetUserDataResponseProto&)> callback,
      int http_status,
      const std::string& response,
      const ServiceRequestSender::ResponseInfo& response_info);
  void OnExternalActionFinished(
      const ExternalActionProto& external_action,
      const bool prompt,
      base::OnceCallback<void(const external::Result& result)> callback,
      const external::Result& result);
  void OnResume();
  void OnReportProgress(
      base::OnceCallback<void(bool)> callback,
      int http_status,
      const std::string& response,
      const ServiceRequestSender::ResponseInfo& response_info);

  // Maybe shows the message specified in a callout, depending on the current
  // state and client settings.
  bool MaybeShowSlowWarning(const std::string& message, bool enabled);

  // Returns the current ActionData, or nullptr if there is no current action.
  Action::ActionData* GetCurrentActionData();

  // Creates new TriggerContext from |delegate_|'s TriggerContext and
  // |additional_context_|.
  TriggerContext GetMergedTriggerContext() const;

  const std::string script_path_;
  std::unique_ptr<TriggerContext> additional_context_;
  std::string last_global_payload_;
  const std::string initial_script_payload_;
  std::string last_script_payload_;
  const raw_ptr<ScriptExecutor::Listener> listener_;
  const raw_ptr<ScriptExecutorDelegate> delegate_;
  const raw_ptr<ScriptExecutorUiDelegate> ui_delegate_;
  // Set of interrupts that might run during wait for dom or prompt action with
  // allow_interrupt. Sorted by priority; an interrupt that appears on the
  // vector first should run first. Note that the content of this vector can
  // change while the script is running, as a result of OnScriptListChanged
  // being called.
  const raw_ptr<const std::vector<std::unique_ptr<Script>>> ordered_interrupts_;
  std::unique_ptr<ElementStore> element_store_;
  RunScriptCallback callback_;
  std::vector<std::unique_ptr<Action>> actions_;
  std::vector<ProcessedActionProto> processed_actions_;
  AtEnd at_end_ = CONTINUE;
  bool should_stop_script_ = false;
  bool should_clean_contextual_ui_on_finish_ = false;
  ActionProto::ActionInfoCase previous_action_type_ =
      ActionProto::ACTION_INFO_NOT_SET;
  absl::optional<DomObjectFrameStack> last_focused_element_;
  std::unique_ptr<ElementAreaProto> touchable_element_area_;

  std::unique_ptr<content::WebContents> web_contents_for_js_execution_;

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

  // Only set while an action is being executed.
  raw_ptr<Action> current_action_ = nullptr;

  raw_ptr<const UserData> user_data_ = nullptr;

  base::TimeTicks batch_start_time_;
  RoundtripTimingStats roundtrip_timing_stats_;
  RoundtripNetworkStats roundtrip_network_stats_;

  bool connection_warning_already_shown_ = false;
  bool website_warning_already_shown_ = false;
  int consecutive_slow_roundtrip_counter_ = 0;

  uint64_t run_id_ = 0;
  std::string report_token_;

  // Whether this instance is executing an interrupt script or not.
  bool is_interrupt_executor_;

  base::WeakPtrFactory<ScriptExecutor> weak_ptr_factory_{this};
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_H_
