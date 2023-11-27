// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/iban_manager.h"

#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/mock_autofill_optimization_guide.h"
#include "components/autofill/core/browser/suggestions_context.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"
#include "ui/gfx/image/image_unittest_util.h"

using testing::_;
using testing::Field;
using testing::Truly;
using testing::UnorderedElementsAre;

namespace autofill {

constexpr char kNickname_0[] = "Nickname 0";
constexpr char kNickname_1[] = "Nickname 1";

namespace {

using MockSuggestionsReturnedCallback =
    base::MockCallback<SingleFieldFormFiller::OnSuggestionsReturnedCallback>;

}  // namespace

class IbanManagerTest : public testing::Test {
 protected:
  IbanManagerTest() : iban_manager_(&personal_data_manager_) {}

  void SetUp() override {
    personal_data_manager_.SetAutofillPaymentMethodsEnabled(true);
    original_resource_bundle_ =
        ui::ResourceBundle::SwapSharedInstanceForTesting(nullptr);

    ui::ResourceBundle::InitSharedInstanceWithLocale(
        "en-US", &mock_resource_delegate_,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
    ON_CALL(mock_resource_delegate_, GetImageNamed(IDR_AUTOFILL_IBAN))
        .WillByDefault(testing::Return(gfx::test::CreateImage()));

    ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
                autofill_client_.GetAutofillOptimizationGuide()),
            ShouldBlockSingleFieldSuggestions)
        .WillByDefault(testing::Return(false));
  }

  void TearDown() override {
    ui::ResourceBundle::CleanupSharedInstance();
    ui::ResourceBundle::SwapSharedInstanceForTesting(original_resource_bundle_);
  }

  // Sets up the TestPersonalDataManager with a local IBAN.
  Iban SetUpLocalIban(std::string_view value, std::string_view nickname) {
    Iban iban;
    iban.set_value(base::UTF8ToUTF16(std::string(value)));
    iban.set_nickname(base::UTF8ToUTF16(std::string(nickname)));
    personal_data_manager_.AddAsLocalIban(iban);
    return iban;
  }

  // Sets up the TestPersonalDataManager with a server IBAN.
  Iban SetUpServerIban(int64_t instrument_id,
                       std::string_view prefix,
                       std::string_view suffix,
                       int length,
                       std::string_view nickname) {
    Iban iban{Iban::InstrumentId(instrument_id)};
    iban.set_prefix(base::UTF8ToUTF16(std::string(prefix)));
    iban.set_suffix(base::UTF8ToUTF16(std::string(suffix)));
    iban.set_length(length);
    iban.set_nickname(base::UTF8ToUTF16(std::string(nickname)));
    personal_data_manager_.AddServerIban(iban);
    return iban;
  }

  // Adds an IBAN focused field to the suggestions context.
  SuggestionsContext GetIbanFocusedSuggestionsContext(
      AutofillField& autofill_field,
      autofill::ServerFieldType type = IBAN_VALUE) {
    SuggestionsContext context;
    autofill_field.SetTypeTo(AutofillType(type));
    context.focused_field = &autofill_field;
    form_structure_ =
        std::make_unique<FormStructure>(test::CreateTestIbanFormData());
    context.form_structure = form_structure_.get();
    return context;
  }

  // Get an IBAN suggestion with the given `iban`.
  Suggestion GetSuggestionForIban(const Iban& iban) {
    Suggestion iban_suggestion(iban.GetIdentifierStringForAutofillDisplay());
    iban_suggestion.popup_item_id = PopupItemId::kIbanEntry;
    return iban_suggestion;
  }

  Suggestion SetUpSeparator() {
    Suggestion separator;
    separator.popup_item_id = PopupItemId::kSeparator;
    return separator;
  }

