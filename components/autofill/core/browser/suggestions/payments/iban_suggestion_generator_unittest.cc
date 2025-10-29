// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/iban_suggestion_generator.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/payments/test/mock_iban_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace autofill {
namespace {

using ::testing::_;

constexpr char kNickname_0[] = "Nickname 0";
constexpr char kNickname_1[] = "Nickname 1";

class IbanSuggestionGeneratorTest : public testing::Test,
                                    public testing::WithParamInterface<bool> {
 protected:
  IbanSuggestionGeneratorTest() {
    feature_list_.InitWithFeatureStates(
        {{features::kAutofillEnableNewFopDisplayDesktop,
          IsNewFopDisplayEnabled()},
         {features::kAutofillNewSuggestionGeneration, true}});
    prefs_ = test::PrefServiceForTesting();
    payments_data_manager().SetAutofillPaymentMethodsEnabled(true);
    autofill_client_.set_payments_autofill_client(
        std::make_unique<payments::TestPaymentsAutofillClient>(
            &autofill_client_));
    form_structure_ = std::make_unique<FormStructure>(
        test::CreateTestIbanFormData(/*value=*/""));
    test_api(*form_structure_).SetFieldTypes({IBAN_VALUE});

    ON_CALL(*autofill_client_.GetAutofillOptimizationGuideDecider(),
            ShouldBlockSingleFieldSuggestions)
        .WillByDefault(testing::Return(false));
  }

  bool IsNewFopDisplayEnabled() const {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    return false;
#else
    return GetParam();
#endif
  }

  AutofillClient& client() { return autofill_client_; }
  FormStructure& form() { return *form_structure_; }
  AutofillField& field() { return *form_structure_->fields().front(); }
  PersonalDataManager& personal_data_manager() {
    return client().GetPersonalDataManager();
  }
  TestPaymentsDataManager& payments_data_manager() {
    return autofill_client_.GetPersonalDataManager()
        .test_payments_data_manager();
  }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  Iban SetUpLocalIban(std::string_view value, std::string_view nickname) {
    Iban iban;
    iban.set_value(base::UTF8ToUTF16(std::string(value)));
    iban.set_nickname(base::UTF8ToUTF16(std::string(nickname)));
    payments_data_manager().AddAsLocalIban(iban);
    iban.set_record_type(Iban::kLocalIban);
    return iban;
  }

  Iban SetUpServerIban(int64_t instrument_id,
                       std::string_view prefix,
                       std::string_view suffix,
                       std::string_view nickname) {
    Iban iban{Iban::InstrumentId(instrument_id)};
    iban.set_prefix(base::UTF8ToUTF16(std::string(prefix)));
    iban.set_suffix(base::UTF8ToUTF16(std::string(suffix)));
    iban.set_nickname(base::UTF8ToUTF16(std::string(nickname)));
    payments_data_manager().AddServerIban(iban);
    iban.set_record_type(Iban::kServerIban);
    return iban;
  }

  // Get an IBAN suggestion with the given `iban`.
  Suggestion GetSuggestionForIban(const Iban& iban) {
    Suggestion iban_suggestion(SuggestionType::kIbanEntry);
    const std::u16string iban_identifier =
        iban.GetIdentifierStringForAutofillDisplay();
#if BUILDFLAG(IS_ANDROID)
    if (!iban.nickname().empty()) {
      iban_suggestion.main_text.value = iban.nickname();
      iban_suggestion.minor_texts.emplace_back(iban_identifier);
    } else {
      iban_suggestion.main_text.value = iban_identifier;
    }
#else
    if (iban.nickname().empty()) {
      iban_suggestion.main_text =
          Suggestion::Text(iban_identifier, Suggestion::Text::IsPrimary(true));
    } else {
      iban_suggestion.main_text =
          Suggestion::Text(iban.nickname(), Suggestion::Text::IsPrimary(true));
      iban_suggestion.labels = {{Suggestion::Text(iban_identifier)}};
    }
#endif

    if (iban.record_type() == Iban::kServerIban) {
      iban_suggestion.payload = Suggestion::InstrumentId(iban.instrument_id());
    } else {
      iban_suggestion.payload = Suggestion::Guid(iban.guid());
    }
    return iban_suggestion;
  }

  Suggestion SetUpFooterManagePaymentMethods() {
    Suggestion footer_suggestion(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS),
        SuggestionType::kManageIban);
    footer_suggestion.icon = Suggestion::Icon::kSettings;
    return footer_suggestion;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<PrefService> prefs_;
  std::unique_ptr<FormStructure> form_structure_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(IbanSuggestionGeneratorTest,
                         IbanSuggestionGeneratorTest,
                         ::testing::Bool());

MATCHER_P(MatchesTextAndSuggestionType, suggestion, "") {
  return arg.main_text == suggestion.main_text && arg.type == suggestion.type;
}

// Checks that all Ibans are returned as suggestion data, and used for
// generating suggestions.
TEST_P(IbanSuggestionGeneratorTest, GeneratesIbanSuggestions) {
  payments_data_manager().SetAutofillWalletImportEnabled(true);
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
  Suggestion separator_suggestion(SuggestionType::kSeparator);
  Suggestion footer_suggestion = SetUpFooterManagePaymentMethods();

  base::MockCallback<base::OnceCallback<void(
      std::pair<SuggestionGenerator::SuggestionDataSource,
                std::vector<SuggestionGenerator::SuggestionData>>)>>
      suggestion_data_callback;
  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  IbanSuggestionGenerator generator;
  std::pair<SuggestionGenerator::SuggestionDataSource,
            std::vector<SuggestionGenerator::SuggestionData>>
      savedCallbackArgument;

  EXPECT_CALL(
      suggestion_data_callback,
      Run(testing::Pair(SuggestionGenerator::SuggestionDataSource::kIban,
                        testing::SizeIs(4))))
      .WillOnce(testing::SaveArg<0>(&savedCallbackArgument));
  generator.FetchSuggestionData(form().ToFormData(), field(), &form(), &field(),
                                client(), suggestion_data_callback.Get());

  EXPECT_CALL(suggestions_generated_callback,
              Run(testing::Pair(
                  FillingProduct::kIban,
                  testing::UnorderedElementsAre(
                      MatchesTextAndSuggestionType(local_iban_suggestion_0),
                      MatchesTextAndSuggestionType(local_iban_suggestion_1),
                      MatchesTextAndSuggestionType(server_iban_suggestion_0),
                      MatchesTextAndSuggestionType(server_iban_suggestion_1),
                      MatchesTextAndSuggestionType(separator_suggestion),
                      MatchesTextAndSuggestionType(footer_suggestion)))));
  generator.GenerateSuggestions(form().ToFormData(), field(), &form(), &field(),
                                client(), {savedCallbackArgument},
                                suggestions_generated_callback.Get());
  task_environment().RunUntilIdle();
}

}  // namespace
}  // namespace autofill
