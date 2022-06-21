// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/top_padding.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "components/autofill_assistant/browser/web/fake_element_store.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {
class PasswordChangeSuccessTracker;
}

namespace autofill_assistant {
class ElementFinderResult;
class UserModel;

class MockActionDelegate : public ActionDelegate {
 public:
  MockActionDelegate();
  ~MockActionDelegate() override;

  MOCK_METHOD1(RunElementChecks, void(BatchElementChecker*));

  void ShortWaitForElement(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback)
      override {
    OnShortWaitForElement(selector, callback);
  }
  void ShortWaitForElementWithSlowWarning(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback) {
    OnShortWaitForElement(selector, callback);
  }
  MOCK_METHOD2(
      OnShortWaitForElement,
      void(const Selector& selector,
           base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>&));

  MOCK_METHOD6(
      WaitForDom,
      void(base::TimeDelta max_wait_time,
           bool allow_observer_mode,
           bool allow_interrupt,
           WaitForDomObserver* observer,
           base::RepeatingCallback<void(
               BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements,
           base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>
               callback));
  MOCK_METHOD5(
      WaitForDomWithSlowWarning,
      void(base::TimeDelta max_wait_time,
           bool allow_interrupt,
           WaitForDomObserver* observer,
           base::RepeatingCallback<void(
               BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements,
           base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>
               callback));
  MOCK_METHOD1(SetStatusMessage, void(const std::string& message));
  MOCK_CONST_METHOD0(GetStatusMessage, std::string());
  MOCK_METHOD1(SetBubbleMessage, void(const std::string& message));
  MOCK_CONST_METHOD0(GetBubbleMessage, std::string());
  MOCK_METHOD1(SetTtsMessage, void(const std::string& message));
  MOCK_CONST_METHOD0(GetTtsButtonState, TtsButtonState());
  MOCK_METHOD0(MaybePlayTtsMessage, void());
  MOCK_CONST_METHOD2(FindElement,
                     void(const Selector& selector, ElementFinder::Callback));
  MOCK_CONST_METHOD2(FindAllElements,
                     void(const Selector& selector,
                          ElementFinder::Callback callback));
  MOCK_METHOD5(Prompt,
               void(std::unique_ptr<std::vector<UserAction>> user_actions,
                    bool disable_force_expand_sheet,
                    base::OnceCallback<void()> end_on_navigation_callback,
                    bool browse_mode,
                    bool browse_mode_invisible));
  MOCK_METHOD1(CleanUpAfterPrompt, void(bool));
  MOCK_METHOD1(SetBrowseDomainsAllowlist,
               void(std::vector<std::string> domains));
  MOCK_METHOD2(
      RetrieveElementFormAndFieldData,
      void(const Selector& selector,
           base::OnceCallback<void(const ClientStatus&,
                                   const autofill::FormData&,
                                   const autofill::FormFieldData&)> callback));
  MOCK_METHOD1(StoreScrolledToElement,
               void(const ElementFinderResult& element));
  MOCK_METHOD1(SetTouchableElementArea,
               void(const ElementAreaProto& touchable_element_area));
  MOCK_METHOD1(CollectUserData,
               void(CollectUserDataOptions* collect_user_data_options));
  MOCK_METHOD1(
      SetLastSuccessfulUserDataOptions,
      void(std::unique_ptr<CollectUserDataOptions> collect_user_data_options));
  MOCK_CONST_METHOD0(GetLastSuccessfulUserDataOptions,
                     CollectUserDataOptions*());
  MOCK_METHOD1(WriteUserData,
               void(base::OnceCallback<void(UserData*, UserDataFieldChange*)>));
  MOCK_METHOD2(GetFullCard,
               void(const autofill::CreditCard* credit_card,
                    ActionDelegate::GetFullCardCallback callback));
  MOCK_METHOD0(ExpectNavigation, void());
  MOCK_METHOD0(ExpectedNavigationHasStarted, bool());
  MOCK_METHOD1(WaitForNavigation,
               bool(base::OnceCallback<void(bool)> callback));
  MOCK_METHOD1(LoadURL, void(const GURL& url));
  MOCK_METHOD1(Shutdown, void(bool show_feedback_chip));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(Restart, void());
  MOCK_CONST_METHOD0(GetUserData, UserData*());
  MOCK_CONST_METHOD0(GetPersonalDataManager, autofill::PersonalDataManager*());
  MOCK_CONST_METHOD0(GetWebsiteLoginManager, WebsiteLoginManager*());
  MOCK_CONST_METHOD0(GetPasswordChangeSuccessTracker,
                     password_manager::PasswordChangeSuccessTracker*());
  MOCK_CONST_METHOD0(GetWebContents, content::WebContents*());
  MOCK_METHOD(JsFlowDevtoolsWrapper*,
              GetJsFlowDevtoolsWrapper,
              (),
              (const override));
  MOCK_CONST_METHOD0(GetWebController, WebController*());
  MOCK_CONST_METHOD0(GetEmailAddressForAccessTokenAccount, std::string());
  MOCK_CONST_METHOD0(GetUkmRecorder, ukm::UkmRecorder*());
  MOCK_METHOD2(SetDetails,
               void(std::unique_ptr<Details> details, base::TimeDelta delay));
  MOCK_METHOD2(AppendDetails,
               void(std::unique_ptr<Details> details, base::TimeDelta delay));
  MOCK_METHOD1(SetInfoBox, void(const InfoBox& info_box));
  MOCK_METHOD0(ClearInfoBox, void());
  MOCK_METHOD1(SetProgressActiveStepIdentifier,
               bool(const std::string& active_step_identifier));
  MOCK_METHOD1(SetProgressActiveStep, void(int active_step));
  MOCK_METHOD1(SetProgressVisible, void(bool visible));
  MOCK_METHOD1(SetProgressBarErrorState, void(bool error));
  MOCK_METHOD1(SetStepProgressBarConfiguration,
               void(const ShowProgressBarProto::StepProgressBarConfiguration&
                        configuration));
  MOCK_METHOD1(SetUserActions,
               void(std::unique_ptr<std::vector<UserAction>> user_action));
  MOCK_METHOD1(SetViewportMode, void(ViewportMode mode));
  MOCK_CONST_METHOD0(GetViewportMode, ViewportMode());
  MOCK_METHOD1(SetPeekMode,
               void(ConfigureBottomSheetProto::PeekMode peek_mode));
  MOCK_CONST_METHOD0(GetPeekMode, ConfigureBottomSheetProto::PeekMode());
  MOCK_METHOD0(ExpandBottomSheet, void());
  MOCK_METHOD0(CollapseBottomSheet, void());
  MOCK_METHOD1(SetClientSettings,
               void(const ClientSettingsProto& client_settings));
  MOCK_METHOD3(
      SetForm,
      bool(std::unique_ptr<FormProto> form,
           base::RepeatingCallback<void(const FormProto::Result*)>
               changed_callback,
           base::OnceCallback<void(const ClientStatus&)> cancel_callback));
  MOCK_CONST_METHOD0(GetUserModel, UserModel*());
  MOCK_METHOD1(WaitForWindowHeightChange,
               void(base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD4(WaitForDocumentReadyState,
               void(base::TimeDelta max_wait_time,
                    DocumentReadyState min_ready_state,
                    const ElementFinderResult& optional_frame_element,
                    base::OnceCallback<void(const ClientStatus&,
                                            DocumentReadyState,
                                            base::TimeDelta)> callback));
  MOCK_METHOD4(
      WaitUntilDocumentIsInReadyState,
      void(base::TimeDelta,
           DocumentReadyState,
           const ElementFinderResult&,
           base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>));
  MOCK_METHOD0(RequireUI, void());
  MOCK_METHOD0(SetExpandSheetForPromptAction, bool());
  MOCK_METHOD3(
      SetGenericUi,
      void(std::unique_ptr<GenericUserInterfaceProto> generic_ui,
           base::OnceCallback<void(const ClientStatus&)> end_action_callback,
           base::OnceCallback<void(const ClientStatus&)>
               view_inflation_finished_callback));

  MOCK_METHOD2(SetPersistentGenericUi,
               void(std::unique_ptr<GenericUserInterfaceProto> generic_ui,
                    base::OnceCallback<void(const ClientStatus&)>
                        view_inflation_finished_callback));

  MOCK_METHOD0(ClearGenericUi, void());
  MOCK_METHOD0(ClearPersistentGenericUi, void());
  MOCK_METHOD1(SetOverlayBehavior,
               void(ConfigureUiStateProto::OverlayBehavior));
  MOCK_METHOD1(MaybeShowSlowWebsiteWarning,
               void(base::OnceCallback<void(bool)>));
  MOCK_METHOD0(MaybeShowSlowConnectionWarning, void());
  MOCK_METHOD0(GetLogInfo, ProcessedActionStatusDetailsProto&());
  MOCK_CONST_METHOD0(GetElementStore, ElementStore*());
  MOCK_METHOD3(
      RequestUserData,
      void(UserDataEventField event_field,
           const CollectUserDataOptions& options,
           base::OnceCallback<void(bool, const GetUserDataResponseProto&)>
               callback));
  MOCK_METHOD0(SupportsExternalActions, bool());
  MOCK_METHOD3(
      RequestExternalAction,
      void(const ExternalActionProto& external_action,
           base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>
               start_dom_checks_callback,
           base::OnceCallback<void(const external::Result& result)>
               end_action_callback));
  MOCK_CONST_METHOD0(MustUseBackendData, bool());
  MOCK_METHOD1(MaybeSetPreviousAction,
               void(const ProcessedActionProto& processed_action));

  base::WeakPtr<ActionDelegate> GetWeakPtr() const override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  const ClientSettings& GetSettings() const override {
    return client_settings_;
  }

 private:
  FakeElementStore fake_element_store_;
  ClientSettings client_settings_;
  ProcessedActionStatusDetailsProto log_info_;

  base::WeakPtrFactory<MockActionDelegate> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_