  Suggestion SetUpFooterManagePaymentMethods() {
    Suggestion footer_suggestion(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS));
    footer_suggestion.popup_item_id = PopupItemId::kAutofillOptions;
    footer_suggestion.icon = Suggestion::Icon::kSettings;
    return footer_suggestion;
  }

  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  TestPersonalDataManager personal_data_manager_;
  std::unique_ptr<FormStructure> form_structure_;
  IbanManager iban_manager_;
  testing::NiceMock<ui::MockResourceBundleDelegate> mock_resource_delegate_;
  raw_ptr<ui::ResourceBundle> original_resource_bundle_;
};

MATCHER_P(MatchesTextAndPopupItemId, suggestion, "") {
  return arg.main_text == suggestion.main_text &&
         arg.popup_item_id == suggestion.popup_item_id;
}

TEST_F(IbanManagerTest, ShowsAllIbanSuggestions) {
  personal_data_manager_.SetAutofillWalletImportEnabled(true);
  Suggestion local_iban_suggestion_0 =
      GetSuggestionForIban(SetUpLocalIban(test::kIbanValue, kNickname_0));
  Suggestion local_iban_suggestion_1 =
      GetSuggestionForIban(SetUpLocalIban(test::kIbanValue_1, kNickname_1));
  Suggestion server_iban_suggestion_0 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12345, /*prefix=*/"DE91", /*suffix=*/"6789",
      /*length=*/22, kNickname_0));
  Suggestion server_iban_suggestion_1 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12346, /*prefix=*/"CH56", /*suffix=*/"8009",
      /*length=*/34, kNickname_1));
  Suggestion seperator_suggestion = SetUpSeparator();
  Suggestion footer_suggestion = SetUpFooterManagePaymentMethods();

  AutofillField test_field;
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Setting up mock to verify that the handler is returned a list of
  // IBAN-based suggestions.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(test_field.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  testing::UnorderedElementsAre(
                      MatchesTextAndPopupItemId(local_iban_suggestion_0),
                      MatchesTextAndPopupItemId(local_iban_suggestion_1),
                      MatchesTextAndPopupItemId(server_iban_suggestion_0),
                      MatchesTextAndPopupItemId(server_iban_suggestion_1),
                      MatchesTextAndPopupItemId(seperator_suggestion),
                      MatchesTextAndPopupItemId(footer_suggestion))));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));
}

TEST_F(IbanManagerTest, PaymentsAutofillEnabledPrefOff_NoIbanSuggestionsShown) {
  personal_data_manager_.SetAutofillPaymentMethodsEnabled(false);
  GetSuggestionForIban(SetUpLocalIban(test::kIbanValue, kNickname_0));
  GetSuggestionForIban(SetUpLocalIban(test::kIbanValue_1, kNickname_1));

  AutofillField test_field;
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);

  // Because the "Save and autofill payment methods" toggle is off, the
  // suggestion handler should not be triggered.
  EXPECT_FALSE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));
}

TEST_F(IbanManagerTest, IbanSuggestions_SeparatorAndFooter) {
  Suggestion iban_suggestion_0 =
      GetSuggestionForIban(SetUpLocalIban(test::kIbanValue, kNickname_0));
  Suggestion iban_suggestion_1 = SetUpSeparator();
  Suggestion iban_suggestion_2 = SetUpFooterManagePaymentMethods();

  AutofillField test_field;
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Setting up mock to verify that the handler is returned IBAN-based
  // suggestions. A separator and the "Manage payment methods..." row should
  // also be returned.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(test_field.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  testing::UnorderedElementsAre(
                      MatchesTextAndPopupItemId(iban_suggestion_0),
                      MatchesTextAndPopupItemId(iban_suggestion_1),
                      MatchesTextAndPopupItemId(iban_suggestion_2))));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));
}

TEST_F(IbanManagerTest,
       OnGetSingleFieldSuggestions_FieldEqualsLocalIban_NothingReturned) {
  Suggestion iban_suggestion_0 = GetSuggestionForIban(
      SetUpLocalIban("CH93 0076 2011 6238 5295 7", kNickname_0));
  Suggestion iban_suggestion_1 = GetSuggestionForIban(
      SetUpLocalIban("CH56 0483 5012 3456 7800 9", kNickname_1));

  AutofillField test_field;
  test_field.value = u"CH5604835012345678009";
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // The field contains value matches existing IBAN already, so check that we do
  // not return suggestions to the handler.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));
}

