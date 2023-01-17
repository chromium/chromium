// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

using testing::_;
using testing::NiceMock;

namespace autofill {

namespace {

// A constant value to use as an Autofill profile ID.
const int kAutofillProfileId = 1;

class MockAutofillDriver : public TestAutofillDriver {
 public:
  MockAutofillDriver() = default;
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;
  // Mock methods to enable testability.
  MOCK_METHOD(void,
              RendererShouldAcceptDataListSuggestion,
              (const FieldGlobalId&, const std::u16string&),
              (override));
  MOCK_METHOD(void, RendererShouldClearFilledSection, (), (override));
  MOCK_METHOD(void, RendererShouldClearPreviewedForm, (), (override));
  MOCK_METHOD(void,
              RendererShouldFillFieldWithValue,
              (const FieldGlobalId&, const std::u16string&),
              (override));
  MOCK_METHOD(void,
              RendererShouldPreviewFieldWithValue,
              (const FieldGlobalId&, const std::u16string&),
              (override));
};

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
  MockAutofillClient(const MockAutofillClient&) = delete;
  MockAutofillClient& operator=(const MockAutofillClient&) = delete;
  MOCK_METHOD(void,
              ScanCreditCard,
              (CreditCardScanCallback callbacK),
              (override));
  MOCK_METHOD(void,
              ShowAutofillPopup,
              (const autofill::AutofillClient::PopupOpenArgs& open_args,
               base::WeakPtr<AutofillPopupDelegate> delegate),
              (override));
  MOCK_METHOD(void,
              UpdateAutofillPopupDataListValues,
              (const std::vector<std::u16string>& values,
               const std::vector<std::u16string>& lables),
              (override));
  MOCK_METHOD(void, HideAutofillPopup, (PopupHidingReason), (override));
  MOCK_METHOD(void, ExecuteCommand, (int), (override));
  MOCK_METHOD(void,
              OpenPromoCodeOfferDetailsURL,
              (const GURL& url),
              (override));

#if BUILDFLAG(IS_IOS)
  // Mock the client query ID check.
  bool IsLastQueriedField(FieldGlobalId field_id) override {
    return !last_queried_field_id_ || last_queried_field_id_ == field_id;
  }

  void set_last_queried_field(FieldGlobalId field_id) {
    last_queried_field_id_ = field_id;
  }

 private:
  FieldGlobalId last_queried_field_id_;
#endif
};

class MockBrowserAutofillManager : public BrowserAutofillManager {
 public:
  MockBrowserAutofillManager(AutofillDriver* driver, MockAutofillClient* client)
      : BrowserAutofillManager(driver,
                               client,
                               "en-US",
                               EnableDownloadManager(false)) {}
  MockBrowserAutofillManager(const MockBrowserAutofillManager&) = delete;
  MockBrowserAutofillManager& operator=(const MockBrowserAutofillManager&) =
      delete;

  PopupType GetPopupType(const FormData& form,
                         const FormFieldData& field) override {
    return PopupType::kPersonalInformation;
  }

  MOCK_METHOD(bool,
              ShouldShowScanCreditCard,
              (const FormData& form, const FormFieldData& field),
              (override));
  MOCK_METHOD(bool,
              ShouldShowCreditCardSigninPromo,
              (const FormData& form, const FormFieldData& field),
              (override));
  MOCK_METHOD(void,
              OnUserHideSuggestions,
              (const FormData& form, const FormFieldData& field),
              (override));
  MOCK_METHOD(void,
              FillOrPreviewVirtualCardInformation,
              (mojom::RendererFormDataAction action,
               const std::string& guid,
               const FormData& form,
               const FormFieldData& field),
              (override));

  bool ShouldShowCardsFromAccountOption(const FormData& form,
                                        const FormFieldData& field) override {
    return should_show_cards_from_account_option_;
  }

  void ShowCardsFromAccountOption() {
    should_show_cards_from_account_option_ = true;
  }

  MOCK_METHOD(void,
              FillOrPreviewForm,
              (mojom::RendererFormDataAction action,
               const FormData& form,
               const FormFieldData& field,
               int unique_id),
              (override));
  MOCK_METHOD(void,
              FillCreditCardFormImpl,
              (const FormData& form,
               const FormFieldData& field,
               const CreditCard& credit_card,
               const std::u16string& cvc),
              (override));

 private:
  bool should_show_cards_from_account_option_ = false;
};

}  // namespace

