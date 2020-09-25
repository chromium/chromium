// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
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

using base::ASCIIToUTF16;
using testing::_;

namespace autofill {

namespace {

// A constant value to use as the Autofill query ID.
const int kRecentQueryId = 1;

// A constant value to use as an Autofill profile ID.
const int kAutofillProfileId = 1;

class MockAutofillDriver : public TestAutofillDriver {
 public:
  MockAutofillDriver() = default;
  // Mock methods to enable testability.
  MOCK_METHOD(void,
              RendererShouldAcceptDataListSuggestion,
              (const base::string16&),
              (override));
  MOCK_METHOD(void, RendererShouldClearFilledSection, (), (override));
  MOCK_METHOD(void, RendererShouldClearPreviewedForm, (), (override));
  MOCK_METHOD(void,
              RendererShouldFillFieldWithValue,
              (const base::string16&),
              (override));
  MOCK_METHOD(void,
              RendererShouldPreviewFieldWithValue,
              (const base::string16&),
              (override));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillDriver);
};

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
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
              (const std::vector<base::string16>& values,
               const std::vector<base::string16>& lables),
              (override));
  MOCK_METHOD(void, HideAutofillPopup, (PopupHidingReason), (override));
  MOCK_METHOD(void, ExecuteCommand, (int), (override));

  // Mock the client query ID check.
  bool IsQueryIDRelevant(int query_id) { return query_id == kRecentQueryId; }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillClient);
};

class MockAutofillManager : public AutofillManager {
 public:
  MockAutofillManager(AutofillDriver* driver, MockAutofillClient* client)
      // Force to use the constructor designated for unit test.
      : AutofillManager(driver,
                        client,
                        client->GetPersonalDataManager(),
                        client->GetAutocompleteHistoryManager()) {}

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

  bool ShouldShowCardsFromAccountOption(const FormData& form,
                                        const FormFieldData& field) {
    return should_show_cards_from_account_option_;
  }

  void ShowCardsFromAccountOption() {
    should_show_cards_from_account_option_ = true;
  }

  MOCK_METHOD(void,
              FillOrPreviewForm,
              (AutofillDriver::RendererFormDataAction action,
               int query_id,
               const FormData& form,
               const FormFieldData& field,
               int unique_id),
              (override));
  MOCK_METHOD(void,
              FillCreditCardForm,
              (int query_id,
               const FormData& form,
               const FormFieldData& field,
               const CreditCard& credit_card,
               const base::string16& cvc),
              (override));

 private:
  bool should_show_cards_from_account_option_ = false;
  DISALLOW_COPY_AND_ASSIGN(MockAutofillManager);
};

}  // namespace

class AutofillExternalDelegateUnitTest : public testing::Test {
 protected:
  void SetUp() override {
    autofill_driver_ =
        std::make_unique<testing::NiceMock<MockAutofillDriver>>();
    autofill_manager_ = std::make_unique<MockAutofillManager>(
        autofill_driver_.get(), &autofill_client_);
    external_delegate_ = std::make_unique<AutofillExternalDelegate>(
        autofill_manager_.get(), autofill_driver_.get());
  }

  void TearDown() override {
    // Order of destruction is important as AutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    autofill_manager_.reset();
    external_delegate_.reset();
    autofill_driver_.reset();
  }

  // Issue an OnQuery call with the given |query_id|.
  void IssueOnQuery(int query_id) {
    const FormData form;
    FormFieldData field;
    field.is_focusable = true;
    field.should_autocomplete = true;

    external_delegate_->OnQuery(query_id, form, field, gfx::RectF());
  }

