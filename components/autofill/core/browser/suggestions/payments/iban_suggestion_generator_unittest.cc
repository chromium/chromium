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
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
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
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Matcher;

constexpr char kNickname_0[] = "Nickname 0";
constexpr char kNickname_1[] = "Nickname 1";

Matcher<Suggestion> EqualsIbanSuggestion(
    const std::u16string& identifier_string,
    const Suggestion::Payload& payload,
    const std::u16string& nickname) {
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    if (nickname.empty()) {
      return AllOf(
          Field(&Suggestion::type, SuggestionType::kIbanEntry),
          Field(&Suggestion::main_text, Suggestion::Text(identifier_string)),
          Field(&Suggestion::payload, payload));
    }
    return AllOf(Field(&Suggestion::type, SuggestionType::kIbanEntry),
                 Field(&Suggestion::main_text, Suggestion::Text(nickname)),
                 Field(&Suggestion::minor_texts,
                       std::vector<Suggestion::Text>{
                           Suggestion::Text(identifier_string)}),
                 Field(&Suggestion::payload, payload));
  }
  if (nickname.empty()) {
    return AllOf(Field(&Suggestion::type, SuggestionType::kIbanEntry),
                 Field(&Suggestion::main_text,
                       Suggestion::Text(identifier_string,
                                        Suggestion::Text::IsPrimary(true))),
                 Field(&Suggestion::payload, payload));
  }
  return AllOf(
      Field(&Suggestion::type, SuggestionType::kIbanEntry),
      Field(&Suggestion::main_text,
            Suggestion::Text(nickname, Suggestion::Text::IsPrimary(true))),
      Field(&Suggestion::payload, payload),
      Field(&Suggestion::labels, std::vector<std::vector<Suggestion::Text>>{
                                     {Suggestion::Text(identifier_string)}}));
}

class IbanSuggestionGeneratorTest : public testing::Test {
 protected:
  IbanSuggestionGeneratorTest() {
    feature_list_.InitWithFeatureStates(
        {{features::kAutofillNewSuggestionGeneration, true}});
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

  std::vector<Suggestion> GetSuggestionsForIbans(
      const std::vector<Iban>& ibans) {
    IbanSuggestionGenerator generator;
    std::vector<Suggestion> suggestions;

    auto on_suggestions_generated =
        [&suggestions](
            SuggestionGenerator::ReturnedSuggestions returned_suggestions) {
          suggestions = returned_suggestions.second;
        };

    std::vector<SuggestionGenerator::SuggestionData> suggestion_data(
        ibans.begin(), ibans.end());
    base::flat_map<SuggestionGenerator::SuggestionDataSource,
                   std::vector<SuggestionGenerator::SuggestionData>>
        fetched_data = {{SuggestionGenerator::SuggestionDataSource::kIban,
                         std::move(suggestion_data)}};
    generator.GenerateSuggestions(form().ToFormData(), field(), &form(),
                                  &field(), client(), fetched_data,
                                  on_suggestions_generated);
    return suggestions;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<PrefService> prefs_;
  std::unique_ptr<FormStructure> form_structure_;
  base::test::ScopedFeatureList feature_list_;
};

MATCHER_P(MatchesTextAndSuggestionType, suggestion, "") {
  return arg.main_text == suggestion.main_text && arg.type == suggestion.type;
}

// Checks that all Ibans are returned as suggestion data, and used for
// generating suggestions.
TEST_F(IbanSuggestionGeneratorTest, GeneratesIbanSuggestions) {
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
      saved_callback_argument;

  EXPECT_CALL(
      suggestion_data_callback,
      Run(testing::Pair(SuggestionGenerator::SuggestionDataSource::kIban,
                        testing::SizeIs(4))))
      .WillOnce(testing::SaveArg<0>(&saved_callback_argument));
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
                                client(), {saved_callback_argument},
                                suggestions_generated_callback.Get());
  task_environment().RunUntilIdle();
}

TEST_F(IbanSuggestionGeneratorTest, GetLocalIbanSuggestions) {
  auto MakeLocalIban = [](const std::u16string& value,
                          const std::u16string& nickname) {
    Iban iban(Iban::Guid(base::Uuid::GenerateRandomV4().AsLowercaseString()));
    iban.set_value(value);
    if (!nickname.empty()) {
      iban.set_nickname(nickname);
    }
    return iban;
  };
  Iban iban0 =
      MakeLocalIban(u"CH56 0483 5012 3456 7800 9", u"My doctor's IBAN");
  Iban iban1 =
      MakeLocalIban(u"DE91 1000 0000 0123 4567 89", u"My brother's IBAN");
  Iban iban2 =
      MakeLocalIban(u"GR96 0810 0010 0000 0123 4567 890", u"My teacher's IBAN");
  Iban iban3 = MakeLocalIban(u"PK70 BANK 0000 1234 5678 9000", u"");

  std::vector<Suggestion> iban_suggestions =
      GetSuggestionsForIbans({iban0, iban1, iban2, iban3});

  // There are 6 suggestions, 4 for IBAN suggestions, followed by a separator,
  // and followed by "Manage payment methods..." which redirects to the Chrome
  // payment methods settings page.
  ASSERT_EQ(iban_suggestions.size(), 6u);

  EXPECT_THAT(
      iban_suggestions[0],
      EqualsIbanSuggestion(iban0.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::Guid(iban0.guid()), iban0.nickname()));

  EXPECT_THAT(
      iban_suggestions[1],
      EqualsIbanSuggestion(iban1.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::Guid(iban1.guid()), iban1.nickname()));

  EXPECT_THAT(
      iban_suggestions[2],
      EqualsIbanSuggestion(iban2.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::Guid(iban2.guid()), iban2.nickname()));

  EXPECT_THAT(
      iban_suggestions[3],
      EqualsIbanSuggestion(iban3.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::Guid(iban3.guid()), iban3.nickname()));

  EXPECT_EQ(iban_suggestions[4].type, SuggestionType::kSeparator);

  EXPECT_EQ(iban_suggestions[5].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS));
  EXPECT_EQ(iban_suggestions[5].type, SuggestionType::kManageIban);
}

