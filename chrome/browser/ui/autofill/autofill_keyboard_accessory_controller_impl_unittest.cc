// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_controller_impl.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_test_base.h"
#include "chrome/browser/ui/autofill/test_autofill_keyboard_accessory_controller_autofill_client.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_test_api.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::MockFunction;
using ::testing::Property;
using ::testing::Return;

using RemovalConfirmationText =
    AutofillKeyboardAccessoryController::RemovalConfirmationText;

auto MatchesConfirmationText(const std::u16string& title,
                             const std::u16string& body,
                             const std::u16string& button_text,
                             bool is_body_link_empty) {
  return AllOf(
      Field("title", &RemovalConfirmationText::title, title),
      Field("body", &RemovalConfirmationText::body, body),
      Field("confirm_button_text",
            &RemovalConfirmationText::confirm_button_text, button_text),
      Field("body_link", &RemovalConfirmationText::body_link,
            Property(&std::u16string::empty, is_body_link_empty)));
}

std::vector<Suggestion> CreateSuggestionsWithUndoOrClearEntry(
    size_t clear_form_offset) {
  auto create_pw_suggestion = [](std::string_view password,
                                 std::string_view username,
                                 std::string_view origin) {
    Suggestion s(/*main_text=*/username, /*label=*/password,
                 Suggestion::Icon::kNoIcon, SuggestionType::kPasswordEntry);
    s.additional_label = base::UTF8ToUTF16(origin);
    return s;
  };
  std::vector<Suggestion> suggestions = {
      create_pw_suggestion("****************", "Alf", ""),
      create_pw_suggestion("****************", "Berta", "psl.origin.eg"),
      create_pw_suggestion("***", "Carl", "")};
  suggestions.emplace(suggestions.begin() + clear_form_offset, "Clear", "",
                      Suggestion::Icon::kNoIcon, SuggestionType::kUndoOrClear);
  return suggestions;
}

class AutofillKeyboardAccessoryControllerImplTest
    : public AutofillSuggestionControllerTestBase<
          TestAutofillKeyboardAccessoryControllerAutofillClient<>> {
 protected:
  void ShowAutofillProfileSuggestion(AutofillProfile complete_profile) {
    personal_data().address_data_manager().AddProfile(complete_profile);
    ShowSuggestions(
        manager(),
        {test::CreateAutofillSuggestion(
            SuggestionType::kAddressEntry, u"Complete autofill profile",
            Suggestion::AutofillProfilePayload(
                Suggestion::Guid(complete_profile.guid())))});
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

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       AcceptSuggestionRespectsTimeout) {
  // Calls before the threshold are ignored.
  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call);
    EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion);
  }

  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, autofill::AutofillMetrics::SuggestionAcceptedMethod::kTap);
  task_environment()->FastForwardBy(base::Milliseconds(100));
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, autofill::AutofillMetrics::SuggestionAcceptedMethod::kTap);
  task_environment()->FastForwardBy(base::Milliseconds(400));

  // Only now suggestions should be accepted.
  check.Call();
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, autofill::AutofillMetrics::SuggestionAcceptedMethod::kTap);
}

// Tests that reshowing the suggestions resets the accept threshold.
TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       AcceptSuggestionTimeoutIsUpdatedOnUiUpdate) {
  // Calls before the threshold are ignored.
  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call);
    EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion);
  }

  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  // Calls before the threshold are ignored.
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, autofill::AutofillMetrics::SuggestionAcceptedMethod::kTap);
  task_environment()->FastForwardBy(base::Milliseconds(100));
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, autofill::AutofillMetrics::SuggestionAcceptedMethod::kTap);
  task_environment()->FastForwardBy(base::Milliseconds(400));

  // Show the suggestions again (simulating, e.g., a click somewhere slightly
  // different).
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, autofill::AutofillMetrics::SuggestionAcceptedMethod::kTap);

  // After waiting again, suggestions become acceptable.
  task_environment()->FastForwardBy(base::Milliseconds(500));
  check.Call();
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, autofill::AutofillMetrics::SuggestionAcceptedMethod::kTap);
}

// Tests that calling `Show()` on the controller shows the view.
TEST_F(AutofillKeyboardAccessoryControllerImplTest, ShowCallsView) {
  // Ensure that controller and view have been created.
  client().suggestion_controller(manager());

  EXPECT_CALL(*client().popup_view(), Show());
  ShowSuggestions(manager(), {Suggestion(u"Autocomplete entry",
                                         SuggestionType::kAutocompleteEntry)});
}