  void IssueOnSuggestionsReturned(int query_id) {
    std::vector<Suggestion> suggestions;
    suggestions.push_back(Suggestion());
    suggestions[0].frontend_id = kAutofillProfileId;
    external_delegate_->OnSuggestionsReturned(
        query_id, suggestions, /*autoselect_first_suggestion=*/false);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  testing::NiceMock<MockAutofillClient> autofill_client_;
  std::unique_ptr<testing::NiceMock<MockAutofillDriver>> autofill_driver_;
  std::unique_ptr<MockAutofillManager> autofill_manager_;
  std::unique_ptr<AutofillExternalDelegate> external_delegate_;
};

// Variant for use in cases when we expect the AutofillManager would normally
// set the |should_show_cards_from_account_option_| bit.
class AutofillExternalDelegateCardsFromAccountTest
    : public AutofillExternalDelegateUnitTest {
 protected:
  void SetUp() override {
    AutofillExternalDelegateUnitTest::SetUp();
    autofill_manager_->ShowCardsFromAccountOption();
  }
};

// Test that our external delegate called the virtual methods at the right time.
TEST_F(AutofillExternalDelegateUnitTest, TestExternalDelegateVirtualCalls) {
  IssueOnQuery(kRecentQueryId);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids = testing::ElementsAre(
      kAutofillProfileId, static_cast<int>(POPUP_ITEM_ID_AUTOFILL_OPTIONS));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> autofill_item;
  autofill_item.push_back(Suggestion());
  autofill_item[0].frontend_id = kAutofillProfileId;
  external_delegate_->OnSuggestionsReturned(
      kRecentQueryId, autofill_item, /*autoselect_first_suggestion=*/false);
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIdsAre(element_ids));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPersonalInformation);

  EXPECT_CALL(
      *autofill_manager_,
      FillOrPreviewForm(AutofillDriver::FORM_DATA_ACTION_FILL, _, _, _, _));
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));

  // This should trigger a call to hide the popup since we've selected an
  // option.
  external_delegate_->DidAcceptSuggestion(autofill_item[0].value,
                                          autofill_item[0].frontend_id, 0);
}

// Test that our external delegate does not add the signin promo and its
// separator in the popup items when there are suggestions.
TEST_F(AutofillExternalDelegateUnitTest,
       TestSigninPromoIsNotAdded_WithSuggestions) {
  EXPECT_CALL(*autofill_manager_, ShouldShowCreditCardSigninPromo(_, _))
      .WillOnce(testing::Return(true));

  IssueOnQuery(kRecentQueryId);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids = testing::ElementsAre(
      kAutofillProfileId, static_cast<int>(POPUP_ITEM_ID_AUTOFILL_OPTIONS));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  base::UserActionTester user_action_tester;

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> autofill_item;
  autofill_item.push_back(Suggestion());
  autofill_item[0].frontend_id = kAutofillProfileId;
  external_delegate_->OnSuggestionsReturned(
      kRecentQueryId, autofill_item, /*autoselect_first_suggestion=*/false);
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Signin_Impression_FromAutofillDropdown"));
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIdsAre(element_ids));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPersonalInformation);

  EXPECT_CALL(
      *autofill_manager_,
      FillOrPreviewForm(AutofillDriver::FORM_DATA_ACTION_FILL, _, _, _, _));
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));

  // This should trigger a call to hide the popup since we've selected an
  // option.
  external_delegate_->DidAcceptSuggestion(autofill_item[0].value,
                                          autofill_item[0].frontend_id, 0);
}

// Test that our external delegate properly adds the signin promo and no
// separator in the dropdown, when there are no suggestions.
TEST_F(AutofillExternalDelegateUnitTest,
       TestSigninPromoIsAdded_WithNoSuggestions) {
  EXPECT_CALL(*autofill_manager_, ShouldShowCreditCardSigninPromo(_, _))
      .WillOnce(testing::Return(true));

  IssueOnQuery(kRecentQueryId);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids = testing::ElementsAre(
      static_cast<int>(POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO));

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  base::UserActionTester user_action_tester;

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> items;
  external_delegate_->OnSuggestionsReturned(
      kRecentQueryId, items, /*autoselect_first_suggestion=*/false);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Impression_FromAutofillDropdown"));
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIdsAre(element_ids));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPersonalInformation);

  EXPECT_CALL(autofill_client_,
              ExecuteCommand(autofill::POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO));
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));

  // This should trigger a call to start the signin flow and hide the popup
  // since we've selected the sign-in promo option.
  external_delegate_->DidAcceptSuggestion(
      base::string16(), POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO, 0);
}

