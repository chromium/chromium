// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/iban_manager.h"

#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/mock_autofill_optimization_guide.h"
#include "components/autofill/core/browser/payments_data_manager.h"
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
using testing::IsEmpty;
using testing::Truly;
using testing::UnorderedElementsAre;

namespace autofill {
namespace {

constexpr char kNickname_0[] = "Nickname 0";
constexpr char kNickname_1[] = "Nickname 1";
constexpr char16_t kIbanValue[] = u"FR7630006000011234567890189";

using MockSuggestionsReturnedCallback =
    base::MockCallback<SingleFieldFormFiller::OnSuggestionsReturnedCallback>;

class IbanManagerTest : public testing::Test {
 protected:
  IbanManagerTest() : iban_manager_(&personal_data_manager_) {}

  void SetUp() override {
    personal_data_manager_.test_payments_data_manager()
        .SetAutofillPaymentMethodsEnabled(true);
    original_resource_bundle_ =
        ui::ResourceBundle::SwapSharedInstanceForTesting(nullptr);
    form_structure_ = std::make_unique<FormStructure>(
        test::CreateTestIbanFormData(/*value=*/""));
    test_api(*form_structure_).SetFieldTypes({IBAN_VALUE});
    autofill_field_ = form_structure_->field(0);

    ui::ResourceBundle::InitSharedInstanceWithLocale(
        "en-US", &mock_resource_delegate_,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
    ON_CALL(mock_resource_delegate_, GetImageNamed(IDR_AUTOFILL_IBAN))
        .WillByDefault(testing::Return(gfx::test::CreateImage(100, 50)));

    ON_CALL(*autofill_client_.GetAutofillOptimizationGuide(),
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
    personal_data_manager_.payments_data_manager().AddAsLocalIban(iban);
    return iban;
  }

  // Sets up the TestPersonalDataManager with a server IBAN.
  Iban SetUpServerIban(int64_t instrument_id,
                       std::string_view prefix,
                       std::string_view suffix,
                       std::string_view nickname) {
    Iban iban{Iban::InstrumentId(instrument_id)};
    iban.set_prefix(base::UTF8ToUTF16(std::string(prefix)));
    iban.set_suffix(base::UTF8ToUTF16(std::string(suffix)));
    iban.set_nickname(base::UTF8ToUTF16(std::string(nickname)));
    personal_data_manager_.test_payments_data_manager().AddServerIban(iban);
    return iban;
  }

  // Get an IBAN suggestion with the given `iban`.
  Suggestion GetSuggestionForIban(const Iban& iban) {
    Suggestion iban_suggestion;
    const std::u16string iban_identifier =
        iban.GetIdentifierStringForAutofillDisplay();
    if constexpr (BUILDFLAG(IS_ANDROID)) {
      if (!iban.nickname().empty()) {
        iban_suggestion.main_text.value = iban.nickname();
        iban_suggestion.minor_text.value = iban_identifier;
      } else {
        iban_suggestion.main_text.value = iban_identifier;
      }
    } else {
      if (iban.nickname().empty()) {
        iban_suggestion.main_text = Suggestion::Text(
            iban_identifier, Suggestion::Text::IsPrimary(true));
      } else {
        iban_suggestion.main_text = Suggestion::Text(
            iban.nickname(), Suggestion::Text::IsPrimary(true));
        iban_suggestion.labels = {{Suggestion::Text(iban_identifier)}};
      }
    }

    iban_suggestion.type = SuggestionType::kIbanEntry;
    if (iban.record_type() == Iban::kServerIban) {
      iban_suggestion.payload =
          Suggestion::BackendId(Suggestion::InstrumentId(iban.instrument_id()));
    } else {
      iban_suggestion.payload =
          Suggestion::BackendId(Suggestion::Guid(iban.guid()));
    }
    return iban_suggestion;
  }

  Suggestion SetUpSeparator() {
    Suggestion separator;
    separator.type = SuggestionType::kSeparator;
    return separator;
  }

  Suggestion SetUpFooterManagePaymentMethods() {
    Suggestion footer_suggestion(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS));
    footer_suggestion.type = SuggestionType::kManageIban;
    footer_suggestion.icon = Suggestion::Icon::kSettings;
    return footer_suggestion;
  }

  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  TestPersonalDataManager personal_data_manager_;
  std::unique_ptr<FormStructure> form_structure_;
  // Owned by `form_structure_`.
  raw_ptr<AutofillField> autofill_field_;
  IbanManager iban_manager_;
  testing::NiceMock<ui::MockResourceBundleDelegate> mock_resource_delegate_;
  raw_ptr<ui::ResourceBundle> original_resource_bundle_;
};

MATCHER_P(MatchesTextAndSuggestionType, suggestion, "") {
  return arg.main_text == suggestion.main_text && arg.type == suggestion.type;
}

TEST_F(IbanManagerTest, ShowsAllIbanSuggestions) {
  personal_data_manager_.test_payments_data_manager()
      .SetAutofillWalletImportEnabled(true);
  Suggestion local_iban_suggestion_0 = GetSuggestionForIban(
      SetUpLocalIban("FR76 3000 6000 0112 3456 7890 189", kNickname_0));
  Suggestion local_iban_suggestion_1 = GetSuggestionForIban(
      SetUpLocalIban("CH56 0483 5012 3456 7800 9", kNickname_1));
  Suggestion server_iban_suggestion_0 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12345, /*prefix=*/"DE91", /*suffix=*/"6789",
      kNickname_0));
  Suggestion server_iban_suggestion_1 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12346, /*prefix=*/"BE71", /*suffix=*/"6769",
      kNickname_1));
  Suggestion separator_suggestion = SetUpSeparator();
  Suggestion footer_suggestion = SetUpFooterManagePaymentMethods();

  // Setting up mock to verify that the handler is returned a list of
  // IBAN-based suggestions.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(autofill_field_->global_id(),
                  testing::UnorderedElementsAre(
                      MatchesTextAndSuggestionType(local_iban_suggestion_0),
                      MatchesTextAndSuggestionType(local_iban_suggestion_1),
                      MatchesTextAndSuggestionType(server_iban_suggestion_0),
                      MatchesTextAndSuggestionType(server_iban_suggestion_1),
                      MatchesTextAndSuggestionType(separator_suggestion),
                      MatchesTextAndSuggestionType(footer_suggestion))));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));
}

