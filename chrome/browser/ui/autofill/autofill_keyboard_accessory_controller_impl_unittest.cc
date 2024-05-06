// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_controller_impl.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_test_base.h"
#include "chrome/browser/ui/autofill/test_autofill_keyboard_accessory_controller_autofill_client.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::Return;

std::vector<Suggestion> CreateSuggestionsWithClearFormEntry(
    size_t clear_form_offset) {
  auto create_pw_suggestion = [](std::string_view password,
                                 std::string_view username,
                                 std::string_view origin) {
    Suggestion s(/*main_text=*/username, /*label=*/origin,
                 Suggestion::Icon::kNoIcon, SuggestionType::kPasswordEntry);
    s.additional_label = base::UTF8ToUTF16(password);
    return s;
  };
  std::vector<Suggestion> suggestions = {
      create_pw_suggestion("****************", "Alf", ""),
      create_pw_suggestion("****************", "Berta", "psl.origin.eg"),
      create_pw_suggestion("***", "Carl", "")};
  suggestions.emplace(suggestions.begin() + clear_form_offset, "Clear", "",
                      Suggestion::Icon::kNoIcon, SuggestionType::kClearForm);
  return suggestions;
}

class AutofillKeyboardAccessoryControllerImplTest
    : public AutofillSuggestionControllerTestBase<
          TestAutofillKeyboardAccessoryControllerAutofillClient<>> {
 protected:
  AutofillProfile ShowAutofillProfileSuggestion() {
    AutofillProfile complete_profile = test::GetFullProfile();
    personal_data().address_data_manager().AddProfile(complete_profile);
    ShowSuggestions(manager(), {test::CreateAutofillSuggestion(
                                   SuggestionType::kAddressEntry,
                                   u"Complete autofill profile",
                                   Suggestion::Guid(complete_profile.guid()))});
    return complete_profile;
  }

  CreditCard ShowLocalCardSuggestion() {
    CreditCard local_card = test::GetCreditCard();
    personal_data().payments_data_manager().AddCreditCard(local_card);
    ShowSuggestions(manager(),
                    {test::CreateAutofillSuggestion(
                        SuggestionType::kCreditCardEntry, u"Local credit card",
                        Suggestion::Guid(local_card.guid()))});
    return local_card;
  }
};

}  // namespace

// Tests that calling `Show()` on the controller shows the view.
TEST_F(AutofillKeyboardAccessoryControllerImplTest, ShowCallsView) {
  // Ensure that controller and view have been created.
  client().popup_controller(manager());

  EXPECT_CALL(*client().popup_view(), Show());
  ShowSuggestions(manager(), {Suggestion(u"Autocomplete entry",
                                         SuggestionType::kAutocompleteEntry)});
}

// Tests that calling `Hide()` on the controller hides and destroys the view.
TEST_F(AutofillKeyboardAccessoryControllerImplTest, HideDestroysView) {
  ShowSuggestions(manager(), {Suggestion(u"Autocomplete entry",
                                         SuggestionType::kAutocompleteEntry)});

  EXPECT_CALL(*client().popup_view(), Hide);
  client().popup_controller(manager()).Hide(SuggestionHidingReason::kTabGone);
  // The keyboard accessory view is destroyed synchronously.
  EXPECT_FALSE(client().popup_view());
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_UnrelatedSuggestionType) {
  std::u16string title;
  std::u16string body;
  ShowSuggestions(
      manager(),
      {Suggestion(u"Entry", SuggestionType::kAddressFieldByFieldFilling)});

  EXPECT_FALSE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_InvalidUniqueId) {
  std::u16string title;
  std::u16string body;
  ShowSuggestions(manager(), {test::CreateAutofillSuggestion(
                                 SuggestionType::kAddressFieldByFieldFilling,
                                 u"Entry", Suggestion::Guid("1111"))});

  EXPECT_FALSE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_Autocomplete) {
  std::u16string title;
  std::u16string body;
  ShowSuggestions(manager(), {Suggestion(u"Autocomplete entry",
                                         SuggestionType::kAutocompleteEntry)});

  EXPECT_TRUE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
  EXPECT_EQ(title, u"Autocomplete entry");
  EXPECT_EQ(body,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_CONFIRMATION_BODY));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_LocalCreditCard) {
  CreditCard local_card = ShowLocalCardSuggestion();

  std::u16string title;
  std::u16string body;
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
                      SuggestionType::kCreditCardEntry, u"Server credit card",
                      Suggestion::Guid(server_card.guid()))});

  EXPECT_FALSE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_CompleteAutofillProfile) {
  AutofillProfile complete_profile = ShowAutofillProfileSuggestion();

  std::u16string title;
  std::u16string body;
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
  personal_data().address_data_manager().AddProfile(profile);

  std::u16string title;
  std::u16string body;
  ShowSuggestions(manager(), {test::CreateAutofillSuggestion(
                                 SuggestionType::kAddressEntry,
                                 u"Autofill profile without city",
                                 Suggestion::Guid(profile.guid()))});

  EXPECT_TRUE(client().popup_controller(manager()).GetRemovalConfirmationText(
      0, &title, &body));
  EXPECT_EQ(title, u"Autofill profile without city");
  EXPECT_EQ(body,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_DELETE_PROFILE_SUGGESTION_CONFIRMATION_BODY));
}