class AutofillExternalDelegateUnitTest : public testing::Test {
 protected:
  void SetUp() override {
    autofill_driver_ = std::make_unique<NiceMock<MockAutofillDriver>>();
    browser_autofill_manager_ =
        std::make_unique<NiceMock<MockBrowserAutofillManager>>(
            autofill_driver_.get(), &autofill_client_);
    external_delegate_ = std::make_unique<AutofillExternalDelegate>(
        browser_autofill_manager_.get(), autofill_driver_.get());
  }

  void TearDown() override {
    // Order of destruction is important as BrowserAutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    browser_autofill_manager_.reset();
    external_delegate_.reset();
    autofill_driver_.reset();
  }

  // Issue an OnQuery call.
  void IssueOnQuery() {
    FormData form;
    form.host_frame = form_id_.frame_token;
    form.unique_renderer_id = form_id_.renderer_id;
    FormFieldData field;
    field.host_frame = field_id_.frame_token;
    field.unique_renderer_id = field_id_.renderer_id;
    field.host_form_id = form.unique_renderer_id;
    field.is_focusable = true;
    field.should_autocomplete = true;
    external_delegate_->OnQuery(form, field, gfx::RectF());
  }

  void IssueOnSuggestionsReturned(FieldGlobalId field_id) {
    std::vector<Suggestion> suggestions;
    suggestions.emplace_back();
    suggestions[0].frontend_id = kAutofillProfileId;
    external_delegate_->OnSuggestionsReturned(field_id, suggestions,
                                              AutoselectFirstSuggestion(false));
  }

  base::test::TaskEnvironment task_environment_;
  test::AutofillEnvironment autofill_environment_;

  NiceMock<MockAutofillClient> autofill_client_;
  std::unique_ptr<NiceMock<MockAutofillDriver>> autofill_driver_;
  std::unique_ptr<MockBrowserAutofillManager> browser_autofill_manager_;
  std::unique_ptr<AutofillExternalDelegate> external_delegate_;

  FormGlobalId form_id_ = test::MakeFormGlobalId();
  FieldGlobalId field_id_ = test::MakeFieldGlobalId();
};

// Variant for use in cases when we expect the BrowserAutofillManager would
// normally set the |should_show_cards_from_account_option_| bit.
class AutofillExternalDelegateCardsFromAccountTest
    : public AutofillExternalDelegateUnitTest {
 protected:
  void SetUp() override {
    AutofillExternalDelegateUnitTest::SetUp();
    browser_autofill_manager_->ShowCardsFromAccountOption();
  }
};

// Test that our external delegate called the virtual methods at the right time.
TEST_F(AutofillExternalDelegateUnitTest, TestExternalDelegateVirtualCalls) {
  IssueOnQuery();

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids =
      testing::ElementsAre(kAutofillProfileId,
#if !BUILDFLAG(IS_ANDROID)
                           static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
                           static_cast<int>(POPUP_ITEM_ID_AUTOFILL_OPTIONS));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].frontend_id = kAutofillProfileId;
  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            AutoselectFirstSuggestion(false));
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIdsAre(element_ids));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);

  EXPECT_CALL(*browser_autofill_manager_,
              FillOrPreviewForm(mojom::RendererFormDataAction::kFill, _, _, _));
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));

  // This should trigger a call to hide the popup since we've selected an
  // option.
  external_delegate_->DidAcceptSuggestion(autofill_item[0], 0);
}

// Test that our external delegate does not add the signin promo and its
// separator in the popup items when there are suggestions.
TEST_F(AutofillExternalDelegateUnitTest,
       TestSigninPromoIsNotAdded_WithSuggestions) {
  EXPECT_CALL(*browser_autofill_manager_, ShouldShowCreditCardSigninPromo(_, _))
      .WillOnce(testing::Return(true));

  IssueOnQuery();

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids =
      testing::ElementsAre(kAutofillProfileId,
#if !BUILDFLAG(IS_ANDROID)
                           static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
                           static_cast<int>(POPUP_ITEM_ID_AUTOFILL_OPTIONS));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  base::UserActionTester user_action_tester;

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].frontend_id = kAutofillProfileId;
  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            AutoselectFirstSuggestion(false));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Signin_Impression_FromAutofillDropdown"));
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIdsAre(element_ids));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);

  EXPECT_CALL(*browser_autofill_manager_,
              FillOrPreviewForm(mojom::RendererFormDataAction::kFill, _, _, _));
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));

  // This should trigger a call to hide the popup since we've selected an
  // option.
  external_delegate_->DidAcceptSuggestion(autofill_item[0], 0);
}