TEST_F(IbanManagerTest, PaymentsAutofillEnabledPrefOff_NoIbanSuggestionsShown) {
  personal_data_manager_.test_payments_data_manager()
      .SetAutofillPaymentMethodsEnabled(false);
  GetSuggestionForIban(SetUpLocalIban(test::kIbanValue, kNickname_0));
  GetSuggestionForIban(SetUpLocalIban(test::kIbanValue_1, kNickname_1));

  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);

  // Because the "Save and autofill payment methods" toggle is off, the
  // suggestion handler should not be triggered.
  EXPECT_FALSE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));
}

TEST_F(IbanManagerTest, IbanSuggestions_SeparatorAndFooter) {
  Suggestion iban_suggestion_0 =
      GetSuggestionForIban(SetUpLocalIban(test::kIbanValue, kNickname_0));
  Suggestion iban_suggestion_1 = SetUpSeparator();
  Suggestion iban_suggestion_2 = SetUpFooterManagePaymentMethods();

  // Setting up mock to verify that the handler is returned IBAN-based
  // suggestions. A separator and the "Manage payment methods..." row should
  // also be returned.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(autofill_field_->global_id(),
                  testing::UnorderedElementsAre(
                      MatchesTextAndSuggestionType(iban_suggestion_0),
                      MatchesTextAndSuggestionType(iban_suggestion_1),
                      MatchesTextAndSuggestionType(iban_suggestion_2))));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));
}

TEST_F(IbanManagerTest,
       OnGetSingleFieldSuggestions_FieldEqualsLocalIban_NothingReturned) {
  Suggestion iban_suggestion_0 = GetSuggestionForIban(
      SetUpLocalIban("CH93 0076 2011 6238 5295 7", kNickname_0));
  Suggestion iban_suggestion_1 = GetSuggestionForIban(
      SetUpLocalIban("CH56 0483 5012 3456 7800 9", kNickname_1));

  autofill_field_->set_value(u"CH5604835012345678009");

  // The field contains value matches existing IBAN already, so check that we do
  // not return suggestions to the handler.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(autofill_field_->global_id(), IsEmpty()));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));
}

