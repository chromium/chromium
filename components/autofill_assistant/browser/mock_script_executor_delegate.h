// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_SCRIPT_EXECUTOR_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_SCRIPT_EXECUTOR_DELEGATE_H_

#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/event_handler.h"
#include "components/autofill_assistant/browser/generic_ui.pb.h"
#include "components/autofill_assistant/browser/info_box.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/viewport_mode.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "components/autofill_assistant/browser/website_login_manager.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace autofill_assistant {

class MockScriptExecutorDelegate : public ScriptExecutorDelegate {
 public:
  MockScriptExecutorDelegate();
  ~MockScriptExecutorDelegate() override;

  MOCK_METHOD(const ClientSettings&, GetSettings, (), (override));
  MOCK_METHOD(const GURL&, GetCurrentURL, (), (override));
  MOCK_METHOD(const GURL&, GetDeeplinkURL, (), (override));
  MOCK_METHOD(const GURL&, GetScriptURL, (), (override));
  MOCK_METHOD(Service*, GetService, (), (override));
  MOCK_METHOD(WebController*, GetWebController, (), (override));
  MOCK_METHOD(const TriggerContext*, GetTriggerContext, (), (override));
  MOCK_METHOD(autofill::PersonalDataManager*,
              GetPersonalDataManager,
              (),
              (override));
  MOCK_METHOD(WebsiteLoginManager*, GetWebsiteLoginManager, (), (override));
  MOCK_METHOD(content::WebContents*, GetWebContents, (), (override));
  MOCK_METHOD(std::string,
              GetEmailAddressForAccessTokenAccount,
              (),
              (override));
  MOCK_METHOD(ukm::UkmRecorder*, GetUkmRecorder, (), (override));
  MOCK_METHOD(bool, EnterState, (AutofillAssistantState state), (override));
  MOCK_METHOD(AutofillAssistantState, GetState, (), (override));
  MOCK_METHOD(void,
              SetOverlayBehavior,
              (ConfigureUiStateProto::OverlayBehavior overlay_behavior),
              (override));
  MOCK_METHOD(void,
              SetTouchableElementArea,
              (const ElementAreaProto& element),
              (override));
  MOCK_METHOD(void, SetStatusMessage, (const std::string& message), (override));
  MOCK_METHOD(std::string, GetStatusMessage, (), (const, override));
  MOCK_METHOD(void, SetBubbleMessage, (const std::string& message), (override));
  MOCK_METHOD(std::string, GetBubbleMessage, (), (const, override));
  MOCK_METHOD(void, SetTtsMessage, (const std::string& message), (override));
  MOCK_METHOD(std::string, GetTtsMessage, (), (const, override));
  MOCK_METHOD(TtsButtonState, GetTtsButtonState, (), (const, override));
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
  MOCK_METHOD(
      void,
      SetLastSuccessfulUserDataOptions,
      (std::unique_ptr<CollectUserDataOptions> collect_user_data_options),
      (override));
  MOCK_METHOD(const CollectUserDataOptions*,
              GetLastSuccessfulUserDataOptions,
              (),
              (const, override));
  MOCK_METHOD(void,
              WriteUserData,
              (base::OnceCallback<void(UserData*, UserData::FieldChange*)>
                   write_callback),
              (override));
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
  MOCK_METHOD(ViewportMode, GetViewportMode, (), (override));
  MOCK_METHOD(void, SetViewportMode, (ViewportMode mode), (override));
  MOCK_METHOD(void,
              SetPeekMode,
              (ConfigureBottomSheetProto::PeekMode peek_mode),
              (override));
  MOCK_METHOD(ConfigureBottomSheetProto::PeekMode, GetPeekMode, (), (override));
  MOCK_METHOD(void, ExpandBottomSheet, (), (override));
  MOCK_METHOD(void, CollapseBottomSheet, (), (override));
  MOCK_METHOD(void,
              SetClientSettings,
              (const ClientSettingsProto& client_settings),
              (override));
  MOCK_METHOD(
      bool,
      SetForm,
      (std::unique_ptr<FormProto> form,
       base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
       base::OnceCallback<void(const ClientStatus&)> cancel_callback),
      (override));
  MOCK_METHOD(UserModel*, GetUserModel, (), (override));
  MOCK_METHOD(EventHandler*, GetEventHandler, (), (override));
  MOCK_METHOD(void, SetShowFeedbackChip, (bool show_feedback_chip), (override));
  MOCK_METHOD(void, ExpectNavigation, (), (override));
  MOCK_METHOD(bool, IsNavigatingToNewDocument, (), (override));
  MOCK_METHOD(bool, HasNavigationError, (), (override));
  MOCK_METHOD(void, RequireUI, (), (override));
  MOCK_METHOD(void,
              AddNavigationListener,
              (NavigationListener * listener),
              (override));
  MOCK_METHOD(void,
              RemoveNavigationListener,
              (NavigationListener * listener),
              (override));
  MOCK_METHOD(void, AddListener, (Listener * listener), (override));
  MOCK_METHOD(void, RemoveListener, (Listener * listener), (override));
  MOCK_METHOD(void, SetExpandSheetForPromptAction, (bool expand), (override));
  MOCK_METHOD(void,
              SetBrowseDomainsAllowlist,
              (std::vector<std::string> domains),
              (override));
  MOCK_METHOD(
      void,
      SetGenericUi,
      (std::unique_ptr<GenericUserInterfaceProto> generic_ui,
       base::OnceCallback<void(const ClientStatus&)> end_action_callback,
       base::OnceCallback<void(const ClientStatus&)>
           view_inflation_finished_callback),
      (override));
  MOCK_METHOD(void,
              SetPersistentGenericUi,
              (std::unique_ptr<GenericUserInterfaceProto> generic_ui,
               base::OnceCallback<void(const ClientStatus&)>
                   view_inflation_finished_callback),
              (override));
  MOCK_METHOD(void, ClearGenericUi, (), (override));
  MOCK_METHOD(void, ClearPersistentGenericUi, (), (override));
  MOCK_METHOD(void, SetBrowseModeInvisible, (bool invisible), (override));
  MOCK_METHOD(bool, ShouldShowWarning, (), (override));
  MOCK_METHOD(ProcessedActionStatusDetailsProto&, GetLogInfo, (), (override));

 private:
  ClientSettings client_settings_;
  ProcessedActionStatusDetailsProto log_info_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_SCRIPT_EXECUTOR_DELEGATE_H_