// Test that data list elements for a node will appear in the Autofill popup.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateDataList) {
  IssueOnQuery(kRecentQueryId);

  std::vector<base::string16> data_list_items;
  data_list_items.push_back(base::string16());

  EXPECT_CALL(autofill_client_, UpdateAutofillPopupDataListValues(
                                    data_list_items, data_list_items));

  external_delegate_->SetCurrentDataListValues(data_list_items,
                                               data_list_items);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids = testing::ElementsAre(
      static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY),
#if !defined(OS_ANDROID)
      static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
      kAutofillProfileId, static_cast<int>(POPUP_ITEM_ID_AUTOFILL_OPTIONS));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> autofill_item;
  autofill_item.push_back(Suggestion());
  autofill_item[0].frontend_id = kAutofillProfileId;
  external_delegate_->OnSuggestionsReturned(
      kRecentQueryId, autofill_item, /*autoselect_first_suggestion=*/false);
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIdsAre(element_ids));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPersonalInformation);

  // Try calling OnSuggestionsReturned with no Autofill values and ensure
  // the datalist items are still shown.
  // The enum must be cast to an int to prevent compile errors on linux_rel.

  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  autofill_item.clear();
  external_delegate_->OnSuggestionsReturned(
      kRecentQueryId, autofill_item, /*autoselect_first_suggestion=*/false);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(testing::ElementsAre(
                  static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY))));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPersonalInformation);
}

// Test that datalist values can get updated while a popup is showing.
TEST_F(AutofillExternalDelegateUnitTest, UpdateDataListWhileShowingPopup) {
  IssueOnQuery(kRecentQueryId);

  EXPECT_CALL(autofill_client_, ShowAutofillPopup).Times(0);

  // Make sure just setting the data list values doesn't cause the popup to
  // appear.
  std::vector<base::string16> data_list_items;
  data_list_items.push_back(base::string16());

  EXPECT_CALL(autofill_client_, UpdateAutofillPopupDataListValues(
                                    data_list_items, data_list_items));

  external_delegate_->SetCurrentDataListValues(data_list_items,
                                               data_list_items);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids = testing::ElementsAre(
      static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY),
#if !defined(OS_ANDROID)
      static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
      kAutofillProfileId, static_cast<int>(POPUP_ITEM_ID_AUTOFILL_OPTIONS));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // Ensure the popup is displayed.
  std::vector<Suggestion> autofill_item;
  autofill_item.push_back(Suggestion());
  autofill_item[0].frontend_id = kAutofillProfileId;
  external_delegate_->OnSuggestionsReturned(
      kRecentQueryId, autofill_item, /*autoselect_first_suggestion=*/false);
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIdsAre(element_ids));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPersonalInformation);

  // This would normally get called from ShowAutofillPopup, but it is mocked so
  // we need to call OnPopupShown ourselves.
  external_delegate_->OnPopupShown();

  // Update the current data list and ensure the popup is updated.
  data_list_items.push_back(base::string16());

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_CALL(autofill_client_, UpdateAutofillPopupDataListValues(
                                    data_list_items, data_list_items));

  external_delegate_->SetCurrentDataListValues(data_list_items,
                                               data_list_items);
}

// Test that we _don't_ de-dupe autofill values against datalist values. We
// keep both with a separator.
TEST_F(AutofillExternalDelegateUnitTest, DuplicateAutofillDatalistValues) {
  IssueOnQuery(kRecentQueryId);

  std::vector<base::string16> data_list_values{base::ASCIIToUTF16("Rick"),
                                               base::ASCIIToUTF16("Beyonce")};
  std::vector<base::string16> data_list_labels{base::ASCIIToUTF16("Deckard"),
                                               base::ASCIIToUTF16("Knowles")};

  EXPECT_CALL(autofill_client_, UpdateAutofillPopupDataListValues(
                                    data_list_values, data_list_labels));

  external_delegate_->SetCurrentDataListValues(data_list_values,
                                               data_list_labels);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids = testing::ElementsAre(
      static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY),
      static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY),