// Tests that calling `Hide()` on the controller hides and destroys the view.
TEST_F(AutofillKeyboardAccessoryControllerImplTest, HideDestroysView) {
  ShowSuggestions(manager(), {Suggestion(u"Autocomplete entry",
                                         SuggestionType::kAutocompleteEntry)});

  EXPECT_CALL(*client().popup_view(), Hide);
  client().suggestion_controller(manager()).Hide(
      SuggestionHidingReason::kTabGone);
  // The keyboard accessory view is destroyed synchronously.
  EXPECT_FALSE(client().popup_view());
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_UnrelatedSuggestionType) {
  ShowSuggestions(
      manager(),
      {Suggestion(u"Entry", SuggestionType::kAddressFieldByFieldFilling)});

  EXPECT_FALSE(
      client().suggestion_controller(manager()).GetRemovalConfirmationText(
          0, nullptr));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_InvalidUniqueId) {
  ShowSuggestions(manager(), {test::CreateAutofillSuggestion(
                                 SuggestionType::kAddressFieldByFieldFilling,
                                 u"Entry", Suggestion::Guid("1111"))});

  EXPECT_FALSE(
      client().suggestion_controller(manager()).GetRemovalConfirmationText(
          0, nullptr));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_Autocomplete) {
  ShowSuggestions(manager(), {Suggestion(u"Autocomplete entry",
                                         SuggestionType::kAutocompleteEntry)});
  RemovalConfirmationText confirmation_text;
  EXPECT_TRUE(
      client().suggestion_controller(manager()).GetRemovalConfirmationText(
          0, &confirmation_text));
  EXPECT_THAT(
      confirmation_text,
      MatchesConfirmationText(
          u"Autocomplete entry",
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_CONFIRMATION_BODY),
          l10n_util::GetStringUTF16(IDS_AUTOFILL_DELETE_SUGGESTION_BUTTON),
          /*is_body_link_empty=*/true));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_LocalCreditCard) {
  CreditCard local_card = ShowLocalCardSuggestion();
  RemovalConfirmationText confirmation_text;
  EXPECT_TRUE(
      client().suggestion_controller(manager()).GetRemovalConfirmationText(
          0, &confirmation_text));
  EXPECT_THAT(
      confirmation_text,
      MatchesConfirmationText(
          local_card.CardNameAndLastFourDigits(),
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_DELETE_CREDIT_CARD_SUGGESTION_CONFIRMATION_BODY),
          l10n_util::GetStringUTF16(IDS_AUTOFILL_DELETE_SUGGESTION_BUTTON),
          /*is_body_link_empty=*/true));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_ServerCreditCard) {
  CreditCard server_card = test::GetMaskedServerCard();
  personal_data().test_payments_data_manager().AddServerCreditCard(server_card);

  ShowSuggestions(manager(),
                  {test::CreateAutofillSuggestion(
                      SuggestionType::kCreditCardEntry, u"Server credit card",
                      Suggestion::Guid(server_card.guid()))});

  EXPECT_FALSE(
      client().suggestion_controller(manager()).GetRemovalConfirmationText(
          0, nullptr));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_CompleteAutofillProfile) {
  AutofillProfile complete_profile = test::GetFullProfile();
  ShowAutofillProfileSuggestion(complete_profile);
  RemovalConfirmationText confirmation_text;
  EXPECT_TRUE(
      client().suggestion_controller(manager()).GetRemovalConfirmationText(
          0, &confirmation_text));
  EXPECT_THAT(
      confirmation_text,
      MatchesConfirmationText(
          complete_profile.GetRawInfo(ADDRESS_HOME_CITY),
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_DELETE_PROFILE_SUGGESTION_CONFIRMATION_BODY),
          l10n_util::GetStringUTF16(IDS_AUTOFILL_DELETE_SUGGESTION_BUTTON),
          /*is_body_link_empty=*/true));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_CompleteAutofillHomeProfile) {
  AutofillProfile complete_profile = test::GetFullProfile();
  test_api(complete_profile)
      .set_record_type(AutofillProfile::RecordType::kAccountHome);
  ShowAutofillProfileSuggestion(complete_profile);
  std::u16string email =
      base::UTF8ToUTF16(GetPrimaryAccountInfoFromBrowserContext(
                            web_contents()->GetBrowserContext())
                            ->email);
  RemovalConfirmationText confirmation_text;
  EXPECT_TRUE(
      client().suggestion_controller(manager()).GetRemovalConfirmationText(
          0, &confirmation_text));
  EXPECT_THAT(
      confirmation_text,
      MatchesConfirmationText(
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_REMOVE_HOME_PROFILE_SUGGESTION_CONFIRMATION_TITLE),
          l10n_util::GetStringFUTF16(
              IDS_AUTOFILL_REMOVE_HOME_PROFILE_SUGGESTION_CONFIRMATION_BODY,
              email),
          l10n_util::GetStringUTF16(IDS_AUTOFILL_REMOVE_SUGGESTION_BUTTON),
          /*is_body_link_empty=*/false));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_CompleteAutofillWorkProfile) {
  AutofillProfile complete_profile = test::GetFullProfile();
  test_api(complete_profile)
      .set_record_type(AutofillProfile::RecordType::kAccountWork);
  ShowAutofillProfileSuggestion(complete_profile);
  std::u16string email =
      base::UTF8ToUTF16(GetPrimaryAccountInfoFromBrowserContext(
                            web_contents()->GetBrowserContext())
                            ->email);
  RemovalConfirmationText confirmation_text;
  EXPECT_TRUE(
      client().suggestion_controller(manager()).GetRemovalConfirmationText(
          0, &confirmation_text));
  EXPECT_THAT(
      confirmation_text,
      MatchesConfirmationText(
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_REMOVE_WORK_PROFILE_SUGGESTION_CONFIRMATION_TITLE),
          l10n_util::GetStringFUTF16(
              IDS_AUTOFILL_REMOVE_WORK_PROFILE_SUGGESTION_CONFIRMATION_BODY,
              email),
          l10n_util::GetStringUTF16(IDS_AUTOFILL_REMOVE_SUGGESTION_BUTTON),
          /*is_body_link_empty=*/false));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_CompleteAutofillAccountNameEmailProfile) {
  AutofillProfile complete_profile = test::GetFullProfile();
  test_api(complete_profile)
      .set_record_type(AutofillProfile::RecordType::kAccountNameEmail);
  ShowAutofillProfileSuggestion(complete_profile);
  std::u16string email =
      base::UTF8ToUTF16(GetPrimaryAccountInfoFromBrowserContext(
                            web_contents()->GetBrowserContext())
                            ->email);
  RemovalConfirmationText confirmation_text;
  EXPECT_TRUE(
      client().suggestion_controller(manager()).GetRemovalConfirmationText(
          0, &confirmation_text));
  EXPECT_THAT(
      confirmation_text,
      MatchesConfirmationText(
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_REMOVE_ACCOUNT_NAME_AND_EMAIL_PROFILE_SUGGESTION_CONFIRMATION_TITLE),
          l10n_util::GetStringFUTF16(
              IDS_AUTOFILL_REMOVE_ACCOUNT_NAME_AND_EMAIL_PROFILE_SUGGESTION_CONFIRMATION_BODY,
              email),
          l10n_util::GetStringUTF16(IDS_AUTOFILL_REMOVE_SUGGESTION_BUTTON),
          /*is_body_link_empty=*/false));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       GetRemovalConfirmationText_AutofillProfile_EmptyCity) {
  AutofillProfile profile = test::GetFullProfile();
  profile.ClearFields({ADDRESS_HOME_CITY});
  personal_data().address_data_manager().AddProfile(profile);
  ShowSuggestions(manager(), {test::CreateAutofillSuggestion(
                                 SuggestionType::kAddressEntry,
                                 u"Autofill profile without city",
                                 Suggestion::AutofillProfilePayload(
                                     Suggestion::Guid(profile.guid())))});

  RemovalConfirmationText confirmation_text;
  EXPECT_TRUE(
      client().suggestion_controller(manager()).GetRemovalConfirmationText(
          0, &confirmation_text));
  EXPECT_THAT(
      confirmation_text,
      MatchesConfirmationText(
          u"Autofill profile without city",
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_DELETE_PROFILE_SUGGESTION_CONFIRMATION_BODY),
          l10n_util::GetStringUTF16(IDS_AUTOFILL_DELETE_SUGGESTION_BUTTON),
          /*is_body_link_empty=*/true));
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
      .WillOnce(base::test::RunOnceCallback<4>(/*confirmed=*/true));
  EXPECT_CALL(manager().external_delegate(), RemoveSuggestion(suggestion))
      .WillOnce(Return(true));
  EXPECT_CALL(*client().popup_view(),
              AxAnnounce(Eq(u"Entry Autocomplete entry has been deleted")));

  EXPECT_TRUE(client().suggestion_controller(manager()).RemoveSuggestion(
      /*index=*/0,
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardAccessory));
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
        client().suggestion_controller(manager()).Hide(
            SuggestionHidingReason::kAcceptSuggestion);
      });
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, autofill::AutofillMetrics::SuggestionAcceptedMethod::kTap);
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       DoesNotAcceptUnacceptableSuggestions) {
  Suggestion suggestion(u"Open the pod bay doors, HAL",
                        SuggestionType::kAutocompleteEntry);
  suggestion.acceptability = Suggestion::Acceptability::kUnacceptable;
  ShowSuggestions(manager(), {std::move(suggestion)});
  task_environment()->FastForwardBy(base::Milliseconds(500));

  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(0);
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, autofill::AutofillMetrics::SuggestionAcceptedMethod::kTap);
}