// Test that our external delegate properly adds the signin promo and no
// separator in the dropdown, when there are no suggestions.
TEST_F(AutofillExternalDelegateUnitTest,
       TestSigninPromoIsAdded_WithNoSuggestions) {
  EXPECT_CALL(*browser_autofill_manager_, ShouldShowCreditCardSigninPromo(_, _))
      .WillOnce(testing::Return(true));

  IssueOnQuery();

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids = testing::ElementsAre(
      static_cast<int>(POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO));

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  base::UserActionTester user_action_tester;

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> items;
  external_delegate_->OnSuggestionsReturned(field_id_, items,
                                            AutoselectFirstSuggestion(false));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Impression_FromAutofillDropdown"));
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIdsAre(element_ids));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);

  EXPECT_CALL(autofill_client_,
              ExecuteCommand(autofill::POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO));
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));

  // This should trigger a call to start the signin flow and hide the popup
  // since we've selected the sign-in promo option.
  external_delegate_->DidAcceptSuggestion(
      Suggestion(POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO), 0);
}

// Test that data list elements for a node will appear in the Autofill popup.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateDataList) {
  IssueOnQuery();

  std::vector<std::u16string> data_list_items;
  data_list_items.emplace_back();

  EXPECT_CALL(autofill_client_, UpdateAutofillPopupDataListValues(
                                    data_list_items, data_list_items));

  external_delegate_->SetCurrentDataListValues(data_list_items,
                                               data_list_items);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids =
      testing::ElementsAre(static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY),
#if !BUILDFLAG(IS_ANDROID)
                           static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
                           kAutofillProfileId,
#if !BUILDFLAG(IS_ANDROID)
                           static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
                           static_cast<int>(POPUP_ITEM_ID_AUTOFILL_OPTIONS));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].frontend_id = kAutofillProfileId;
  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            AutoselectFirstSuggestion(false));
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIdsAre(element_ids));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);

  // Try calling OnSuggestionsReturned with no Autofill values and ensure
  // the datalist items are still shown.
  // The enum must be cast to an int to prevent compile errors on linux_rel.

  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  autofill_item.clear();
  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            AutoselectFirstSuggestion(false));
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(testing::ElementsAre(
                  static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY))));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
}

// Test that datalist values can get updated while a popup is showing.
TEST_F(AutofillExternalDelegateUnitTest, UpdateDataListWhileShowingPopup) {
  IssueOnQuery();

  EXPECT_CALL(autofill_client_, ShowAutofillPopup).Times(0);

  // Make sure just setting the data list values doesn't cause the popup to
  // appear.
  std::vector<std::u16string> data_list_items;
  data_list_items.emplace_back();

  EXPECT_CALL(autofill_client_, UpdateAutofillPopupDataListValues(
                                    data_list_items, data_list_items));

  external_delegate_->SetCurrentDataListValues(data_list_items,
                                               data_list_items);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids =
      testing::ElementsAre(static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY),
#if !BUILDFLAG(IS_ANDROID)
                           static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
                           kAutofillProfileId,
#if !BUILDFLAG(IS_ANDROID)
                           static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
                           static_cast<int>(POPUP_ITEM_ID_AUTOFILL_OPTIONS));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // Ensure the popup is displayed.
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].frontend_id = kAutofillProfileId;
  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            AutoselectFirstSuggestion(false));
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIdsAre(element_ids));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);

  // This would normally get called from ShowAutofillPopup, but it is mocked so
  // we need to call OnPopupShown ourselves.
  external_delegate_->OnPopupShown();

  // Update the current data list and ensure the popup is updated.
  data_list_items.emplace_back();

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_CALL(autofill_client_, UpdateAutofillPopupDataListValues(
                                    data_list_items, data_list_items));

  external_delegate_->SetCurrentDataListValues(data_list_items,
                                               data_list_items);
}

// Test that we _don't_ de-dupe autofill values against datalist values. We
// keep both with a separator.
TEST_F(AutofillExternalDelegateUnitTest, DuplicateAutofillDatalistValues) {
  IssueOnQuery();

  std::vector<std::u16string> data_list_values{u"Rick", u"Beyonce"};
  std::vector<std::u16string> data_list_labels{u"Deckard", u"Knowles"};

  EXPECT_CALL(autofill_client_, UpdateAutofillPopupDataListValues(
                                    data_list_values, data_list_labels));

  external_delegate_->SetCurrentDataListValues(data_list_values,
                                               data_list_labels);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids =
      testing::ElementsAre(static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY),
                           static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY),