#if !defined(OS_ANDROID)
      static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
      kAutofillProfileId, static_cast<int>(POPUP_ITEM_ID_AUTOFILL_OPTIONS));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // Have an Autofill item that is identical to one of the datalist entries.
  std::vector<Suggestion> autofill_item;
  autofill_item.push_back(Suggestion());
  autofill_item[0].value = ASCIIToUTF16("Rick");
  autofill_item[0].label = ASCIIToUTF16("Deckard");
  autofill_item[0].frontend_id = kAutofillProfileId;
  external_delegate_->OnSuggestionsReturned(
      kRecentQueryId, autofill_item, /*autoselect_first_suggestion=*/false);
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIdsAre(element_ids));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPersonalInformation);
}

// Test that we de-dupe autocomplete values against datalist values, keeping the
// latter in case of a match.
TEST_F(AutofillExternalDelegateUnitTest, DuplicateAutocompleteDatalistValues) {
  IssueOnQuery(kRecentQueryId);

  std::vector<base::string16> data_list_values{base::ASCIIToUTF16("Rick"),
                                               base::ASCIIToUTF16("Beyonce")};
  std::vector<base::string16> data_list_labels{base::ASCIIToUTF16("Deckard"),
                                               base::ASCIIToUTF16("Knowles")};

  EXPECT_CALL(autofill_client_, UpdateAutofillPopupDataListValues(
                                    data_list_values, data_list_labels));

  external_delegate_->SetCurrentDataListValues(data_list_values,
                                               data_list_labels);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids = testing::ElementsAre(
      // We are expecting only two data list entries.
      static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY),
      static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY),
#if !defined(OS_ANDROID)
      static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
      static_cast<int>(POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // Have an Autocomplete item that is identical to one of the datalist entries
  // and one that is distinct.
  std::vector<Suggestion> autocomplete_items;
  autocomplete_items.push_back(Suggestion());
  autocomplete_items[0].value = ASCIIToUTF16("Rick");
  autocomplete_items[0].frontend_id = POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY;
  autocomplete_items.push_back(Suggestion());
  autocomplete_items[1].value = ASCIIToUTF16("Cain");
  autocomplete_items[1].frontend_id = POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY;
  external_delegate_->OnSuggestionsReturned(
      kRecentQueryId, autocomplete_items,
      /*autoselect_first_suggestion=*/false);
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIdsAre(element_ids));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPersonalInformation);
}

// Test that the Autofill popup is able to display warnings explaining why
// Autofill is disabled for a website.
// Regression test for http://crbug.com/247880
TEST_F(AutofillExternalDelegateUnitTest, AutofillWarnings) {
  IssueOnQuery(kRecentQueryId);

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> autofill_item;
  autofill_item.push_back(Suggestion());
  autofill_item[0].frontend_id =
      POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE;
  external_delegate_->OnSuggestionsReturned(
      kRecentQueryId, autofill_item, /*autoselect_first_suggestion=*/false);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(testing::ElementsAre(static_cast<int>(
                  POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE))));
  EXPECT_EQ(open_args.element_bounds, gfx::RectF());
  EXPECT_EQ(open_args.text_direction, base::i18n::UNKNOWN_DIRECTION);
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPersonalInformation);
}

// Test that Autofill warnings are removed if there are also autocomplete
// entries in the vector.
TEST_F(AutofillExternalDelegateUnitTest,
       AutofillWarningsNotShown_WithSuggestions) {
  IssueOnQuery(kRecentQueryId);

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> suggestions;
  suggestions.push_back(Suggestion());
  suggestions[0].frontend_id =
      POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE;
  suggestions.push_back(Suggestion());
  suggestions[1].value = ASCIIToUTF16("Rick");
  suggestions[1].frontend_id = POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY;
  external_delegate_->OnSuggestionsReturned(
      kRecentQueryId, suggestions, /*autoselect_first_suggestion=*/false);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(testing::ElementsAre(
                  static_cast<int>(POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY))));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPersonalInformation);
}

