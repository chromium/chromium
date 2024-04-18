// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_controller_impl.h"

#include <string>

#include "base/time/time.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_test_base.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

using ::testing::_;

using AutofillKeyboardAccessoryControllerImplTest =
    AutofillSuggestionControllerTestBase<>;

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

}  // namespace autofill