TEST_F(IbanManagerTest,
       OnGetSingleFieldSuggestions_LocalIbansMatchingPrefix_Shows) {
  Suggestion iban_suggestion_0 =
      GetSuggestionForIban(SetUpLocalIban(test::kIbanValue_1, kNickname_0));
  Suggestion iban_suggestion_1 =
      GetSuggestionForIban(SetUpLocalIban(test::kIbanValue_2, kNickname_1));
  Suggestion iban_suggestion_2 = SetUpSeparator();
  Suggestion iban_suggestion_3 = SetUpFooterManagePaymentMethods();

  autofill_field_->set_value(u"CH");

  // Setting up mock to verify that the handler is returned a list of
  // IBAN-based suggestions whose prefixes match `prefix_`. Both values should
  // be returned because they both start with CH56. Other than that, there are
  // one separator and one footer suggestion displayed.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(autofill_field_->global_id(),
                  testing::UnorderedElementsAre(
                      MatchesTextAndSuggestionType(iban_suggestion_0),
                      MatchesTextAndSuggestionType(iban_suggestion_1),
                      MatchesTextAndSuggestionType(iban_suggestion_2),
                      MatchesTextAndSuggestionType(iban_suggestion_3))));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));

  autofill_field_->set_value(u"CH5604");

  // Setting up mock to verify that the handler is returned only one
  // IBAN-based suggestion whose prefix matches `prefix_`. Only one of the two
  // IBANs should stay because the other will be filtered out. Other than that,
  // there are one separator and one footer suggestion displayed.
  EXPECT_CALL(mock_callback,
              Run(autofill_field_->global_id(),
                  testing::UnorderedElementsAre(
                      MatchesTextAndSuggestionType(iban_suggestion_0),
                      MatchesTextAndSuggestionType(iban_suggestion_2),
                      MatchesTextAndSuggestionType(iban_suggestion_3))));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));

  autofill_field_->set_value(u"AB56");

  // Verify that the handler is not triggered because no IBAN suggestions match
  // the given prefix.
  EXPECT_CALL(mock_callback, Run(autofill_field_->global_id(), IsEmpty()));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));
}

// Test that when the input text field is shorter than IBAN's prefix, all IBANs
// with matching prefixes should be returned.
TEST_F(IbanManagerTest,
       OnGetSingleFieldSuggestions_ServerIbansMatchingPrefix_Shows_All) {
  personal_data_manager_.test_payments_data_manager()
      .SetAutofillWalletImportEnabled(true);
  // Set up two server IBANs with different prefixes except for the first two
  // characters, and with same suffixes and lengths.
  Suggestion server_iban_suggestion_0 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12345, /*prefix=*/"CH56", /*suffix=*/"8009",
      /*nickname=*/"My IBAN"));
  Suggestion server_iban_suggestion_1 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12346, /*prefix=*/"CH78", /*suffix=*/"8009",
      /*nickname=*/"My doctor's IBAN"));
  Suggestion separator_suggestion = SetUpSeparator();
  Suggestion footer_suggestion = SetUpFooterManagePaymentMethods();

  autofill_field_->set_value(u"CH");

  // Expect that a list of IBAN suggestions whose prefixes match input field is
  // returned because they both start with "CH". Other than that, there is one
  // separator and one footer suggestion displayed.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(autofill_field_->global_id(),
                  testing::UnorderedElementsAre(
                      MatchesTextAndSuggestionType(server_iban_suggestion_0),
                      MatchesTextAndSuggestionType(server_iban_suggestion_1),
                      MatchesTextAndSuggestionType(separator_suggestion),
                      MatchesTextAndSuggestionType(footer_suggestion))));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));
}

// Test that when the input text field is shorter than IBAN's prefix, only IBANs
// with matching prefixes should be returned.
TEST_F(IbanManagerTest,
       OnGetSingleFieldSuggestions_ServerIbansMatchingPrefix_Shows_Some) {
  personal_data_manager_.test_payments_data_manager()
      .SetAutofillWalletImportEnabled(true);
  // Set up two server IBANs with different prefixes except for the first two
  // characters, and with same suffixes and lengths.
  Suggestion server_iban_suggestion_0 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12345, /*prefix=*/"CH56", /*suffix=*/"8009",
      /*nickname=*/"My IBAN"));
  Suggestion server_iban_suggestion_1 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12346, /*prefix=*/"CH78", /*suffix=*/"8009",
      /*nickname=*/"My doctor's IBAN"));
  Suggestion separator_suggestion = SetUpSeparator();
  Suggestion footer_suggestion = SetUpFooterManagePaymentMethods();

  autofill_field_->set_value(u"CH567");

  // Expect that only one of the two IBANs should stay because the other will be
  // filtered out. Other than that, there is one separator and one footer
  // suggestion displayed.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(autofill_field_->global_id(),
                  testing::UnorderedElementsAre(
                      MatchesTextAndSuggestionType(server_iban_suggestion_0),
                      MatchesTextAndSuggestionType(separator_suggestion),
                      MatchesTextAndSuggestionType(footer_suggestion))));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));
}