// Test that the Autofill delegate doesn't try and fill a form with a
// negative unique id.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateInvalidUniqueId) {
  // Ensure it doesn't try to preview the negative id.
  EXPECT_CALL(*autofill_manager_, FillOrPreviewForm(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm()).Times(1);
  external_delegate_->DidSelectSuggestion(base::string16(), -1);

  // Ensure it doesn't try to fill the form in with the negative id.
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  EXPECT_CALL(*autofill_manager_, FillOrPreviewForm(_, _, _, _, _)).Times(0);
  external_delegate_->DidAcceptSuggestion(base::string16(), -1, 0);
}

// Test that the ClearPreview call is only sent if the form was being previewed
// (i.e. it isn't autofilling a password).
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateClearPreviewedForm) {
  // Ensure selecting a new password entries or Autofill entries will
  // cause any previews to get cleared.
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm()).Times(1);
  external_delegate_->DidSelectSuggestion(ASCIIToUTF16("baz foo"),
                                          POPUP_ITEM_ID_PASSWORD_ENTRY);
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm()).Times(1);
  EXPECT_CALL(
      *autofill_manager_,
      FillOrPreviewForm(AutofillDriver::FORM_DATA_ACTION_PREVIEW, _, _, _, _));
  external_delegate_->DidSelectSuggestion(ASCIIToUTF16("baz foo"), 1);

  // Ensure selecting an autocomplete entry will cause any previews to
  // get cleared.
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm()).Times(1);
  EXPECT_CALL(*autofill_driver_,
              RendererShouldPreviewFieldWithValue(ASCIIToUTF16("baz foo")));
  external_delegate_->DidSelectSuggestion(ASCIIToUTF16("baz foo"),
                                          POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY);
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
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  base::string16 dummy_string(ASCIIToUTF16("baz qux"));
  EXPECT_CALL(*autofill_driver_,
              RendererShouldAcceptDataListSuggestion(dummy_string));
  external_delegate_->DidAcceptSuggestion(dummy_string,
                                          POPUP_ITEM_ID_DATALIST_ENTRY, 0);
}

// Test that an accepted autofill suggestion will fill the form.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateAcceptAutofillSuggestion) {
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  base::string16 dummy_string(ASCIIToUTF16("John Legend"));
  EXPECT_CALL(*autofill_manager_,
              FillOrPreviewForm(AutofillDriver::FORM_DATA_ACTION_FILL, _, _, _,
                                kAutofillProfileId));
  external_delegate_->DidAcceptSuggestion(dummy_string, kAutofillProfileId,
                                          2);  // Row 2
}

// Test that the driver is directed to clear the form after being notified that
// the user accepted the suggestion to clear the form.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateClearForm) {
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  EXPECT_CALL(*autofill_driver_, RendererShouldClearFilledSection());

  external_delegate_->DidAcceptSuggestion(base::string16(),
                                          POPUP_ITEM_ID_CLEAR_FORM, 0);
}

// Test that the client is directed to hide the autofill popup after being
// notified that the user clicked "Hide suggestions" menu item.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateHideSuggestions) {
  EXPECT_CALL(*autofill_manager_, OnUserHideSuggestions(_, _));
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));

  external_delegate_->DidAcceptSuggestion(
      base::string16(), POPUP_ITEM_ID_HIDE_AUTOFILL_SUGGESTIONS, 0);
}

// Test that autofill client will scan a credit card after use accepted the
// suggestion to scan a credit card.
TEST_F(AutofillExternalDelegateUnitTest, ScanCreditCardMenuItem) {
  EXPECT_CALL(autofill_client_, ScanCreditCard(_));
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  external_delegate_->DidAcceptSuggestion(base::string16(),
                                          POPUP_ITEM_ID_SCAN_CREDIT_CARD, 0);
}