TEST_F(IbanManagerTest,
       OnGetSingleFieldSuggestions_LocalIbansMatchingPrefix_Shows) {
  Suggestion iban_suggestion_0 =
      GetSuggestionForIban(SetUpLocalIban(test::kIbanValue_1, kNickname_0));
  Suggestion iban_suggestion_1 =
      GetSuggestionForIban(SetUpLocalIban(test::kIbanValue_2, kNickname_1));
  Suggestion iban_suggestion_2 = SetUpSeparator();
  Suggestion iban_suggestion_3 = SetUpFooterManagePaymentMethods();

  AutofillField test_field;
  test_field.value = u"CH";
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Setting up mock to verify that the handler is returned a list of
  // IBAN-based suggestions whose prefixes match `prefix_`. Both values should
  // be returned because they both start with CH56. Other than that, there are
  // one separator and one footer suggestion displayed.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(test_field.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  testing::UnorderedElementsAre(
                      MatchesTextAndPopupItemId(iban_suggestion_0),
                      MatchesTextAndPopupItemId(iban_suggestion_1),
                      MatchesTextAndPopupItemId(iban_suggestion_2),
                      MatchesTextAndPopupItemId(iban_suggestion_3))));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));

  test_field.value = u"CH5604";

  // Setting up mock to verify that the handler is returned only one
  // IBAN-based suggestion whose prefix matches `prefix_`. Only one of the two
  // IBANs should stay because the other will be filtered out. Other than that,
  // there are one separator and one footer suggestion displayed.
  EXPECT_CALL(mock_callback,
              Run(test_field.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  testing::UnorderedElementsAre(
                      MatchesTextAndPopupItemId(iban_suggestion_0),
                      MatchesTextAndPopupItemId(iban_suggestion_2),
                      MatchesTextAndPopupItemId(iban_suggestion_3))));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));

  test_field.value = u"AB56";

  // Verify that the handler is not triggered because no IBAN suggestions match
  // the given prefix.
  EXPECT_CALL(mock_callback, Run).Times(0);

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));
}

// Test that when the input text field is shorter than IBAN's prefix, all IBANs
// with matching prefixes should be returned.
TEST_F(IbanManagerTest,
       OnGetSingleFieldSuggestions_ServerIbansMatchingPrefix_Shows_All) {
  personal_data_manager_.SetAutofillWalletImportEnabled(true);
  // Set up two server IBANs with different prefixes except for the first two
  // characters, and with same suffixes and lengths.
  Suggestion server_iban_suggestion_0 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12345, /*prefix=*/"CH56", /*suffix=*/"8009",
      /*length=*/21, /*nickname=*/"My IBAN"));
  Suggestion server_iban_suggestion_1 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12346, /*prefix=*/"CH78", /*suffix=*/"8009",
      /*length=*/21, /*nickname=*/"My doctor's IBAN"));
  Suggestion separator_suggestion = SetUpSeparator();
  Suggestion footer_suggestion = SetUpFooterManagePaymentMethods();

  AutofillField test_field;
  test_field.value = u"CH";
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Expect that a list of IBAN suggestions whose prefixes match input field is
  // returned because they both start with "CH". Other than that, there is one
  // separator and one footer suggestion displayed.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(test_field.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  testing::UnorderedElementsAre(
                      MatchesTextAndPopupItemId(server_iban_suggestion_0),
                      MatchesTextAndPopupItemId(server_iban_suggestion_1),
                      MatchesTextAndPopupItemId(separator_suggestion),
                      MatchesTextAndPopupItemId(footer_suggestion))));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));
}