// Test that when there is no prefix present, all server IBANs should be
// recommended when the character count of the input text is less than
// `kFieldLengthLimitOnServerIbanSuggestion`.
TEST_F(
    IbanManagerTest,
    OnGetSingleFieldSuggestions_ServerIbansLackingPrefix_ShowsIfFewCharsInField) {
  personal_data_manager_.test_payments_data_manager()
      .SetAutofillWalletImportEnabled(true);
  // Set up three server IBANs with empty `prefix`.
  Suggestion server_iban_suggestion_0 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12345, /*prefix=*/"", /*suffix=*/"8009",
      /*nickname=*/"My IBAN"));
  Suggestion server_iban_suggestion_1 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12346, /*prefix=*/"", /*suffix=*/"8009",
      /*nickname=*/"My doctor's IBAN"));
  Suggestion server_iban_suggestion_2 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12347, /*prefix=*/"", /*suffix=*/"9123",
      /*nickname=*/"My sister's IBAN"));
  Suggestion separator_suggestion = SetUpSeparator();
  Suggestion footer_suggestion = SetUpFooterManagePaymentMethods();

  // Expect that all server IBANs are returned.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(autofill_field_->global_id(),
                  testing::UnorderedElementsAre(
                      MatchesTextAndSuggestionType(server_iban_suggestion_0),
                      MatchesTextAndSuggestionType(server_iban_suggestion_1),
                      MatchesTextAndSuggestionType(server_iban_suggestion_2),
                      MatchesTextAndSuggestionType(separator_suggestion),
                      MatchesTextAndSuggestionType(footer_suggestion))));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));

  autofill_field_->set_value(u"AB567");

  // Expect that all server IBANs are returned because the count of input
  // character is less than `kFieldLengthLimitOnServerIbanSuggestion`.
  EXPECT_CALL(mock_callback,
              Run(autofill_field_->global_id(),
                  testing::UnorderedElementsAre(
                      MatchesTextAndSuggestionType(server_iban_suggestion_0),
                      MatchesTextAndSuggestionType(server_iban_suggestion_1),
                      MatchesTextAndSuggestionType(server_iban_suggestion_2),
                      MatchesTextAndSuggestionType(separator_suggestion),
                      MatchesTextAndSuggestionType(footer_suggestion))));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));
}

// Test that when there is no prefix present, no server IBANs should be
// recommended if the length equals or exceeds
// `kFieldLengthLimitOnServerIbanSuggestion`.
TEST_F(
    IbanManagerTest,
    OnGetSingleFieldSuggestions_ServerIbansLackingPrefix_HidesIfManyCharsInField) {
  personal_data_manager_.test_payments_data_manager()
      .SetAutofillWalletImportEnabled(true);
  // Set up three server IBANs with empty `prefix`.
  Suggestion server_iban_suggestion_0 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12345, /*prefix=*/"", /*suffix=*/"8009",
      /*nickname=*/"My IBAN"));
  Suggestion server_iban_suggestion_1 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12346, /*prefix=*/"", /*suffix=*/"8009",
      /*nickname=*/"My doctor's IBAN"));
  Suggestion server_iban_suggestion_2 = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12347, /*prefix=*/"", /*suffix=*/"9123",
      /*nickname=*/"My sister's IBAN"));
  Suggestion separator_suggestion = SetUpSeparator();
  Suggestion footer_suggestion = SetUpFooterManagePaymentMethods();

  autofill_field_->set_value(u"AB5678");

  // Expect that no suggestions are returned because length of input field
  // exceeds `kFieldLengthLimitOnServerIbanSuggestion`.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(autofill_field_->global_id(), IsEmpty()));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));
}