TEST_F(AutofillExternalDelegateUnitTest, ScanCreditCardPromptMetricsTest) {
  // Log that the scan card item was shown, although nothing was selected.
  {
    EXPECT_CALL(*autofill_manager_, ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(true));
    base::HistogramTester histogram;
    IssueOnQuery(kRecentQueryId);
    IssueOnSuggestionsReturned(kRecentQueryId);
    external_delegate_->OnPopupShown();
    histogram.ExpectUniqueSample("Autofill.ScanCreditCardPrompt",
                                 AutofillMetrics::SCAN_CARD_ITEM_SHOWN, 1);
  }
  // Log that the scan card item was selected.
  {
    EXPECT_CALL(*autofill_manager_, ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(true));
    base::HistogramTester histogram;
    IssueOnQuery(kRecentQueryId);
    IssueOnSuggestionsReturned(kRecentQueryId);
    external_delegate_->OnPopupShown();
    external_delegate_->DidAcceptSuggestion(base::string16(),
                                            POPUP_ITEM_ID_SCAN_CREDIT_CARD, 0);
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
    EXPECT_CALL(*autofill_manager_, ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(true));
    base::HistogramTester histogram;
    IssueOnQuery(kRecentQueryId);
    IssueOnSuggestionsReturned(kRecentQueryId);
    external_delegate_->OnPopupShown();
    external_delegate_->DidAcceptSuggestion(base::string16(),
                                            POPUP_ITEM_ID_CLEAR_FORM, 0);
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
    EXPECT_CALL(*autofill_manager_, ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(false));
    base::HistogramTester histogram;
    IssueOnQuery(kRecentQueryId);
    IssueOnSuggestionsReturned(kRecentQueryId);
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
      base::string16(), POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO, 0);
}

MATCHER_P(CreditCardMatches, card, "") {
  return !arg.Compare(card);
}

// Test that autofill manager will fill the credit card form after user scans a
// credit card.
TEST_F(AutofillExternalDelegateUnitTest, FillCreditCardForm) {
  CreditCard card;
  test::SetCreditCardInfo(&card, "Alice", "4111", "1", "3000", "1");
  EXPECT_CALL(
      *autofill_manager_,
      FillCreditCardForm(_, _, _, CreditCardMatches(card), base::string16()));
  external_delegate_->OnCreditCardScanned(card);
}

TEST_F(AutofillExternalDelegateUnitTest, IgnoreAutocompleteOffForAutofill) {
  const FormData form;
  FormFieldData field;
  field.is_focusable = true;
  field.should_autocomplete = false;

  external_delegate_->OnQuery(kRecentQueryId, form, field, gfx::RectF());

  std::vector<Suggestion> autofill_items;
  autofill_items.push_back(Suggestion());
  autofill_items[0].frontend_id = POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY;

  // Ensure the popup tries to show itself, despite autocomplete="off".
  EXPECT_CALL(autofill_client_, ShowAutofillPopup);
  EXPECT_CALL(autofill_client_, HideAutofillPopup(_)).Times(0);

  external_delegate_->OnSuggestionsReturned(
      kRecentQueryId, autofill_items, /*autoselect_first_suggestion=*/false);
}

TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateFillFieldWithValue) {
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  base::string16 dummy_string(ASCIIToUTF16("baz foo"));
  EXPECT_CALL(*autofill_driver_,
              RendererShouldFillFieldWithValue(dummy_string));
  EXPECT_CALL(*autofill_client_.GetMockAutocompleteHistoryManager(),
              OnAutocompleteEntrySelected(dummy_string))
      .Times(1);
  base::HistogramTester histogram_tester;
  external_delegate_->DidAcceptSuggestion(dummy_string,
                                          POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY, 0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SuggestionAcceptedIndex.Autocomplete", 0, 1);
}

TEST_F(AutofillExternalDelegateUnitTest, ShouldShowGooglePayIcon) {
  IssueOnQuery(kRecentQueryId);

  auto element_icons =
      testing::ElementsAre(std::string(), testing::StartsWith("googlePay"));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  std::vector<Suggestion> autofill_item;
  autofill_item.push_back(Suggestion());
  autofill_item[0].frontend_id = kAutofillProfileId;

  // This should call ShowAutofillPopup.
  external_delegate_->OnSuggestionsReturned(
      kRecentQueryId, autofill_item, /*autoselect_first_suggestion=*/false,
      true);
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIconsAre(element_icons));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPersonalInformation);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ShouldNotShowGooglePayIconIfSuggestionsContainLocalCards) {
  IssueOnQuery(kRecentQueryId);

  auto element_icons = testing::ElementsAre(
      std::string(),
      std::string() /* Autofill setting item does not have icon. */);
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  std::vector<Suggestion> autofill_item;
  autofill_item.push_back(Suggestion());
  autofill_item[0].frontend_id = kAutofillProfileId;

  // This should call ShowAutofillPopup.
  external_delegate_->OnSuggestionsReturned(
      kRecentQueryId, autofill_item, /*autoselect_first_suggestion=*/false,
      false);
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIconsAre(element_icons));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPersonalInformation);
}

TEST_F(AutofillExternalDelegateUnitTest, ShouldUseNewSettingName) {
  IssueOnQuery(kRecentQueryId);

  auto element_values = testing::ElementsAre(
      base::string16(), l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  std::vector<Suggestion> autofill_item;
  autofill_item.push_back(Suggestion());
  autofill_item[0].frontend_id = kAutofillProfileId;

  // This should call ShowAutofillPopup.
  external_delegate_->OnSuggestionsReturned(
      kRecentQueryId, autofill_item, /*autoselect_first_suggestion=*/false);
  EXPECT_THAT(open_args.suggestions, SuggestionVectorValuesAre(element_values));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPersonalInformation);
}

// Tests that the prompt to show account cards shows up when the corresponding
// bit is set, including any suggestions that are passed along and the "Manage"
// row in the footer.
TEST_F(AutofillExternalDelegateCardsFromAccountTest,
       ShouldShowCardsFromAccountOptionWithCards) {
  IssueOnQuery(kRecentQueryId);

  auto element_values = testing::ElementsAre(
      base::string16(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ACCOUNT_CARDS),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  std::vector<Suggestion> autofill_item;
  autofill_item.push_back(Suggestion());
  autofill_item[0].frontend_id = kAutofillProfileId;

  external_delegate_->OnSuggestionsReturned(
      kRecentQueryId, autofill_item, /*autoselect_first_suggestion=*/false);
  EXPECT_THAT(open_args.suggestions, SuggestionVectorValuesAre(element_values));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPersonalInformation);
}

// Tests that the prompt to show account cards shows up when the corresponding
// bit is set, even if no suggestions are passed along. The "Manage" row should
// *not* show up in this case.
TEST_F(AutofillExternalDelegateCardsFromAccountTest,
       ShouldShowCardsFromAccountOptionWithoutCards) {
  IssueOnQuery(kRecentQueryId);

  auto element_values = testing::ElementsAre(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ACCOUNT_CARDS));
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  external_delegate_->OnSuggestionsReturned(
      kRecentQueryId, std::vector<Suggestion>(),
      /*autoselect_first_suggestion=*/false);
  EXPECT_THAT(open_args.suggestions, SuggestionVectorValuesAre(element_values));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPersonalInformation);
}

#if defined(OS_IOS)
// Tests that outdated returned suggestions are discarded.
TEST_F(AutofillExternalDelegateCardsFromAccountTest,
       ShouldDiscardOutdatedSuggestions) {
  int older_query_id = kRecentQueryId - 1;
  IssueOnQuery(older_query_id);

  EXPECT_CALL(autofill_client_, ShowAutofillPopup).Times(0);
  external_delegate_->OnSuggestionsReturned(
      older_query_id, std::vector<Suggestion>(),
      /*autoselect_first_suggestion=*/false);
}
#endif

}  // namespace autofill