// Test that when the input text field is shorter than IBAN's prefix, only IBANs
// with matching prefixes should be returned.
TEST_F(IbanManagerTest,
       OnGetSingleFieldSuggestions_ServerIbansMatchingPrefix_Shows_Some) {
  personal_data_manager_.SetAutofillWalletImportEnabled(true);
  // Set up two server IBANs with different prefixes except for the first two
  // characters, and with same suffixes and lengths.
  Suggestion server_iban_suggestion_0 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12345, /*prefix=*/"CH56", /*suffix=*/"8009",
      /*length=*/21, /*nickname=*/"My IBAN"));
  Suggestion server_iban_suggestion_1 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12346, /*prefix=*/"CH78", /*suffix=*/"8009",
      /*length=*/21, /*nickname=*/"My doctor's IBAN"));
  Suggestion separator_suggestion = SetUpSeparator();
  Suggestion footer_suggestion = SetUpFooterManagePaymentMethods();

  AutofillField test_field;
  test_field.value = u"CH567";
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Expect that only one of the two IBANs should stay because the other will be
  // filtered out. Other than that, there is one separator and one footer
  // suggestion displayed.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(test_field.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  testing::UnorderedElementsAre(
                      MatchesTextAndPopupItemId(server_iban_suggestion_0),
                      MatchesTextAndPopupItemId(separator_suggestion),
                      MatchesTextAndPopupItemId(footer_suggestion))));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));
}

// Test that when there is no prefix present, all server IBANs should be
// recommended when the character count of the input text is less than
// `kFieldLengthLimitOnServerIbanSuggestion`.
TEST_F(
    IbanManagerTest,
    OnGetSingleFieldSuggestions_ServerIbansLackingPrefix_ShowsIfFewCharsInField) {
  personal_data_manager_.SetAutofillWalletImportEnabled(true);
  // Set up three server IBANs with empty `prefix`.
  Suggestion server_iban_suggestion_0 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12345, /*prefix=*/"", /*suffix=*/"8009",
      /*length=*/21, /*nickname=*/"My IBAN"));
  Suggestion server_iban_suggestion_1 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12346, /*prefix=*/"", /*suffix=*/"8009",
      /*length=*/24, /*nickname=*/"My doctor's IBAN"));
  Suggestion server_iban_suggestion_2 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12347, /*prefix=*/"", /*suffix=*/"9123",
      /*length=*/28, /*nickname=*/"My sister's IBAN"));
  Suggestion separator_suggestion = SetUpSeparator();
  Suggestion footer_suggestion = SetUpFooterManagePaymentMethods();

  AutofillField test_field;
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Expect that all server IBANs are returned.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(test_field.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  testing::UnorderedElementsAre(
                      MatchesTextAndPopupItemId(server_iban_suggestion_0),
                      MatchesTextAndPopupItemId(server_iban_suggestion_1),
                      MatchesTextAndPopupItemId(server_iban_suggestion_2),
                      MatchesTextAndPopupItemId(separator_suggestion),
                      MatchesTextAndPopupItemId(footer_suggestion))));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));

  test_field.value = u"AB567";

  // Expect that all server IBANs are returned because the count of input
  // character is less than `kFieldLengthLimitOnServerIbanSuggestion`.
  EXPECT_CALL(mock_callback,
              Run(test_field.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  testing::UnorderedElementsAre(
                      MatchesTextAndPopupItemId(server_iban_suggestion_0),
                      MatchesTextAndPopupItemId(server_iban_suggestion_1),
                      MatchesTextAndPopupItemId(server_iban_suggestion_2),
                      MatchesTextAndPopupItemId(separator_suggestion),
                      MatchesTextAndPopupItemId(footer_suggestion))));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));
}