TEST_F(IbanManagerTest, DoesNotShowIbansForBlockedWebsite) {
  SetUpLocalIban(test::kIbanValue, kNickname_0);

  // Setting up mock to verify that suggestions returning is not triggered if
  // the website is blocked.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);
  ON_CALL(*autofill_client_.GetAutofillOptimizationGuide(),
          ShouldBlockSingleFieldSuggestions)
      .WillByDefault(testing::Return(true));

  // Simulate request for suggestions.
  EXPECT_FALSE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));
}

// Test that suggestions are returned on platforms that don't have an
// AutofillOptimizationGuide. Having no AutofillOptimizationGuide means that
// suggestions cannot and will not be blocked.
TEST_F(IbanManagerTest, ShowsIbanSuggestions_OptimizationGuideNotPresent) {
  Suggestion iban_suggestion_0 =
      GetSuggestionForIban(SetUpLocalIban(test::kIbanValue, kNickname_0));

  // Delete the AutofillOptimizationGuide.
  autofill_client_.ResetAutofillOptimizationGuide();

  // Setting up mock to verify that the handler is returned a list of
  // IBAN-based suggestions.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(autofill_field_->global_id(),
                  testing::IsSupersetOf(
                      {MatchesTextAndSuggestionType(iban_suggestion_0)})));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));
}

TEST_F(IbanManagerTest, NotIbanFieldFocused_NoSuggestionsShown) {
  SetUpLocalIban(test::kIbanValue, kNickname_0);

  autofill_field_->set_value(std::u16string(test::kIbanValue16));
  // Set the field type to any type than "IBAN_VALUE".
  autofill_field_->SetTypeTo(AutofillType(CREDIT_CARD_VERIFICATION_CODE));

  // Setting up mock to verify that suggestions returning is not triggered if
  // we are not focused on an IBAN field.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);

  // Simulate request for suggestions.
  EXPECT_FALSE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));
}

// Tests that when showing IBAN suggestions is allowed by the site-specific
// blocklist, appropriate metrics are logged.
TEST_F(IbanManagerTest, Metrics_Suggestions_Allowed) {
  base::HistogramTester histogram_tester;
  SetUpLocalIban(test::kIbanValue, kNickname_0);

  autofill_field_->set_renderer_id(test::MakeFieldRendererId());
  // Simulate request for suggestions.
  // TODO: handle return value.
  std::ignore = iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, base::DoNothing());

  histogram_tester.ExpectUniqueSample(
      "Autofill.Iban.ShowSuggestionsBlocklistDecision",
      autofill_metrics::IbanSuggestionBlockListStatus::kAllowed, 1);
}

// Tests that when showing IBAN suggestions is blocked by the site-specific
// blocklist, appropriate metrics are logged.
TEST_F(IbanManagerTest, Metrics_Suggestions_Blocked) {
  base::HistogramTester histogram_tester;
  SetUpLocalIban(test::kIbanValue, kNickname_0);

  // Setting up mock to verify that suggestions returning is not triggered if
  // the website is blocked.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);
  ON_CALL(*autofill_client_.GetAutofillOptimizationGuide(),
          ShouldBlockSingleFieldSuggestions)
      .WillByDefault(testing::Return(true));
  // Simulate request for suggestions.
  // TODO: handle return value.
  std::ignore = iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get());

  histogram_tester.ExpectUniqueSample(
      "Autofill.Iban.ShowSuggestionsBlocklistDecision",
      autofill_metrics::IbanSuggestionBlockListStatus::kBlocked, 1);
}

// Tests that when showing IBAN suggestions and the site-specific blocklist is
// not available, appropriate metrics are logged.
TEST_F(IbanManagerTest, Metrics_Suggestions_BlocklistNotAccessible) {
  base::HistogramTester histogram_tester;
  SetUpLocalIban(test::kIbanValue, kNickname_0);
  // Delete the AutofillOptimizationGuide.
  autofill_client_.ResetAutofillOptimizationGuide();

  // Simulate request for suggestions.
  // TODO: handle return value.
  std::ignore = iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, base::DoNothing());

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

  autofill_field_->set_renderer_id(test::MakeFieldRendererId());

  // Simulate request for suggestions.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));

  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.Iban.Suggestions"),
      BucketsAre(
          base::Bucket(
              autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionsShown, 2),
          base::Bucket(
              autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionsShownOnce,
              1)));
}

