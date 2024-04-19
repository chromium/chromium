// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_controller_impl.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_test_base.h"
#include "chrome/browser/ui/autofill/test_autofill_keyboard_accessory_controller_autofill_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

using ::testing::_;
using ::testing::ElementsAre;

using AutofillKeyboardAccessoryControllerImplTest =
    AutofillSuggestionControllerTestBase<
        TestAutofillKeyboardAccessoryControllerAutofillClient<>>;

std::vector<Suggestion> CreateSuggestionsWithClearFormEntry(
    size_t clear_form_offset) {
  auto create_pw_suggestion = [](std::string_view password,
                                 std::string_view username,
                                 std::string_view origin) {
    Suggestion s(/*main_text=*/username, /*label=*/origin,
                 Suggestion::Icon::kNoIcon, PopupItemId::kPasswordEntry);
    s.additional_label = base::UTF8ToUTF16(password);
    return s;
  };
  std::vector<Suggestion> suggestions = {
      create_pw_suggestion("****************", "Alf", ""),
      create_pw_suggestion("****************", "Berta", "psl.origin.eg"),
      create_pw_suggestion("***", "Carl", "")};
  suggestions.emplace(suggestions.begin() + clear_form_offset, "Clear", "",
                      Suggestion::Icon::kNoIcon, PopupItemId::kClearForm);
  return suggestions;
}

}  // namespace

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_UnrelatedPopupItemId) {
  std::u16string title;
  std::u16string body;
  ShowSuggestions(
      manager(),
      {Suggestion(u"Entry", PopupItemId::kAddressFieldByFieldFilling)});

  EXPECT_FALSE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_InvalidUniqueId) {
  std::u16string title;
  std::u16string body;
  ShowSuggestions(manager(), {test::CreateAutofillSuggestion(
                                 PopupItemId::kAddressFieldByFieldFilling,
                                 u"Entry", Suggestion::Guid("1111"))});

  EXPECT_FALSE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_Autocomplete) {
  std::u16string title;
  std::u16string body;
  ShowSuggestions(manager(), {Suggestion(u"Autocomplete entry",
                                         PopupItemId::kAutocompleteEntry)});

  EXPECT_TRUE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
  EXPECT_EQ(title, u"Autocomplete entry");
  EXPECT_EQ(body,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_CONFIRMATION_BODY));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_LocalCreditCard) {
  CreditCard local_card = test::GetCreditCard();
  personal_data().AddCreditCard(local_card);

  std::u16string title;
  std::u16string body;
  ShowSuggestions(manager(),
                  {test::CreateAutofillSuggestion(
                      PopupItemId::kCreditCardEntry, u"Local credit card",
                      Suggestion::Guid(local_card.guid()))});

  EXPECT_TRUE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
  EXPECT_EQ(title, local_card.CardNameAndLastFourDigits());
  EXPECT_EQ(body,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_DELETE_CREDIT_CARD_SUGGESTION_CONFIRMATION_BODY));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_ServerCreditCard) {
  CreditCard server_card = test::GetMaskedServerCard();
  personal_data().AddServerCreditCard(server_card);

  std::u16string title;
  std::u16string body;
  ShowSuggestions(manager(),
                  {test::CreateAutofillSuggestion(
                      PopupItemId::kCreditCardEntry, u"Server credit card",
                      Suggestion::Guid(server_card.guid()))});

  EXPECT_FALSE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_CompleteAutofillProfile) {
  AutofillProfile complete_profile = test::GetFullProfile();
  personal_data().AddProfile(complete_profile);

  std::u16string title;
  std::u16string body;
  ShowSuggestions(manager(),
                  {test::CreateAutofillSuggestion(
                      PopupItemId::kAddressEntry, u"Complete autofill profile",
                      Suggestion::Guid(complete_profile.guid()))});

  EXPECT_TRUE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
  EXPECT_EQ(title, complete_profile.GetRawInfo(ADDRESS_HOME_CITY));
  EXPECT_EQ(body,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_DELETE_PROFILE_SUGGESTION_CONFIRMATION_BODY));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_AutofillProfile_EmptyCity) {
  AutofillProfile profile = test::GetFullProfile();
  profile.ClearFields({ADDRESS_HOME_CITY});
  personal_data().AddProfile(profile);

  std::u16string title;
  std::u16string body;
  ShowSuggestions(manager(), {test::CreateAutofillSuggestion(
                                 PopupItemId::kAddressEntry,
                                 u"Autofill profile without city",
                                 Suggestion::Guid(profile.guid()))});

  EXPECT_TRUE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
  EXPECT_EQ(title, u"Autofill profile without city");
  EXPECT_EQ(body,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_DELETE_PROFILE_SUGGESTION_CONFIRMATION_BODY));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       AcceptPwdSuggestionInvokesWarningAndroid) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  ShowSuggestions(manager(), {PopupItemId::kPasswordEntry});

  // Calls are accepted immediately.
  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(1);
  EXPECT_CALL(client().show_pwd_migration_warning_callback(),
              Run(_, _,
                  password_manager::metrics_util::
                      PasswordMigrationWarningTriggers::kKeyboardAcessoryBar));
  task_environment()->FastForwardBy(base::Milliseconds(500));
  client().popup_controller(manager()).AcceptSuggestion(0);
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       AcceptUsernameSuggestionInvokesWarningAndroid) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  ShowSuggestions(manager(), {PopupItemId::kPasswordEntry});

  // Calls are accepted immediately.
  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(1);
  EXPECT_CALL(client().show_pwd_migration_warning_callback(), Run);
  task_environment()->FastForwardBy(base::Milliseconds(500));
  client().popup_controller(manager()).AcceptSuggestion(0);
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       AcceptPwdSuggestionNoWarningIfDisabledAndroid) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  ShowSuggestions(manager(), {PopupItemId::kPasswordEntry});

  // Calls are accepted immediately.
  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(1);
  EXPECT_CALL(client().show_pwd_migration_warning_callback(), Run).Times(0);
  task_environment()->FastForwardBy(base::Milliseconds(500));
  client().popup_controller(manager()).AcceptSuggestion(0);
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       AcceptAddressNoPwdWarningAndroid) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});

  // Calls are accepted immediately.
  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(1);
  EXPECT_CALL(client().show_pwd_migration_warning_callback(), Run).Times(0);
  task_environment()->FastForwardBy(base::Milliseconds(500));
  client().popup_controller(manager()).AcceptSuggestion(0);
}