TEST_F(IbanSuggestionGeneratorTest, GetServerIbanSuggestions) {
  Iban server_iban1 = test::GetServerIban();
  Iban server_iban2 = test::GetServerIban2();
  Iban server_iban3 = test::GetServerIban3();

  std::vector<Suggestion> iban_suggestions =
      GetSuggestionsForIbans({server_iban1, server_iban2, server_iban3});

  // There are 5 suggestions, 3 for IBAN suggestions, followed by a separator,
  // and followed by "Manage payment methods..." which redirects to the Chrome
  // payment methods settings page.
  ASSERT_EQ(iban_suggestions.size(), 5u);

  EXPECT_THAT(iban_suggestions[0],
              EqualsIbanSuggestion(
                  server_iban1.GetIdentifierStringForAutofillDisplay(),
                  Suggestion::InstrumentId(server_iban1.instrument_id()),
                  server_iban1.nickname()));

  EXPECT_THAT(iban_suggestions[1],
              EqualsIbanSuggestion(
                  server_iban2.GetIdentifierStringForAutofillDisplay(),
                  Suggestion::InstrumentId(server_iban2.instrument_id()),
                  server_iban2.nickname()));

  EXPECT_THAT(iban_suggestions[2],
              EqualsIbanSuggestion(
                  server_iban3.GetIdentifierStringForAutofillDisplay(),
                  Suggestion::InstrumentId(server_iban3.instrument_id()),
                  server_iban3.nickname()));

  EXPECT_EQ(iban_suggestions[3].type, SuggestionType::kSeparator);

  EXPECT_EQ(iban_suggestions[4].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS));
  EXPECT_EQ(iban_suggestions[4].type, SuggestionType::kManageIban);
}

TEST_F(IbanSuggestionGeneratorTest, GetLocalAndServerIbanSuggestions) {
  Iban server_iban1 = test::GetServerIban();
  Iban server_iban2 = test::GetServerIban2();
  Iban local_iban1 = test::GetLocalIban();

  std::vector<Suggestion> iban_suggestions =
      GetSuggestionsForIbans({server_iban1, server_iban2, local_iban1});

  // There are 5 suggestions, 3 for IBAN suggestions, followed by a separator,
  // and followed by "Manage payment methods..." which redirects to the Chrome
  // payment methods settings page.
  ASSERT_EQ(iban_suggestions.size(), 5u);

  EXPECT_THAT(iban_suggestions[0],
              EqualsIbanSuggestion(
                  server_iban1.GetIdentifierStringForAutofillDisplay(),
                  Suggestion::InstrumentId(server_iban1.instrument_id()),
                  server_iban1.nickname()));

  EXPECT_THAT(iban_suggestions[1],
              EqualsIbanSuggestion(
                  server_iban2.GetIdentifierStringForAutofillDisplay(),
                  Suggestion::InstrumentId(server_iban2.instrument_id()),
                  server_iban2.nickname()));

  EXPECT_THAT(
      iban_suggestions[2],
      EqualsIbanSuggestion(local_iban1.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::Guid(local_iban1.guid()),
                           local_iban1.nickname()));

  EXPECT_EQ(iban_suggestions[3].type, SuggestionType::kSeparator);

  EXPECT_EQ(iban_suggestions[4].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS));
  EXPECT_EQ(iban_suggestions[4].type, SuggestionType::kManageIban);
}

}  // namespace
}  // namespace autofill