// Tests that a call to `RemoveSuggestion()` leads to a deletion confirmation
// dialog and, on accepting that dialog, to the deletion of the suggestion and
// the a11y announcement that it was deleted.
TEST_F(AutofillKeyboardAccessoryControllerImplTest, RemoveAfterConfirmation) {
  const auto suggestion =
      Suggestion(u"Autocomplete entry", SuggestionType::kAutocompleteEntry);
  ShowSuggestions(manager(), {suggestion});
  ASSERT_TRUE(client().popup_view());

  EXPECT_CALL(*client().popup_view(), ConfirmDeletion)
      .WillOnce(base::test::RunOnceCallback<2>(/*confirmed=*/true));
  EXPECT_CALL(manager().external_delegate(), RemoveSuggestion(suggestion))
      .WillOnce(Return(true));
  EXPECT_CALL(*client().popup_view(),
              AxAnnounce(Eq(u"Entry Autocomplete entry has been deleted")));

  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(
      /*index=*/0,
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardAccessory));
}

// Tests that the correct metrics are logged when the confirmation dialog for
// deleting an Autofill profile is cancelled.
TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       MetricsAfterAddressDeletionDeclined) {
  ShowAutofillProfileSuggestion();
  ASSERT_TRUE(client().popup_view());

  base::HistogramTester histogram;
  EXPECT_CALL(*client().popup_view(), ConfirmDeletion)
      .WillOnce(base::test::RunOnceCallback<2>(/*confirmed=*/false));
  EXPECT_CALL(manager().external_delegate(), RemoveSuggestion).Times(0);

  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(
      /*index=*/0,
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardAccessory));
  histogram.ExpectUniqueSample("Autofill.ProfileDeleted.ExtendedMenu", false,
                               1);
  histogram.ExpectUniqueSample("Autofill.ProfileDeleted.Any", false, 1);
}

// Tests that no metrics are logged when the confirmation dialog for deleting a
// credit card is cancelled.
TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       MetricsAfterCreditCardDeletionDeclined) {
  ShowLocalCardSuggestion();
  ASSERT_TRUE(client().popup_view());

  base::HistogramTester histogram;
  EXPECT_CALL(*client().popup_view(), ConfirmDeletion)
      .WillOnce(base::test::RunOnceCallback<2>(/*confirmed=*/false));
  EXPECT_CALL(manager().external_delegate(), RemoveSuggestion).Times(0);

  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(
      /*index=*/0,
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardAccessory));
  histogram.ExpectUniqueSample("Autofill.ProfileDeleted.ExtendedMenu", false,
                               0);
  histogram.ExpectUniqueSample("Autofill.ProfileDeleted.Any", false, 0);
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       AcceptPwdSuggestionInvokesWarningAndroid) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  ShowSuggestions(manager(), {SuggestionType::kPasswordEntry});

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
  ShowSuggestions(manager(), {SuggestionType::kPasswordEntry});

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
  ShowSuggestions(manager(), {SuggestionType::kPasswordEntry});

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
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});

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
  ShowSuggestions(manager(), {SuggestionType::kPasswordEntry});
  task_environment()->FastForwardBy(base::Milliseconds(500));

  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion)
      .WillOnce([this]() {
        client().popup_controller(manager()).Hide(
            SuggestionHidingReason::kAcceptSuggestion);
      });
  client().popup_controller(manager()).AcceptSuggestion(/*index=*/0);
}

// Tests that the `KeyboardAccessoryController` moves "clear form" suggestions
// to the front.
TEST_F(AutofillKeyboardAccessoryControllerImplTest, ReorderUpdatedSuggestions) {
  const std::vector<Suggestion> suggestions =
      CreateSuggestionsWithClearFormEntry(/*clear_form_offset=*/2);
  // Force creation of controller and view.
  client().popup_controller(manager());
  EXPECT_CALL(*client().popup_view(), Show);
  ShowSuggestions(manager(), suggestions);

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