// Tests that the `KeyboardAccessoryController` moves "clear form" suggestions
// to the front.
TEST_F(AutofillKeyboardAccessoryControllerImplTest, ReorderUpdatedSuggestions) {
  const std::vector<Suggestion> suggestions =
      CreateSuggestionsWithUndoOrClearEntry(/*clear_form_offset=*/2);
  // Force creation of controller and view.
  client().suggestion_controller(manager());
  EXPECT_CALL(*client().popup_view(), Show);
  ShowSuggestions(manager(), suggestions);

  EXPECT_THAT(client().suggestion_controller(manager()).GetSuggestions(),
              ElementsAre(suggestions[2], suggestions[0], suggestions[1],
                          suggestions[3]));
}

TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       UseAdditionalLabelForElidedLabel) {
  auto label_is = [](std::u16string label) {
    return ElementsAre(ElementsAre(Suggestion::Text(std::move(label))));
  };

  ShowSuggestions(manager(), CreateSuggestionsWithUndoOrClearEntry(
                                 /*clear_form_offset=*/1));

  // The 1st item is usually not visible (something like clear form) and has an
  // empty label. But it needs to be handled since UI might ask for it anyway.
  EXPECT_THAT(
      client().suggestion_controller(manager()).GetSuggestionLabelsAt(0),
      label_is(std::u16string()));

  // If there is a label, use it but cap at 8 bullets.
  EXPECT_THAT(
      client().suggestion_controller(manager()).GetSuggestionLabelsAt(1),
      label_is(u"********"));

  // If the label is empty, use the additional label:
  EXPECT_THAT(
      client().suggestion_controller(manager()).GetSuggestionLabelsAt(2),
      label_is(u"psl.origin.eg ********"));

  // If the password has less than 8 bullets, show the exact amount.
  EXPECT_THAT(
      client().suggestion_controller(manager()).GetSuggestionLabelsAt(3),
      label_is(u"***"));
}