// Test that when there is no prefix present, no server IBANs should be
// recommended if the length equals or exceeds
// `kFieldLengthLimitOnServerIbanSuggestion`.
TEST_F(
    IbanManagerTest,
    OnGetSingleFieldSuggestions_ServerIbansLackingPrefix_HidesIfManyCharsInField) {
  personal_data_manager_.SetAutofillWalletImportEnabled(true);
  // Set up three server IBANs with empty `prefix`.
  Suggestion server_iban_suggestion_0 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12345, /*prefix=*/"", /*suffix=*/"8009",
      /*length=*/21, /*nickname=*/"My IBAN"));
  Suggestion server_iban_suggestion_1 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12346, /*prefix=*/"", /*suffix=*/"8009",
      /*length=*/24, /*nickname=*/"My doctor's IBAN"));
  Suggestion server_iban_suggestion_2 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12347, /*prefix=*/"", /*suffix=*/"9123",
      /*length=*/28, /*nickname=*/"My sister's IBAN"));
  Suggestion separator_suggestion = SetUpSeparator();
  Suggestion footer_suggestion = SetUpFooterManagePaymentMethods();

  AutofillField test_field;
  test_field.value = u"AB5678";
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Expect that no suggestions are returned because length of input field
  // exceeds `kFieldLengthLimitOnServerIbanSuggestion`.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));
}

TEST_F(IbanManagerTest, DoesNotShowIbansForBlockedWebsite) {
  SetUpLocalIban(test::kIbanValue, kNickname_0);
  AutofillField test_field;
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Setting up mock to verify that suggestions returning is not triggered if
  // the website is blocked.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client_.GetAutofillOptimizationGuide()),
          ShouldBlockSingleFieldSuggestions)
      .WillByDefault(testing::Return(true));

  // Simulate request for suggestions.
  EXPECT_FALSE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));
}

// Test that suggestions are returned on platforms that don't have an
// AutofillOptimizationGuide. Having no AutofillOptimizationGuide means that
// suggestions cannot and will not be blocked.
TEST_F(IbanManagerTest, ShowsIbanSuggestions_OptimizationGuideNotPresent) {
  Suggestion iban_suggestion_0 =
      GetSuggestionForIban(SetUpLocalIban(test::kIbanValue, kNickname_0));
  AutofillField test_field;
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Delete the AutofillOptimizationGuide.
  autofill_client_.ResetAutofillOptimizationGuide();

  // Setting up mock to verify that the handler is returned a list of
  // IBAN-based suggestions.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(test_field.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  testing::IsSupersetOf(
                      {MatchesTextAndPopupItemId(iban_suggestion_0)})));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));
}

TEST_F(IbanManagerTest, NotIbanFieldFocused_NoSuggestionsShown) {
  SetUpLocalIban(test::kIbanValue, kNickname_0);

  AutofillField test_field;
  test_field.value = base::UTF8ToUTF16(std::string(test::kIbanValue));
  // Set the field type to any type than "IBAN_VALUE".
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(
      test_field, CREDIT_CARD_VERIFICATION_CODE);

  // Setting up mock to verify that suggestions returning is not triggered if
  // we are not focused on an IBAN field.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);

  // Simulate request for suggestions.
  EXPECT_FALSE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));
}

// Tests that when showing IBAN suggestions is allowed by the site-specific
// blocklist, appropriate metrics are logged.
TEST_F(IbanManagerTest, Metrics_Suggestions_Allowed) {
  base::HistogramTester histogram_tester;
  SetUpLocalIban(test::kIbanValue, kNickname_0);

  AutofillField test_field;
  test_field.unique_renderer_id = test::MakeFieldRendererId();
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);
  // Simulate request for suggestions.
  // TODO: handle return value.
  std::ignore = iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, base::DoNothing(), context);

  histogram_tester.ExpectUniqueSample(
      "Autofill.Iban.ShowSuggestionsBlocklistDecision",
      autofill_metrics::IbanSuggestionBlockListStatus::kAllowed, 1);
}