#if !BUILDFLAG(IS_ANDROID)
                           static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
                           kAutofillProfileId,
#if !BUILDFLAG(IS_ANDROID)
                           static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
                           static_cast<int>(POPUP_ITEM_ID_AUTOFILL_OPTIONS));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // Have an Autofill item that is identical to one of the datalist entries.
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].main_text =
      Suggestion::Text(u"Rick", Suggestion::Text::IsPrimary(true));
  autofill_item[0].labels = {{Suggestion::Text(u"Deckard")}};
  autofill_item[0].frontend_id = kAutofillProfileId;
  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            AutoselectFirstSuggestion(false));
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIdsAre(element_ids));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
}

// Test that we de-dupe autocomplete values against datalist values, keeping the
// latter in case of a match.
TEST_F(AutofillExternalDelegateUnitTest, DuplicateAutocompleteDatalistValues) {
  IssueOnQuery();

  std::vector<std::u16string> data_list_values{u"Rick", u"Beyonce"};
  std::vector<std::u16string> data_list_labels{u"Deckard", u"Knowles"};

  EXPECT_CALL(autofill_client_, UpdateAutofillPopupDataListValues(
                                    data_list_values, data_list_labels));

  external_delegate_->SetCurrentDataListValues(data_list_values,
                                               data_list_labels);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids = testing::ElementsAre(
      // We are expecting only two data list entries.
      static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY),
      static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY),
#if !BUILDFLAG(IS_ANDROID)
      static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
      static_cast<int>(POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // Have an Autocomplete item that is identical to one of the datalist entries
  // and one that is distinct.
  std::vector<Suggestion> autocomplete_items;
  autocomplete_items.emplace_back();
  autocomplete_items[0].main_text =
      Suggestion::Text(u"Rick", Suggestion::Text::IsPrimary(true));
  autocomplete_items[0].frontend_id = POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY;
  autocomplete_items.emplace_back();
  autocomplete_items[1].main_text =
      Suggestion::Text(u"Cain", Suggestion::Text::IsPrimary(true));
  autocomplete_items[1].frontend_id = POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY;
  external_delegate_->OnSuggestionsReturned(field_id_, autocomplete_items,
                                            AutoselectFirstSuggestion(false));
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIdsAre(element_ids));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
}

// Test that the Autofill popup is able to display warnings explaining why
// Autofill is disabled for a website.
// Regression test for http://crbug.com/247880
TEST_F(AutofillExternalDelegateUnitTest, AutofillWarnings) {
  IssueOnQuery();

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].frontend_id =
      POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE;
  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            AutoselectFirstSuggestion(false));

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(testing::ElementsAre(static_cast<int>(
                  POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE))));
  EXPECT_EQ(open_args.element_bounds, gfx::RectF());
  EXPECT_EQ(open_args.text_direction, base::i18n::UNKNOWN_DIRECTION);
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
}

// Test that Autofill warnings are removed if there are also autocomplete
// entries in the vector.
TEST_F(AutofillExternalDelegateUnitTest,
       AutofillWarningsNotShown_WithSuggestions) {
  IssueOnQuery();

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back();
  suggestions[0].frontend_id =
      POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE;
  suggestions.emplace_back();
  suggestions[1].main_text =
      Suggestion::Text(u"Rick", Suggestion::Text::IsPrimary(true));
  suggestions[1].frontend_id = POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY;
  external_delegate_->OnSuggestionsReturned(field_id_, suggestions,
                                            AutoselectFirstSuggestion(false));

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(testing::ElementsAre(
                  static_cast<int>(POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY))));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
}

// Test that the Autofill delegate doesn't try and fill a form with a
// negative unique id.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateInvalidUniqueId) {
  // Ensure it doesn't try to preview the negative id.
  EXPECT_CALL(*browser_autofill_manager_, FillOrPreviewForm(_, _, _, _))
      .Times(0);
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm()).Times(1);
  external_delegate_->DidSelectSuggestion(std::u16string(), -1,
                                          Suggestion::BackendId());

  // Ensure it doesn't try to fill the form in with the negative id.
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  EXPECT_CALL(*browser_autofill_manager_, FillOrPreviewForm(_, _, _, _))
      .Times(0);

  external_delegate_->DidAcceptSuggestion(Suggestion(-1), 0);
}