// This is a regression test for crbug.com/521133 to ensure that we don't crash
// when suggestions updates race with user selections.
TEST_F(AutofillKeyboardAccessoryControllerImplTest, SelectInvalidSuggestion) {
  ShowSuggestions(manager(), {SuggestionType::kMixedFormMessage});

  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(0);

  // The following should not crash:
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0,  // Non-acceptable type.
      autofill::AutofillMetrics::SuggestionAcceptedMethod::kTap);
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/1,  // Out of bounds!
      autofill::AutofillMetrics::SuggestionAcceptedMethod::kTap);
}

// Tests that the profile deletion metric is recorded as true (accepted) when
// the user confirms the deletion dialog.
TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       RecordsAcceptedDeletionMetric) {
  base::HistogramTester histogram_tester;
  AutofillProfile complete_profile = test::GetFullProfile();
  ShowAutofillProfileSuggestion(complete_profile);
  ASSERT_TRUE(client().popup_view());

  // Simulate user accepting deletion dialog.
  EXPECT_CALL(*client().popup_view(), ConfirmDeletion)
      .WillOnce(base::test::RunOnceCallback<4>(/*confirmed=*/true));
  client().suggestion_controller(manager()).RemoveSuggestion(
      /*index=*/0,
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardAccessory);

  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.KeyboardAccessory.Total", 1, 1);
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Any.Total", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.KeyboardAccessory.LocalOrSyncable", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.Any.LocalOrSyncable", 1, 1);
}

// Tests that the profile deletion metric is recorded as false (canceled) when
// the user cancels the deletion dialog.
TEST_F(AutofillKeyboardAccessoryControllerImplTest,
       RecordsCanceledDeletionMetric) {
  base::HistogramTester histogram_tester;
  AutofillProfile complete_profile = test::GetFullProfile();
  ShowAutofillProfileSuggestion(complete_profile);
  ASSERT_TRUE(client().popup_view());

  // Simulate user cancelling deletion dialog.
  EXPECT_CALL(*client().popup_view(), ConfirmDeletion)
      .WillOnce(base::test::RunOnceCallback<4>(/*confirmed=*/false));
  client().suggestion_controller(manager()).RemoveSuggestion(
      /*index=*/0,
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardAccessory);

  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.KeyboardAccessory.Total", 0, 1);
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Any.Total", 0,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.KeyboardAccessory.LocalOrSyncable", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.Any.LocalOrSyncable", 0, 1);
}

}  // namespace
}  // namespace autofill