// When a suggestion is accepted, the popup is hidden inside
// `delegate->DidAcceptSuggestion()`. On Android, some code is still being
// executed after hiding. This test makes sure no use-after-free, null pointer
// dereferencing or other memory violations occur.
TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       AcceptSuggestionIsMemorySafe) {
  ShowSuggestions(manager(), {PopupItemId::kPasswordEntry});
  task_environment()->FastForwardBy(base::Milliseconds(500));

  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion)
      .WillOnce([this]() {
        client().popup_controller(manager()).Hide(
            PopupHidingReason::kAcceptSuggestion);
      });
  client().popup_controller(manager()).AcceptSuggestion(/*index=*/0);
}

// Tests that the `KeyboardAccessoryController` moves "clear form" suggestions
// to the front.
TEST_F(AutofillKeyboardAccessoryControllerImplTest, ReorderUpdatedSuggestions) {
  const std::vector<Suggestion> suggestions =
      CreateSuggestionsWithClearFormEntry(/*clear_form_offset=*/2);
  ShowSuggestions(manager(), suggestions);

  // TODO(crbug.com/333316034): Add expectation for the view.
  EXPECT_THAT(client().popup_controller(manager()).GetSuggestions(),
              ElementsAre(suggestions[2], suggestions[0], suggestions[1],
                          suggestions[3]));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       UseAdditionalLabelForElidedLabel) {
  auto label_is = [](std::u16string label) {
    return ElementsAre(ElementsAre(Suggestion::Text(std::move(label))));
  };

  ShowSuggestions(manager(),
                  CreateSuggestionsWithClearFormEntry(/*clear_form_offset=*/1));

  // The 1st item is usually not visible (something like clear form) and has an
  // empty label. But it needs to be handled since UI might ask for it anyway.
  EXPECT_THAT(client().popup_controller(manager()).GetSuggestionLabelsAt(0),
              label_is(std::u16string()));

  // If there is a label, use it but cap at 8 bullets.
  EXPECT_THAT(client().popup_controller(manager()).GetSuggestionLabelsAt(1),
              label_is(u"********"));

  // If the label is empty, use the additional label:
  EXPECT_THAT(client().popup_controller(manager()).GetSuggestionLabelsAt(2),
              label_is(u"psl.origin.eg ********"));

  // If the password has less than 8 bullets, show the exact amount.
  EXPECT_THAT(client().popup_controller(manager()).GetSuggestionLabelsAt(3),
              label_is(u"***"));
}

}  // namespace autofill
