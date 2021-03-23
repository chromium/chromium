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
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/top_padding.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "components/autofill_assistant/browser/web/fake_element_store.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
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
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback)
      override {
    OnShortWaitForElement(selector, callback);
  }
  MOCK_METHOD2(
      OnShortWaitForElement,
      void(const Selector& selector,
           base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>&));

  void ShortWaitForElementWithSlowWarning(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback)
      override {
    OnShortWaitForElement(selector, callback);
  }
  MOCK_METHOD2(
      OnShortWaitForElementWithSlowWarning,
      void(const Selector& selector,
           base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>&));

  void WaitForDom(
      base::TimeDelta max_wait_time,
      bool allow_interrupt,
      WaitForDomObserver* observer,
      base::RepeatingCallback<
          void(BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback)
      override {
    OnWaitForDom(max_wait_time, allow_interrupt, check_elements, callback);
  }
  MOCK_METHOD4(
      OnWaitForDom,
      void(base::TimeDelta,
           bool,
           base::RepeatingCallback<
               void(BatchElementChecker*,
                    base::OnceCallback<void(const ClientStatus&)>)>&,
           base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>&));

  void WaitForDomWithSlowWarning(
      base::TimeDelta max_wait_time,
      bool allow_interrupt,
      WaitForDomObserver* observer,
      base::RepeatingCallback<
          void(BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback)
      override {
    OnWaitForDom(max_wait_time, allow_interrupt, check_elements, callback);
  }
  MOCK_METHOD4(
      OnWaitForDomWithSlowWarning,
      void(base::TimeDelta,
           bool,
           base::RepeatingCallback<
               void(BatchElementChecker*,
                    base::OnceCallback<void(const ClientStatus&)>)>&,
           base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>&));

  MOCK_METHOD1(SetStatusMessage, void(const std::string& message));

  MOCK_METHOD0(GetStatusMessage, std::string());

  MOCK_METHOD1(SetBubbleMessage, void(const std::string& message));

  MOCK_METHOD0(GetBubbleMessage, std::string());

  MOCK_CONST_METHOD2(FindElement,
                     void(const Selector& selector, ElementFinder::Callback));

  MOCK_CONST_METHOD2(FindAllElements,
                     void(const Selector& selector,
                          ElementFinder::Callback callback));

  MOCK_METHOD3(ClickOrTapElement,
               void(ClickType click_type,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));

  MOCK_METHOD4(WaitUntilElementIsStable,
               void(int,
                    base::TimeDelta,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&,
                                            base::TimeDelta)> callback));

  MOCK_METHOD5(Prompt,
               void(std::unique_ptr<std::vector<UserAction>> user_actions,
                    bool disable_force_expand_sheet,
                    base::OnceCallback<void()> end_on_navigation_callback,
                    bool browse_mode,
                    bool browse_mode_invisible));

  MOCK_METHOD0(CleanUpAfterPrompt, void());

  MOCK_METHOD1(SetBrowseDomainsAllowlist,
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

  MOCK_METHOD5(SelectOption,
               void(const std::string& re2,
                    bool case_sensitive,
                    SelectOptionProto::OptionComparisonAttribute
                        option_comparison_attribute,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));

  MOCK_METHOD5(ScrollToElementPosition,
               void(const Selector& selector,
                    const TopPadding& top_padding,
                    std::unique_ptr<ElementFinder::Result> scrollable_element,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));

  MOCK_METHOD1(SetTouchableElementArea,
               void(const ElementAreaProto& touchable_element_area));

  MOCK_METHOD2(HighlightElement,
               void(const ElementFinder::Result& element,
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

  MOCK_METHOD2(GetFullCard,
               void(const autofill::CreditCard* credit_card,
                    ActionDelegate::GetFullCardCallback callback));

  MOCK_METHOD2(GetFieldValue,
               void(const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)> callback));

  MOCK_METHOD3(GetStringAttribute,
               void(const std::vector<std::string>& attributes,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)> callback));

  MOCK_METHOD3(SetValueAttribute,
               void(const std::string& value,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> callback));

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

  MOCK_METHOD0(ExpectNavigation, void());
  MOCK_METHOD0(ExpectedNavigationHasStarted, bool());
  MOCK_METHOD1(WaitForNavigation,
               bool(base::OnceCallback<void(bool)> callback));
  MOCK_METHOD1(LoadURL, void(const GURL& url));
  MOCK_METHOD1(Shutdown, void(bool show_feedback_chip));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(Restart, void());
  MOCK_CONST_METHOD0(GetUserData, UserData*());
  MOCK_METHOD0(GetPersonalDataManager, autofill::PersonalDataManager*());
  MOCK_METHOD0(GetWebsiteLoginManager, WebsiteLoginManager*());
  MOCK_METHOD0(GetWebContents, content::WebContents*());
  MOCK_CONST_METHOD0(GetWebController, WebController*());
  MOCK_METHOD0(GetEmailAddressForAccessTokenAccount, std::string());
  MOCK_METHOD0(GetLocale, std::string());
  MOCK_METHOD2(SetDetails,
               void(std::unique_ptr<Details> details, base::TimeDelta delay));
  MOCK_METHOD2(AppendDetails,
               void(std::unique_ptr<Details> details, base::TimeDelta delay));
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

  MOCK_METHOD3(OnWaitForDocumentReadyState,
               void(DocumentReadyState,
                    const ElementFinder::Result&,
                    base::OnceCallback<void(const ClientStatus&,
                                            DocumentReadyState,
                                            base::TimeDelta)>&));

  void WaitForDocumentReadyState(
      base::TimeDelta max_wait_time,
      DocumentReadyState min_ready_state,
      const ElementFinder::Result& optional_frame_element,
      base::OnceCallback<void(const ClientStatus&,
                              DocumentReadyState,
                              base::TimeDelta)> callback) override {
    OnWaitForDocumentReadyState(min_ready_state, optional_frame_element,
                                callback);
  }

  MOCK_METHOD4(
      WaitUntilDocumentIsInReadyState,
      void(base::TimeDelta,
           DocumentReadyState,
           const ElementFinder::Result&,
           base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>));

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

  MOCK_METHOD1(MaybeShowSlowWebsiteWarning,
               void(base::OnceCallback<void(bool)>));
  MOCK_METHOD0(MaybeShowSlowConnectionWarning, void());

  MOCK_CONST_METHOD1(OnDispatchJsEvent,
                     void(base::OnceCallback<void(const ClientStatus&)>));
  void DispatchJsEvent(
      base::OnceCallback<void(const ClientStatus&)> callback) const override {
    OnDispatchJsEvent(std::move(callback));
  }

  base::WeakPtr<ActionDelegate> GetWeakPtr() const override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  const ClientSettings& GetSettings() const override {
    return client_settings_;
  }

  ElementStore* GetElementStore() const override {
    if (!element_store_) {
      element_store_ = std::make_unique<FakeElementStore>();
    }
    return element_store_.get();
  }

  ClientSettings client_settings_;
  mutable std::unique_ptr<ElementStore> element_store_;

  base::WeakPtrFactory<MockActionDelegate> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_