// Tests that when showing IBAN suggestions is blocked by the site-specific
// blocklist, appropriate metrics are logged.
TEST_F(IbanManagerTest, Metrics_Suggestions_Blocked) {
  base::HistogramTester histogram_tester;
  SetUpLocalIban(test::kIbanValue, kNickname_0);
  AutofillField test_field;
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Setting up mock to verify that suggestions returning is not triggered if
  // the website is blocked.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client_.GetAutofillOptimizationGuide()),
          ShouldBlockSingleFieldSuggestions)
      .WillByDefault(testing::Return(true));
  // Simulate request for suggestions.
  // TODO: handle return value.
  std::ignore = iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(),
      /*context=*/context);

  histogram_tester.ExpectUniqueSample(
      "Autofill.Iban.ShowSuggestionsBlocklistDecision",
      autofill_metrics::IbanSuggestionBlockListStatus::kBlocked, 1);
}

// Tests that when showing IBAN suggestions and the site-specific blocklist is
// not available, appropriate metrics are logged.
TEST_F(IbanManagerTest, Metrics_Suggestions_BlocklistNotAccessible) {
  base::HistogramTester histogram_tester;
  SetUpLocalIban(test::kIbanValue, kNickname_0);
  AutofillField test_field;
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);
  // Delete the AutofillOptimizationGuide.
  autofill_client_.ResetAutofillOptimizationGuide();

  // Simulate request for suggestions.
  // TODO: handle return value.
  std::ignore = iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, base::DoNothing(),
      /*context=*/context);

  histogram_tester.ExpectUniqueSample(
      "Autofill.Iban.ShowSuggestionsBlocklistDecision",
      autofill_metrics::IbanSuggestionBlockListStatus::kBlocklistIsNotAvailable,
      1);
}

// Test that the metrics for IBAN-related suggestions shown and shown once are
// logged correctly.
TEST_F(IbanManagerTest, Metrics_SuggestionsShown) {
  base::HistogramTester histogram_tester;
  SetUpLocalIban(test::kIbanValue, kNickname_0);

  AutofillField test_field;
  test_field.unique_renderer_id = test::MakeFieldRendererId();
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Simulate request for suggestions.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));

  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.Iban.Suggestions"),
      BucketsAre(
          base::Bucket(
              autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionsShown, 2),
          base::Bucket(
              autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionsShownOnce,
              1)));
}

// Test that the metrics for IBAN-related suggestion selected and selected once
// are logged correctly.
TEST_F(IbanManagerTest, Metrics_SuggestionSelected) {
  base::HistogramTester histogram_tester;
  SetUpLocalIban(test::kIbanValue, kNickname_0);
  SetUpLocalIban(test::kIbanValue_1, kNickname_1);
  SetUpLocalIban(test::kIbanValue_2, "");

  AutofillField test_field;
  test_field.unique_renderer_id = test::MakeFieldRendererId();
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Simulate request for suggestions and select one suggested IBAN.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));
  iban_manager_.OnSingleFieldSuggestionSelected(u"", PopupItemId::kIbanEntry);

  histogram_tester.ExpectBucketCount(
      "Autofill.Iban.Suggestions",
      autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionSelected, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Iban.Suggestions",
      autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionSelectedOnce, 1);

  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));
  iban_manager_.OnSingleFieldSuggestionSelected(u"", PopupItemId::kIbanEntry);

  histogram_tester.ExpectBucketCount(
      "Autofill.Iban.Suggestions",
      autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionSelected, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Iban.Suggestions",
      autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionSelectedOnce, 1);
}

TEST_F(IbanManagerTest, Metrics_NoSuggestionShown) {
  base::HistogramTester histogram_tester;
  SetUpLocalIban(test::kIbanValue, kNickname_0);
  SetUpLocalIban(test::kIbanValue_1, kNickname_1);
  AutofillField test_field;
  // Input a prefix that does not have any matching IBAN value so that no IBAN
  // suggestions will be shown.
  test_field.value = u"XY";
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);

  // The suggestion handler should be triggered as some IBANs are available.
  // However, no suggestions are returned due to the prefix match requirement.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field,
      autofill_client_, mock_callback.Get(), context));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.Iban.Suggestions"),
      BucketsAre(
          base::Bucket(
              autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionsShown, 0),
          base::Bucket(
              autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionsShownOnce,
              0)));
}

}  // namespace autofill
