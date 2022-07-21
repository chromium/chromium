// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill_assistant/browser/autofill_assistant_tts_controller.h"
#include "components/autofill_assistant/browser/basic_interactions.h"
#include "components/autofill_assistant/browser/bottom_sheet_state.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/element_area.h"
#include "components/autofill_assistant/browser/empty_controller_observer.h"
#include "components/autofill_assistant/browser/event_handler.h"
#include "components/autofill_assistant/browser/execution_delegate.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/public/runtime_manager_impl.h"
#include "components/autofill_assistant/browser/script_executor_ui_delegate.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/ui_controller_observer.h"
#include "components/autofill_assistant/browser/ui_delegate.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {
class UiControllerTest;

// UiController controls the Autofill Assistant UI.
// This class includes the logic and state specific to this UI.
// Anything non-UI related or anything which is shared with other UI
// implementations should be in Controller instead.
class UiController : public ScriptExecutorUiDelegate,
                     public virtual UiDelegate,
                     public AutofillAssistantTtsController::TtsEventDelegate,
                     public EmptyControllerObserver,
                     private UserModel::Observer {
 public:
  // |client| and |execution_delegate| must remain valid for the lifetime of the
  // instance.
  UiController(Client* client,
               ExecutionDelegate* execution_delegate,
               std::unique_ptr<AutofillAssistantTtsController> tts_controller);

  UiController(const UiController&) = delete;
  UiController& operator=(const UiController&) = delete;

  ~UiController() override;

  // The UiController starts listening for notifications.
  void StartListening();

  // Called when an accessibility service with "FEEDBACK_SPOKEN" feedback type
  // is enabled or disabled.
  void OnSpokenFeedbackAccessibilityServiceChanged(bool enabled);
  base::Value GetDebugContext() const;

  // Overrides ScriptExecutorUiDelegate
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
  void SetPeekMode(ConfigureBottomSheetProto::PeekMode peek_mode) override;
  void ExpandBottomSheet() override;
  void CollapseBottomSheet() override;
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
  void SetShowFeedbackChip(bool show_feedback_chip) override;

  void SetExpandSheetForPromptAction(bool expand) override;
  void SetCollectUserDataOptions(CollectUserDataOptions* options) override;
  void SetCollectUserDataUiState(bool loading,
                                 UserDataEventField event_field) override;
  void SetLastSuccessfulUserDataOptions(std::unique_ptr<CollectUserDataOptions>
                                            collect_user_data_options) override;
  const CollectUserDataOptions* GetLastSuccessfulUserDataOptions()
      const override;
  void OnInterruptStarted() override;
  void OnInterruptFinished() override;

  // Overrides autofill_assistant::UiDelegate:
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
  const CollectUserDataOptions* GetCollectUserDataOptions() const override;
  void HandleShippingAddressChange(
      std::unique_ptr<autofill::AutofillProfile> address,
      UserDataEventType event_type) override;
  void HandleContactInfoChange(
      std::unique_ptr<autofill::AutofillProfile> profile,
      UserDataEventType event_type) override;
  void HandlePhoneNumberChange(
      std::unique_ptr<autofill::AutofillProfile> profile,
      UserDataEventType event_type) override;
  void HandleCreditCardChange(
      std::unique_ptr<autofill::CreditCard> card,
      std::unique_ptr<autofill::AutofillProfile> billing_profile,
      UserDataEventType event_type) override;
  void SetTermsAndConditions(
      TermsAndConditionsState terms_and_conditions) override;
  void SetLoginOption(const std::string& identifier) override;
  void OnTextLinkClicked(int link) override;
  void OnFormActionLinkClicked(int link) override;
  void OnTtsButtonClicked() override;
  void SetAdditionalValue(const std::string& client_memory_key,
                          const ValueProto& value) override;
  void OnQrCodeScanFinished(const ClientStatus& status,
                            const absl::optional<ValueProto>& value) override;
  ConfigureBottomSheetProto::PeekMode GetPeekMode() override;
  BottomSheetState GetBottomSheetState() override;
  void SetBottomSheetState(BottomSheetState state) override;
  const FormProto* GetForm() const override;
  const FormProto::Result* GetFormResult() const override;
  void SetCounterValue(int input_index, int counter_index, int value) override;
  void SetChoiceSelected(int input_index,
                         int choice_index,
                         bool selected) override;
  void AddObserver(UiControllerObserver* observer) override;
  void RemoveObserver(const UiControllerObserver* observer) override;
  bool ShouldPromptActionExpandSheet() const override;
  BasicInteractions* GetBasicInteractions() override;
  const PromptQrCodeScanProto* GetPromptQrCodeScanProto() const override;
  const GenericUserInterfaceProto* GetGenericUiProto() const override;
  const GenericUserInterfaceProto* GetPersistentGenericUiProto() const override;
  void OnKeyboardVisibilityChanged(bool visible) override;
  void OnInputTextFocusChanged(bool is_text_focused) override;
  EventHandler* GetEventHandler() override;
  void DispatchEvent(const EventHandler::EventKey& key) override;
  bool SupportsExternalActions() override;
  void ExecuteExternalAction(
      const external::Action& external_action,
      base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>
          start_dom_checks_callback,
      base::OnceCallback<void(const external::Result& result)>
          end_action_callback) override;

  // Overrides ControllerObserver.
  void OnError(const std::string& error_message,
               Metrics::DropOutReason reason) override;
  void OnUserDataChanged(const UserData& user_data,
                         UserDataFieldChange field_change) override;
  void OnExecuteScript(const std::string& start_message) override;
  void OnStart(const TriggerContext& trigger_context) override;
  void OnStop() override;
  void OnResetState() override;
  void OnUiShownChanged(bool shown) override;

  // Overrides AutofillAssistantTtsController::TtsEventDelegate
  void OnTtsEvent(AutofillAssistantTtsController::TtsEventType event) override;

 private:
  friend UiControllerTest;

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

  void InitFromParameters(const TriggerContext& trigger_context);
  void UpdateCollectUserDataActions();
  void OnCollectUserDataContinueButtonClicked();
  void OnCollectUserDataAdditionalActionTriggered(int index);

  // Overrides autofill_assistant::UserModel::Observer:
  void OnValueChanged(const std::string& identifier,
                      const ValueProto& new_value) override;

  void SetProfile(const std::string& key,
                  UserDataFieldChange field_change,
                  std::unique_ptr<autofill::AutofillProfile> profile);

  void ReloadUserData(UserDataEventField event_field,
                      UserDataEventType event_type);

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

  void SetInitialState();
  void SetStoppedState();
  void OnFeedbackSent();

  UserData* GetUserData();
  UserModel* GetUserModel();

  const raw_ptr<Client> client_;

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

  // Current peek mode.
  ConfigureBottomSheetProto::PeekMode peek_mode_ =
      ConfigureBottomSheetProto::HANDLE;

  // The latest bottom sheet state stored.
  BottomSheetState bottom_sheet_state_ = BottomSheetState::UNDEFINED;
  // A copy of the most recently set user data options. Can be used to determine
  // which information was requested.
  std::unique_ptr<CollectUserDataOptions> last_collect_user_data_options_;
  raw_ptr<CollectUserDataOptions> collect_user_data_options_ = nullptr;

  std::unique_ptr<FormProto> form_;
  std::unique_ptr<FormProto::Result> form_result_;
  base::RepeatingCallback<void(const FormProto::Result*)>
      form_changed_callback_ = base::DoNothing();
  base::OnceCallback<void(const ClientStatus&)> form_cancel_callback_ =
      base::DoNothing();

  base::ObserverList<UiControllerObserver> observers_;

  raw_ptr<ExecutionDelegate> execution_delegate_;
  EventHandler event_handler_;
  BasicInteractions basic_interactions_{this, execution_delegate_};

  bool expand_sheet_for_prompt_action_ = true;
  bool browse_mode_invisible_ = false;
  bool is_keyboard_showing_ = false;
  bool is_focus_on_bottom_sheet_text_input_ = false;
  bool show_feedback_chip_ = false;
  bool are_chips_visible_ = true;

  bool tts_enabled_ = false;
  std::unique_ptr<AutofillAssistantTtsController> tts_controller_;
  TtsButtonState tts_button_state_ = TtsButtonState::DEFAULT;

  // Only set during a PromptQrCodeScanAction.
  std::unique_ptr<PromptQrCodeScanProto> qr_code_scan_;
  base::OnceCallback<void(const ClientStatus&,
                          const absl::optional<ValueProto>&)>
      qr_code_scan_callback_;

  // Only set during a ShowGenericUiAction.
  std::unique_ptr<GenericUserInterfaceProto> generic_user_interface_;

  std::unique_ptr<GenericUserInterfaceProto> persistent_generic_user_interface_;

  base::WeakPtrFactory<UiController> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_CONTROLLER_H_
