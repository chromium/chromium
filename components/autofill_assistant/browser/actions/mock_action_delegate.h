// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/top_padding.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
class EventHandler;
class UserModel;

class MockActionDelegate : public ActionDelegate {
 public:
  MockActionDelegate();
  ~MockActionDelegate() override;

  MOCK_METHOD1(RunElementChecks, void(BatchElementChecker*));

  void ShortWaitForElement(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&)> callback) override {
    OnShortWaitForElement(selector, callback);
  }
  MOCK_METHOD2(OnShortWaitForElement,
               void(const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&)>&));

  void WaitForDom(
      base::TimeDelta max_wait_time,
      bool allow_interrupt,
      base::RepeatingCallback<
          void(BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements,
      base::OnceCallback<void(const ClientStatus&)> callback) override {
    OnWaitForDom(max_wait_time, allow_interrupt, check_elements, callback);
  }
  MOCK_METHOD4(OnWaitForDom,
               void(base::TimeDelta,
                    bool,
                    base::RepeatingCallback<
                        void(BatchElementChecker*,
                             base::OnceCallback<void(const ClientStatus&)>)>&,
                    base::OnceCallback<void(const ClientStatus&)>&));

  MOCK_METHOD1(SetStatusMessage, void(const std::string& message));

  MOCK_METHOD0(GetStatusMessage, std::string());

  MOCK_METHOD1(SetBubbleMessage, void(const std::string& message));

  MOCK_METHOD0(GetBubbleMessage, std::string());

  MOCK_METHOD2(FindElement,
               void(const Selector& selector, ElementFinder::Callback));

  MOCK_METHOD3(ClickOrTapElement,
               void(ClickType click_type,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));

  MOCK_METHOD2(WaitForDocumentToBecomeInteractive,
               void(const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));

  MOCK_METHOD2(ScrollIntoView,
               void(const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));

  MOCK_METHOD5(Prompt,
               void(std::unique_ptr<std::vector<UserAction>> user_actions,
                    bool disable_force_expand_sheet,
                    base::OnceCallback<void()> end_on_navigation_callback,
                    bool browse_mode,
                    bool browse_mode_invisible));

  MOCK_METHOD0(CleanUpAfterPrompt, void());

  MOCK_METHOD1(SetBrowseDomainsWhitelist,
               void(std::vector<std::string> domains));

  void FillAddressForm(
      const autofill::AutofillProfile* profile,
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&)> callback) override {
    OnFillAddressForm(profile, selector, callback);
  }
  MOCK_METHOD3(OnFillAddressForm,
               void(const autofill::AutofillProfile* profile,
                    const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&)>& callback));

  void FillCardForm(
      std::unique_ptr<autofill::CreditCard> card,
      const base::string16& cvc,
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&)> callback) override {
    OnFillCardForm(card.get(), cvc, selector, callback);
  }

  void RetrieveElementFormAndFieldData(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&,
                              const autofill::FormData&,
                              const autofill::FormFieldData&)> callback)
      override {
    OnRetrieveElementFormAndFieldData(selector, callback);
  }

  MOCK_METHOD2(
      OnRetrieveElementFormAndFieldData,
      void(const Selector& selector,
           base::OnceCallback<void(const ClientStatus&,
                                   const autofill::FormData&,
                                   const autofill::FormFieldData&)>& callback));

  MOCK_METHOD4(OnFillCardForm,
               void(const autofill::CreditCard* card,
                    const base::string16& cvc,
                    const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&)>& callback));

  MOCK_METHOD4(SelectOption,
               void(const std::string& value,
                    DropdownSelectStrategy select_strategy,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));

  MOCK_METHOD3(FocusElement,
               void(const Selector& selector,
                    const TopPadding& top_padding,
                    base::OnceCallback<void(const ClientStatus&)> callback));

  MOCK_METHOD1(SetTouchableElementArea,
               void(const ElementAreaProto& touchable_element_area));

  MOCK_METHOD2(HighlightElement,
               void(const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&)> callback));

  MOCK_METHOD1(CollectUserData,
               void(CollectUserDataOptions* collect_user_data_options));

  MOCK_METHOD1(
      SetLastSuccessfulUserDataOptions,
      void(std::unique_ptr<CollectUserDataOptions> collect_user_data_options));

  MOCK_CONST_METHOD0(GetLastSuccessfulUserDataOptions,
                     CollectUserDataOptions*());

  MOCK_METHOD1(
      WriteUserData,
      void(base::OnceCallback<void(UserData*, UserData::FieldChange*)>));

  void GetFullCard(const autofill::CreditCard* credit_card,
                   ActionDelegate::GetFullCardCallback callback) override {
    OnGetFullCard(credit_card, callback);
  }

  MOCK_METHOD2(
      OnGetFullCard,
      void(const autofill::CreditCard* credit_card,
           base::OnceCallback<void(std::unique_ptr<autofill::CreditCard> card,
                                   const base::string16& cvc)>& callback));

  void GetFieldValue(const Selector& selector,
                     base::OnceCallback<void(const ClientStatus&,
                                             const std::string&)> callback) {
    OnGetFieldValue(selector, callback);
  }
  MOCK_METHOD2(OnGetFieldValue,
               void(const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)>& callback));

  void SetFieldValue(const std::string& value,
                     KeyboardValueFillStrategy fill_strategy,
                     int key_press_delay_in_millisecond,
                     const ElementFinder::Result& element,
                     base::OnceCallback<void(const ClientStatus&)> callback) {
    OnSetFieldValue(value, element, callback);
    OnSetFieldValue(value,
                    fill_strategy == SIMULATE_KEY_PRESSES ||
                        fill_strategy == SIMULATE_KEY_PRESSES_SELECT_VALUE,
                    key_press_delay_in_millisecond, element, callback);
  }
  MOCK_METHOD3(OnSetFieldValue,
               void(const std::string& value,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)>& callback));
  MOCK_METHOD5(OnSetFieldValue,
               void(const std::string& value,
                    bool simulate_key_presses,
                    int delay_in_millisecond,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)>& callback));

  MOCK_METHOD4(SetAttribute,
               void(const std::vector<std::string>& attribute,
                    const std::string& value,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));

  void SendKeyboardInput(
      const std::vector<UChar32>& codepoints,
      int delay_in_millisecond,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback) {
    OnSendKeyboardInput(codepoints, delay_in_millisecond, element, callback);
  }
  MOCK_METHOD4(OnSendKeyboardInput,
               void(const std::vector<UChar32>& codepoints,
                    int delay_in_millisecond,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)>& callback));

  MOCK_METHOD2(GetOuterHtml,
               void(const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)> callback));

  MOCK_METHOD2(GetElementTag,
               void(const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)> callback));

  MOCK_METHOD0(ExpectNavigation, void());
  MOCK_METHOD0(ExpectedNavigationHasStarted, bool());
  MOCK_METHOD1(WaitForNavigation,
               bool(base::OnceCallback<void(bool)> callback));
  MOCK_METHOD1(LoadURL, void(const GURL& url));
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(Restart, void());
  MOCK_CONST_METHOD0(GetUserData, UserData*());
  MOCK_METHOD0(GetPersonalDataManager, autofill::PersonalDataManager*());
  MOCK_METHOD0(GetWebsiteLoginManager, WebsiteLoginManager*());
  MOCK_METHOD0(GetWebContents, content::WebContents*());
  MOCK_METHOD0(GetEmailAddressForAccessTokenAccount, std::string());
  MOCK_METHOD0(GetLocale, std::string());
  MOCK_METHOD1(SetDetails, void(std::unique_ptr<Details> details));
  MOCK_METHOD1(SetInfoBox, void(const InfoBox& info_box));
  MOCK_METHOD0(ClearInfoBox, void());
  MOCK_METHOD1(SetProgress, void(int progress));
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
  MOCK_METHOD0(GetViewportMode, ViewportMode());
  MOCK_METHOD1(SetPeekMode,
               void(ConfigureBottomSheetProto::PeekMode peek_mode));
  MOCK_METHOD0(GetPeekMode, ConfigureBottomSheetProto::PeekMode());
  MOCK_METHOD0(ExpandBottomSheet, void());
  MOCK_METHOD0(CollapseBottomSheet, void());
  MOCK_METHOD3(
      SetForm,
      bool(std::unique_ptr<FormProto> form,
           base::RepeatingCallback<void(const FormProto::Result*)>
               changed_callback,
           base::OnceCallback<void(const ClientStatus&)> cancel_callback));
  MOCK_METHOD0(GetUserModel, UserModel*());
  MOCK_METHOD0(GetEventHandler, EventHandler*());

  void WaitForWindowHeightChange(
      base::OnceCallback<void(const ClientStatus&)> callback) override {
    OnWaitForWindowHeightChange(callback);
  }

  MOCK_METHOD1(OnWaitForWindowHeightChange,
               void(base::OnceCallback<void(const ClientStatus&)>& callback));

  MOCK_METHOD2(
      OnGetDocumentReadyState,
      void(const Selector&,
           base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>&));

  void GetDocumentReadyState(
      const Selector& frame,
      base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
          callback) override {
    OnGetDocumentReadyState(frame, callback);
  }

  MOCK_METHOD3(
      OnWaitForDocumentReadyState,
      void(const Selector&,
           DocumentReadyState min_ready_state,
           base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>&));

  void WaitForDocumentReadyState(
      const Selector& frame,
      DocumentReadyState min_ready_state,
      base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
          callback) override {
    OnWaitForDocumentReadyState(frame, min_ready_state, callback);
  }

  MOCK_METHOD0(RequireUI, void());
  MOCK_METHOD0(SetExpandSheetForPromptAction, bool());

  MOCK_METHOD3(
      OnSetGenericUi,
      void(std::unique_ptr<GenericUserInterfaceProto> generic_ui,
           base::OnceCallback<void(const ClientStatus&)>& end_action_callback,
           base::OnceCallback<void(const ClientStatus&)>&
               view_inflation_finished_callback));

  void SetGenericUi(
      std::unique_ptr<GenericUserInterfaceProto> generic_ui,
      base::OnceCallback<void(const ClientStatus&)> end_action_callback,
      base::OnceCallback<void(const ClientStatus&)>
          view_inflation_finished_callback) override {
    OnSetGenericUi(std::move(generic_ui), end_action_callback,
                   view_inflation_finished_callback);
  }
  MOCK_METHOD0(ClearGenericUi, void());
  MOCK_METHOD1(SetOverlayBehavior,
               void(ConfigureUiStateProto::OverlayBehavior));

  base::WeakPtr<ActionDelegate> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  const ClientSettings& GetSettings() override { return client_settings_; }

  ClientSettings client_settings_;

  base::WeakPtrFactory<MockActionDelegate> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_