// Test that the metrics for local IBAN suggestion selected (once and total
// count) are logged correctly.
TEST_F(IbanManagerTest, Metrics_LocalIbanSuggestionSelected) {
  base::HistogramTester histogram_tester;
  SetUpLocalIban(test::kIbanValue, kNickname_0);
  SetUpLocalIban(test::kIbanValue_1, kNickname_1);
  SetUpLocalIban(test::kIbanValue_2, "");

  autofill_field_->set_renderer_id(test::MakeFieldRendererId());

  // Simulate request for suggestions and select one suggested IBAN.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));
  Suggestion suggestion(kIbanValue, SuggestionType::kIbanEntry);
  iban_manager_.OnSingleFieldSuggestionSelected(suggestion);

  histogram_tester.ExpectBucketCount(
      "Autofill.Iban.Suggestions",
      autofill_metrics::IbanSuggestionsEvent::kLocalIbanSuggestionSelected, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Iban.Suggestions",
      autofill_metrics::IbanSuggestionsEvent::kLocalIbanSuggestionSelectedOnce,
      1);

  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));
  iban_manager_.OnSingleFieldSuggestionSelected(suggestion);

  histogram_tester.ExpectBucketCount(
      "Autofill.Iban.Suggestions",
      autofill_metrics::IbanSuggestionsEvent::kLocalIbanSuggestionSelected, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Iban.Suggestions",
      autofill_metrics::IbanSuggestionsEvent::kLocalIbanSuggestionSelectedOnce,
      1);
}

// Test that the metrics for server IBAN suggestion selected (once and total
// count) is logged correctly.
TEST_F(IbanManagerTest, Metrics_ServerIbanSuggestionSelected) {
  base::HistogramTester histogram_tester;
  personal_data_manager_.test_payments_data_manager()
      .SetAutofillWalletImportEnabled(true);
  Suggestion suggestion = GetSuggestionForIban(SetUpServerIban(
      /*instrument_id=*/12345, /*prefix=*/"DE", /*suffix=*/"6789",
      kNickname_0));

  autofill_field_->set_renderer_id(test::MakeFieldRendererId());

  // Simulate request for suggestions and select one suggested IBAN.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));
  iban_manager_.OnSingleFieldSuggestionSelected(suggestion);

  histogram_tester.ExpectBucketCount(
      "Autofill.Iban.Suggestions",
      autofill_metrics::IbanSuggestionsEvent::kServerIbanSuggestionSelected, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Iban.Suggestions",
      autofill_metrics::IbanSuggestionsEvent::kServerIbanSuggestionSelectedOnce,
      1);

  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));
  iban_manager_.OnSingleFieldSuggestionSelected(suggestion);

  histogram_tester.ExpectBucketCount(
      "Autofill.Iban.Suggestions",
      autofill_metrics::IbanSuggestionsEvent::kServerIbanSuggestionSelected, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Iban.Suggestions",
      autofill_metrics::IbanSuggestionsEvent::kServerIbanSuggestionSelectedOnce,
      1);
}

TEST_F(IbanManagerTest, Metrics_SuggestionSelected_CountryOfSelectedIban) {
  base::HistogramTester histogram_tester;
  // Simulate selecting one suggested IBAN.
  Suggestion suggestion(kIbanValue, SuggestionType::kIbanEntry);
  iban_manager_.OnSingleFieldSuggestionSelected(suggestion);

  histogram_tester.ExpectUniqueSample("Autofill.Iban.CountryOfSelectedIban",
                                      Iban::IbanSupportedCountry::kFR, 1);
}

TEST_F(IbanManagerTest, Metrics_NoSuggestionShown) {
  base::HistogramTester histogram_tester;
  SetUpLocalIban(test::kIbanValue, kNickname_0);
  SetUpLocalIban(test::kIbanValue_1, kNickname_1);
  // Input a prefix that does not have any matching IBAN value so that no IBAN
  // suggestions will be shown.
  autofill_field_->set_value(u"XY");

  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(autofill_field_->global_id(), IsEmpty()));

  // The suggestion handler should be triggered as some IBANs are available.
  // However, no suggestions are returned due to the prefix match requirement.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      form_structure_.get(), *autofill_field_, autofill_field_,
      autofill_client_, mock_callback.Get()));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.Iban.Suggestions"),
      BucketsAre(
          base::Bucket(
              autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionsShown, 0),
          base::Bucket(
              autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionsShownOnce,
              0)));
}

}  // namespace
}  // namespace autofill