// Test that the Autofill delegate still allows previewing and filling
// specifically of the negative ID for POPUP_ITEM_ID_IBAN_ENTRY.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateFillsIbanEntry) {
  IssueOnQuery();

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back();
  std::u16string iban_value = u"IE12BOFI90000112345678";
  suggestions[0].main_text.value = iban_value;
  suggestions[0].labels = {{Suggestion::Text(u"My doctor's IBAN")}};
  suggestions[0].frontend_id = POPUP_ITEM_ID_IBAN_ENTRY;
  external_delegate_->OnSuggestionsReturned(field_id_, suggestions,
                                            AutoselectFirstSuggestion(false));

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(testing::ElementsAre(
                  static_cast<int>(POPUP_ITEM_ID_IBAN_ENTRY))));

  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm()).Times(1);
  EXPECT_CALL(*autofill_driver_,
              RendererShouldPreviewFieldWithValue(field_id_, iban_value));
  external_delegate_->DidSelectSuggestion(iban_value, POPUP_ITEM_ID_IBAN_ENTRY,
                                          Suggestion::BackendId());
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  EXPECT_CALL(*autofill_driver_,
              RendererShouldFillFieldWithValue(field_id_, iban_value));
  external_delegate_->DidAcceptSuggestion(suggestions[0], 0);
}

// Test that the Autofill delegate still allows previewing and filling
// specifically of the negative ID for POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateFillsMerchantPromoCodeEntry) {
  IssueOnQuery();

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back();
  std::u16string promo_code_value = u"PROMOCODE1234";
  suggestions[0].main_text.value = promo_code_value;
  suggestions[0].labels = {{Suggestion::Text(u"12.34% off your purchase!")}};
  suggestions[0].frontend_id = POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY;
  external_delegate_->OnSuggestionsReturned(field_id_, suggestions,
                                            AutoselectFirstSuggestion(false));

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(testing::ElementsAre(
                  static_cast<int>(POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY))));

  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm()).Times(1);
  EXPECT_CALL(*autofill_driver_,
              RendererShouldPreviewFieldWithValue(field_id_, promo_code_value));
  external_delegate_->DidSelectSuggestion(
      promo_code_value, POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY,
      Suggestion::BackendId());
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  EXPECT_CALL(*autofill_driver_,
              RendererShouldFillFieldWithValue(field_id_, promo_code_value));

  external_delegate_->DidAcceptSuggestion(
      test::CreateAutofillSuggestion(POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY,
                                     promo_code_value),
      0);
}

// Test that the Autofill delegate routes the merchant promo code suggestions
// footer redirect logic correctly.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateMerchantPromoCodeSuggestionsFooter) {
  const GURL gurl{"https://example.com/"};
  EXPECT_CALL(autofill_client_, OpenPromoCodeOfferDetailsURL(gurl));

  external_delegate_->DidAcceptSuggestion(
      test::CreateAutofillSuggestion(POPUP_ITEM_ID_SEE_PROMO_CODE_DETAILS,
                                     u"baz foo", gurl),
      0);
}

// Test that the ClearPreview call is only sent if the form was being previewed
// (i.e. it isn't autofilling a password).
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateClearPreviewedForm) {
  // Ensure selecting a new password entries or Autofill entries will
  // cause any previews to get cleared.
  IssueOnQuery();
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm()).Times(1);
  external_delegate_->DidSelectSuggestion(
      u"baz foo", POPUP_ITEM_ID_PASSWORD_ENTRY, Suggestion::BackendId());
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm()).Times(1);
  EXPECT_CALL(
      *browser_autofill_manager_,
      FillOrPreviewForm(mojom::RendererFormDataAction::kPreview, _, _, _));
  external_delegate_->DidSelectSuggestion(u"baz foo", 1,
                                          Suggestion::BackendId());

  // Ensure selecting an autocomplete entry will cause any previews to
  // get cleared.
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm()).Times(1);
  EXPECT_CALL(*autofill_driver_, RendererShouldPreviewFieldWithValue(
                                     field_id_, std::u16string(u"baz foo")));
  external_delegate_->DidSelectSuggestion(
      u"baz foo", POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY, Suggestion::BackendId());

  // Ensure selecting a virtual card entry will cause any previews to
  // get cleared.
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm()).Times(1);
  EXPECT_CALL(*browser_autofill_manager_,
              FillOrPreviewVirtualCardInformation(
                  mojom::RendererFormDataAction::kPreview, _, _, _));
  external_delegate_->DidSelectSuggestion(
      std::u16string(), POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY,
      Suggestion::BackendId());
}

// Test that the popup is hidden once we are done editing the autofill field.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateHidePopupAfterEditing) {
  EXPECT_CALL(autofill_client_, ShowAutofillPopup);
  test::GenerateTestAutofillPopup(external_delegate_.get());

  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(autofill::PopupHidingReason::kEndEditing));
  external_delegate_->DidEndTextFieldEditing();
}

