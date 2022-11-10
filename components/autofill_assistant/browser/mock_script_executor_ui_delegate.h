// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_SCRIPT_EXECUTOR_UI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_SCRIPT_EXECUTOR_UI_DELEGATE_H_

#include "components/autofill_assistant/browser/script_executor_ui_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockScriptExecutorUiDelegate : public ScriptExecutorUiDelegate {
 public:
  MockScriptExecutorUiDelegate();
  ~MockScriptExecutorUiDelegate() override;

  // Override ScriptExecutorUiDelegate:
  MOCK_METHOD(void, SetStatusMessage, (const std::string& message), (override));
  MOCK_METHOD(std::string, GetStatusMessage, (), (const override));
  MOCK_METHOD(void, SetBubbleMessage, (const std::string& message), (override));
  MOCK_METHOD(std::string, GetBubbleMessage, (), (const override));
  MOCK_METHOD(void, SetTtsMessage, (const std::string& message), (override));
  MOCK_METHOD(std::string, GetTtsMessage, (), (const override));
  MOCK_METHOD(TtsButtonState, GetTtsButtonState, (), (const override));
  MOCK_METHOD(void, MaybePlayTtsMessage, (), (override));
  MOCK_METHOD(void,
              SetDetails,
              (std::unique_ptr<Details> details, base::TimeDelta delay),
              (override));
  MOCK_METHOD(void,
              AppendDetails,
              (std::unique_ptr<Details> details, base::TimeDelta delay),
              (override));
  MOCK_METHOD(void, SetInfoBox, (const InfoBox& info_box), (override));
  MOCK_METHOD(void, ClearInfoBox, (), (override));
  MOCK_METHOD(void,
              SetCollectUserDataOptions,
              (CollectUserDataOptions * collect_user_data_options),
              (override));
  MOCK_METHOD(void,
              SetCollectUserDataUiState,
              (bool loading, UserDataEventField event_field),
              (override));
  MOCK_METHOD(
      void,
      SetLastSuccessfulUserDataOptions,
      (std::unique_ptr<CollectUserDataOptions> collect_user_data_options),
      (override));
  MOCK_METHOD(const CollectUserDataOptions*,
              GetLastSuccessfulUserDataOptions,
              (),
              (const override));
  MOCK_METHOD(bool,
              SetProgressActiveStepIdentifier,
              (const std::string& active_step_identifier),
              (override));
  MOCK_METHOD(void, SetProgressActiveStep, (int active_step), (override));
  MOCK_METHOD(void, SetProgressVisible, (bool visible), (override));
  MOCK_METHOD(void, SetProgressBarErrorState, (bool error), (override));
  MOCK_METHOD(
      void,
      SetStepProgressBarConfiguration,
      (const ShowProgressBarProto::StepProgressBarConfiguration& configuration),
      (override));
  MOCK_METHOD(void,
              SetUserActions,
              (std::unique_ptr<std::vector<UserAction>> user_action),
              (override));
  MOCK_METHOD(void,
              SetLegalDisclaimer,
              (std::unique_ptr<LegalDisclaimerProto> legal_disclaimer,
               base::OnceCallback<void(int)> legal_disclaimer_link_callback),
              (override));
  MOCK_METHOD(void,
              SetPeekMode,
              (ConfigureBottomSheetProto::PeekMode peek_mode),
              (override));
  MOCK_METHOD(ConfigureBottomSheetProto::PeekMode, GetPeekMode, (), (override));
  MOCK_METHOD(void, ExpandBottomSheet, (), (override));
  MOCK_METHOD(void, CollapseBottomSheet, (), (override));
  MOCK_METHOD(
      bool,
      SetForm,
      (std::unique_ptr<FormProto> form,
       base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
       base::OnceCallback<void(const ClientStatus&)> cancel_callback),
      (override));
  MOCK_METHOD(void, SetShowFeedbackChip, (bool show_feedback_chip), (override));
  MOCK_METHOD(void, SetExpandSheetForPromptAction, (bool expand), (override));
  MOCK_METHOD(
      void,
      ShowQrCodeScanUi,
      (std::unique_ptr<PromptQrCodeScanProto> qr_code_scan,
       base::OnceCallback<void(const ClientStatus&,
                               const absl::optional<ValueProto>&)> callback),
      (override));
  MOCK_METHOD(void, ClearQrCodeScanUi, (), (override));
  MOCK_METHOD(
      void,
      SetGenericUi,
      (std::unique_ptr<GenericUserInterfaceProto> generic_ui,
       base::OnceCallback<void(const ClientStatus&)> end_action_callback,
       base::OnceCallback<void(const ClientStatus&)>
           view_inflation_finished_callback,
       base::RepeatingCallback<void(const RequestBackendDataProto&)>
           request_backend_data_callback,
       base::RepeatingCallback<void(const ShowAccountScreenProto&)>
           show_account_screen_callback),
      (override));
  MOCK_METHOD(void,
              ShowAccountScreen,
              (const ShowAccountScreenProto& proto,
               const std::string& email_address),
              (override));
  MOCK_METHOD(void,
              SetPersistentGenericUi,
              (std::unique_ptr<GenericUserInterfaceProto> generic_ui,
               base::OnceCallback<void(const ClientStatus&)>
                   view_inflation_finished_callback),
              (override));
  MOCK_METHOD(void, ClearGenericUi, (), (override));
  MOCK_METHOD(void, ClearPersistentGenericUi, (), (override));
  MOCK_METHOD(bool, SupportsExternalActions, (), (override));
  MOCK_METHOD(
      void,
      ExecuteExternalAction,
      (const external::Action& external_action,
       bool is_interrupt,
       base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>
           start_dom_checks_callback,
       base::OnceCallback<void(const external::Result& result)>
           end_action_callback),
      (override));

  // Override ScriptExecutorUiDelegate::WaitForDomObserver:
  MOCK_METHOD(void, OnInterruptStarted, (), (override));
  MOCK_METHOD(void, OnInterruptFinished, (), (override));
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_SCRIPT_EXECUTOR_UI_DELEGATE_H_