// Test that the driver is directed to accept the data list after being notified
// that the user accepted the data list suggestion.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateAcceptDatalistSuggestion) {
  IssueOnQuery();
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  std::u16string dummy_string(u"baz qux");
  EXPECT_CALL(*autofill_driver_,
              RendererShouldAcceptDataListSuggestion(field_id_, dummy_string));

  external_delegate_->DidAcceptSuggestion(
      test::CreateAutofillSuggestion(POPUP_ITEM_ID_DATALIST_ENTRY,
                                     dummy_string),
      0);
}

// Test that an accepted autofill suggestion will fill the form.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateAcceptAutofillSuggestion) {
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  std::u16string dummy_string(u"John Legend");
  EXPECT_CALL(*browser_autofill_manager_,
              FillOrPreviewForm(mojom::RendererFormDataAction::kFill, _, _,
                                kAutofillProfileId));

  external_delegate_->DidAcceptSuggestion(
      test::CreateAutofillSuggestion(kAutofillProfileId, dummy_string),
      2);  // Row 2
}

// Test that the driver is directed to clear the form after being notified that
// the user accepted the suggestion to clear the form.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateClearForm) {
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  EXPECT_CALL(*autofill_driver_, RendererShouldClearFilledSection());

  external_delegate_->DidAcceptSuggestion(Suggestion(POPUP_ITEM_ID_CLEAR_FORM),
                                          0);
}

// Test that autofill client will scan a credit card after use accepted the
// suggestion to scan a credit card.
TEST_F(AutofillExternalDelegateUnitTest, ScanCreditCardMenuItem) {
  EXPECT_CALL(autofill_client_, ScanCreditCard(_));
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));

  external_delegate_->DidAcceptSuggestion(
      Suggestion(POPUP_ITEM_ID_SCAN_CREDIT_CARD), 0);
}

TEST_F(AutofillExternalDelegateUnitTest, ScanCreditCardPromptMetricsTest) {
  // Log that the scan card item was shown, although nothing was selected.
  {
    EXPECT_CALL(*browser_autofill_manager_, ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(true));
    base::HistogramTester histogram;
    IssueOnQuery();
    IssueOnSuggestionsReturned(field_id_);
    external_delegate_->OnPopupShown();
    histogram.ExpectUniqueSample("Autofill.ScanCreditCardPrompt",
                                 AutofillMetrics::SCAN_CARD_ITEM_SHOWN, 1);
  }
  // Log that the scan card item was selected.
  {
    EXPECT_CALL(*browser_autofill_manager_, ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(true));
    base::HistogramTester histogram;
    IssueOnQuery();
    IssueOnSuggestionsReturned(field_id_);
    external_delegate_->OnPopupShown();

    external_delegate_->DidAcceptSuggestion(
        Suggestion(POPUP_ITEM_ID_SCAN_CREDIT_CARD), 0);

    histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                                AutofillMetrics::SCAN_CARD_ITEM_SHOWN, 1);
    histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                                AutofillMetrics::SCAN_CARD_ITEM_SELECTED, 1);
    histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                                AutofillMetrics::SCAN_CARD_OTHER_ITEM_SELECTED,
                                0);
  }
  // Log that something else was selected.
  {
    EXPECT_CALL(*browser_autofill_manager_, ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(true));
    base::HistogramTester histogram;
    IssueOnQuery();
    IssueOnSuggestionsReturned(field_id_);
    external_delegate_->OnPopupShown();

    external_delegate_->DidAcceptSuggestion(
        Suggestion(POPUP_ITEM_ID_CLEAR_FORM), 0);

    histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                                AutofillMetrics::SCAN_CARD_ITEM_SHOWN, 1);
    histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                                AutofillMetrics::SCAN_CARD_ITEM_SELECTED, 0);
    histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                                AutofillMetrics::SCAN_CARD_OTHER_ITEM_SELECTED,
                                1);
  }
  // Nothing is logged when the item isn't shown.
  {
    EXPECT_CALL(*browser_autofill_manager_, ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(false));
    base::HistogramTester histogram;
    IssueOnQuery();
    IssueOnSuggestionsReturned(field_id_);
    external_delegate_->OnPopupShown();
    histogram.ExpectTotalCount("Autofill.ScanCreditCardPrompt", 0);
  }
}

// Test that autofill client will start the signin flow after the user accepted
// the suggestion to sign in.
TEST_F(AutofillExternalDelegateUnitTest, SigninPromoMenuItem) {
  EXPECT_CALL(autofill_client_,
              ExecuteCommand(autofill::POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO));
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));

  external_delegate_->DidAcceptSuggestion(
      Suggestion(POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO), 0);
}

MATCHER_P(CreditCardMatches, card, "") {
  return !arg.Compare(card);
}

// Test that autofill manager will fill the credit card form after user scans a
// credit card.
TEST_F(AutofillExternalDelegateUnitTest, FillCreditCardFormImpl) {
  CreditCard card;
  test::SetCreditCardInfo(&card, "Alice", "4111", "1", "3000", "1");
  EXPECT_CALL(
      *browser_autofill_manager_,
      FillCreditCardFormImpl(_, _, CreditCardMatches(card), std::u16string()));
  external_delegate_->OnCreditCardScanned(card);
}

TEST_F(AutofillExternalDelegateUnitTest, IgnoreAutocompleteOffForAutofill) {
  const FormData form;
  FormFieldData field;
  field.is_focusable = true;
  field.should_autocomplete = false;

  external_delegate_->OnQuery(form, field, gfx::RectF());

  std::vector<Suggestion> autofill_items;
  autofill_items.emplace_back();
  autofill_items[0].frontend_id = POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY;

  // Ensure the popup tries to show itself, despite autocomplete="off".
  EXPECT_CALL(autofill_client_, ShowAutofillPopup);
  EXPECT_CALL(autofill_client_, HideAutofillPopup(_)).Times(0);

  external_delegate_->OnSuggestionsReturned(field.global_id(), autofill_items,
                                            AutoselectFirstSuggestion(false));
}

TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateFillFieldWithValue) {
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion))
      .Times(3);
  IssueOnQuery();
  std::u16string dummy_string(u"baz foo");
  EXPECT_CALL(*autofill_driver_,
              RendererShouldFillFieldWithValue(field_id_, dummy_string));
  EXPECT_CALL(*autofill_client_.GetMockAutocompleteHistoryManager(),
              OnSingleFieldSuggestionSelected(dummy_string,
                                              POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY))
      .Times(1);
  base::HistogramTester histogram_tester;

  external_delegate_->DidAcceptSuggestion(
      test::CreateAutofillSuggestion(POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY,
                                     dummy_string),
      0);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SuggestionAcceptedIndex.Autocomplete", 0, 1);

  // Test that merchant promo code offers get autofilled.
  EXPECT_CALL(*autofill_driver_,
              RendererShouldFillFieldWithValue(field_id_, dummy_string));
  EXPECT_CALL(*autofill_client_.GetMockMerchantPromoCodeManager(),
              OnSingleFieldSuggestionSelected(
                  dummy_string, POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY))
      .Times(1);
  external_delegate_->DidAcceptSuggestion(
      test::CreateAutofillSuggestion(POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY,
                                     dummy_string),
      0);

  // Test that IBANs get autofilled.
  EXPECT_CALL(*autofill_driver_,
              RendererShouldFillFieldWithValue(field_id_, dummy_string));
  EXPECT_CALL(
      *autofill_client_.GetMockIBANManager(),
      OnSingleFieldSuggestionSelected(dummy_string, POPUP_ITEM_ID_IBAN_ENTRY))
      .Times(1);
  external_delegate_->DidAcceptSuggestion(
      test::CreateAutofillSuggestion(POPUP_ITEM_ID_IBAN_ENTRY, dummy_string),
      0);
}

TEST_F(AutofillExternalDelegateUnitTest, ShouldShowGooglePayIcon) {
  IssueOnQuery();

  auto element_icons = testing::ElementsAre(std::string(),
#if !BUILDFLAG(IS_ANDROID)
                                            std::string(),
#endif
                                            testing::StartsWith("googlePay"));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].frontend_id = kAutofillProfileId;

  // This should call ShowAutofillPopup.
  external_delegate_->OnSuggestionsReturned(
      field_id_, autofill_item, AutoselectFirstSuggestion(false), true);

  // On Desktop, the GPay icon should be stored in the store indicator icon.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIconsAre(element_icons));
#else
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorStoreIndicatorIconsAre(element_icons));
#endif
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ShouldNotShowGooglePayIconIfSuggestionsContainLocalCards) {
  IssueOnQuery();

  auto element_icons = testing::ElementsAre(std::string(),
#if !BUILDFLAG(IS_ANDROID)
                                            std::string(),
#endif
                                            "settingsIcon");
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].frontend_id = kAutofillProfileId;

  // This should call ShowAutofillPopup.
  external_delegate_->OnSuggestionsReturned(
      field_id_, autofill_item, AutoselectFirstSuggestion(false), false);
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIconsAre(element_icons));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
}

TEST_F(AutofillExternalDelegateUnitTest, ShouldUseNewSettingName) {
  IssueOnQuery();

  auto element_main_texts = testing::ElementsAre(
      Suggestion::Text(std::u16string(), Suggestion::Text::IsPrimary(true)),
#if !BUILDFLAG(IS_ANDROID)
      Suggestion::Text(std::u16string(), Suggestion::Text::IsPrimary(false)),
#endif
      Suggestion::Text(l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE),
                       Suggestion::Text::IsPrimary(true)));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].frontend_id = kAutofillProfileId;
  autofill_item[0].main_text.is_primary = Suggestion::Text::IsPrimary(true);

  // This should call ShowAutofillPopup.
  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            AutoselectFirstSuggestion(false));
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorMainTextsAre(element_main_texts));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
}

// Test that browser autofill manager will handle the unmasking request for the
// virtual card after users accept the suggestion to use a virtual card.
TEST_F(AutofillExternalDelegateUnitTest, AcceptVirtualCardOptionItem) {
  FormData form;
  EXPECT_CALL(*browser_autofill_manager_,
              FillOrPreviewVirtualCardInformation(
                  mojom::RendererFormDataAction::kFill, _, _, _));
  external_delegate_->DidAcceptSuggestion(
      Suggestion(POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY), 0);
}

TEST_F(AutofillExternalDelegateUnitTest, SelectVirtualCardOptionItem) {
  EXPECT_CALL(*browser_autofill_manager_,
              FillOrPreviewVirtualCardInformation(
                  mojom::RendererFormDataAction::kPreview, _, _, _));
  external_delegate_->DidSelectSuggestion(
      std::u16string(), POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY,
      Suggestion::BackendId());
}

// Tests that the prompt to show account cards shows up when the corresponding
// bit is set, including any suggestions that are passed along and the "Manage"
// row in the footer.
TEST_F(AutofillExternalDelegateCardsFromAccountTest,
       ShouldShowCardsFromAccountOptionWithCards) {
  IssueOnQuery();

  auto element_main_texts = testing::ElementsAre(
      Suggestion::Text(std::u16string(), Suggestion::Text::IsPrimary(true)),
      Suggestion::Text(
          l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ACCOUNT_CARDS),
          Suggestion::Text::IsPrimary(true)),
#if !BUILDFLAG(IS_ANDROID)
      Suggestion::Text(std::u16string(), Suggestion::Text::IsPrimary(false)),
#endif
      Suggestion::Text(l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE),
                       Suggestion::Text::IsPrimary(true)));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].frontend_id = kAutofillProfileId;
  autofill_item[0].main_text.is_primary = Suggestion::Text::IsPrimary(true);

  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            AutoselectFirstSuggestion(false));
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorMainTextsAre(element_main_texts));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
}

// Tests that the prompt to show account cards shows up when the corresponding
// bit is set, even if no suggestions are passed along. The "Manage" row should
// *not* show up in this case.
TEST_F(AutofillExternalDelegateCardsFromAccountTest,
       ShouldShowCardsFromAccountOptionWithoutCards) {
  IssueOnQuery();

  auto element_main_texts = testing::ElementsAre(Suggestion::Text(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ACCOUNT_CARDS),
      Suggestion::Text::IsPrimary(true)));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  external_delegate_->OnSuggestionsReturned(
      field_id_, std::vector<Suggestion>(), AutoselectFirstSuggestion(false));
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorMainTextsAre(element_main_texts));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
}

#if BUILDFLAG(IS_IOS)
// Tests that outdated returned suggestions are discarded.
TEST_F(AutofillExternalDelegateCardsFromAccountTest,
       ShouldDiscardOutdatedSuggestions) {
  FieldGlobalId old_field_id = test::MakeFieldGlobalId();
  FieldGlobalId new_field_id = test::MakeFieldGlobalId();
  autofill_client_.set_last_queried_field(new_field_id);
  IssueOnQuery();
  EXPECT_CALL(autofill_client_, ShowAutofillPopup).Times(0);
  external_delegate_->OnSuggestionsReturned(old_field_id,
                                            std::vector<Suggestion>(),
                                            AutoselectFirstSuggestion(false));
}
#endif

}  // namespace autofill
