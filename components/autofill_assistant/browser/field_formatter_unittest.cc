// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/field_formatter.h"

#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/browser/geo/mock_alternative_state_name_map_updater.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_assistant {
namespace field_formatter {
namespace {
const char kFakeUrl[] = "https://www.example.com";

using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::IsSupersetOf;
using ::testing::Not;
using ::testing::Pair;

void AddReplacement(ValueExpression::Chunk* chunk,
                    const std::string& match,
                    const std::string& replacement) {
  (*chunk->mutable_replacements())[match] = replacement;
}
}  // namespace

class FieldFormatterStateMapTest : public ::testing::Test {
 public:
  FieldFormatterStateMapTest() = default;

  void SetUp() override {
    feature_.InitAndEnableFeature(
        autofill::features::kAutofillUseAlternativeStateNameMap);

    autofill_client_.SetPrefs(autofill::test::PrefServiceForTesting());
    ASSERT_TRUE(data_install_dir_.CreateUniqueTempDir());
    personal_data_manager_.Init(/*profile_database=*/database_,
                                /*account_database=*/nullptr,
                                /*pref_service=*/autofill_client_.GetPrefs(),
                                /*local_state=*/autofill_client_.GetPrefs(),
                                /*identity_manager=*/nullptr,
                                /*history_service=*/nullptr,
                                /*strike_database=*/nullptr,
                                /*image_fetcher=*/nullptr,
                                /*is_off_the_record=*/false);
    alternative_state_name_map_updater_ =
        std::make_unique<autofill::AlternativeStateNameMapUpdater>(
            autofill_client_.GetPrefs(), &personal_data_manager_);
  }

  const base::FilePath& GetPath() const { return data_install_dir_.GetPath(); }

  void WritePathToPref(const base::FilePath& file_path) {
    autofill_client_.GetPrefs()->SetFilePath(
        autofill::prefs::kAutofillStatesDataDir, file_path);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  autofill::TestAutofillClient autofill_client_;
  scoped_refptr<autofill::AutofillWebDataService> database_;
  std::unique_ptr<autofill::AlternativeStateNameMapUpdater>
      alternative_state_name_map_updater_;
  base::ScopedTempDir data_install_dir_;
  base::test::ScopedFeatureList feature_;
  autofill::TestPersonalDataManager personal_data_manager_;
};

TEST_F(FieldFormatterStateMapTest, AlternativeStateNameForNonUS) {
  autofill::test::ClearAlternativeStateNameMapForTesting();
  WritePathToPref(GetPath());
  autofill::test::TestStateEntry test_state_entry;
  test_state_entry.canonical_name = "Bavaria";
  test_state_entry.abbreviations = {"Bay", "BY"};
  test_state_entry.alternative_names = {"Bayern"};
  base::WriteFile(
      GetPath().AppendASCII("DE"),
      autofill::test::CreateStatesProtoAsString("DE", {test_state_entry}));

  autofill::AutofillProfile alternative_state_map_profile;
  alternative_state_map_profile.SetInfo(autofill::ADDRESS_HOME_STATE,
                                        u"Bavaria", "en-US");
  alternative_state_map_profile.SetInfo(autofill::ADDRESS_HOME_COUNTRY, u"DE",
                                        "en-US");

  base::RunLoop run_loop;
  autofill::MockAlternativeStateNameMapUpdater
      mock_alternative_state_name_updater(run_loop.QuitClosure(),
                                          autofill_client_.GetPrefs(),
                                          &personal_data_manager_);
  personal_data_manager_.AddObserver(&mock_alternative_state_name_updater);
  personal_data_manager_.AddProfile(alternative_state_map_profile);
  run_loop.Run();
  personal_data_manager_.RemoveObserver(&mock_alternative_state_name_updater);

  EXPECT_FALSE(autofill::AlternativeStateNameMap::GetInstance()
                   ->IsLocalisedStateNamesMapEmpty());

  // State handling from abbreviation.
  autofill::AutofillProfile state_abbr_profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(&state_abbr_profile, "John", "", "Doe", "", "",
                                 "", "", "", "BY", "", "DE", "");
  EXPECT_THAT(CreateAutofillMappings(state_abbr_profile, "en-US"),
              IsSupersetOf({Pair(Key(34), "BY"), Pair(Key(-6), "Bavaria")}));

  // State handling from state name.
  autofill::AutofillProfile state_name_profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(&state_name_profile, "John", "", "Doe", "", "",
                                 "", "", "", "Bavaria", "", "DE", "");
  EXPECT_THAT(CreateAutofillMappings(state_name_profile, "en-US"),
              IsSupersetOf({Pair(Key(34), "BY"), Pair(Key(-6), "Bavaria")}));

  // State handling for invalid cases.
  autofill::test::SetProfileInfo(&state_name_profile, "John", "", "Doe", "", "",
                                 "", "", "", "Invalid", "", "DE", "");
  EXPECT_THAT(CreateAutofillMappings(state_name_profile, "en-US"),
              AllOf(Not(Contains(Pair(Key(34), _))),
                    Contains(Pair(Key(-6), "Invalid"))));
}

TEST_F(FieldFormatterStateMapTest,
       AlternativeStateNameDoesNotOverrideUSResult) {
  autofill::test::ClearAlternativeStateNameMapForTesting();
  WritePathToPref(GetPath());
  autofill::test::TestStateEntry test_state_entry;
  test_state_entry.canonical_name = "Chesapeake Bay State";
  test_state_entry.abbreviations = {"MD"};
  test_state_entry.alternative_names = {"Free State", "Maryland"};
  base::WriteFile(
      GetPath().AppendASCII("US"),
      autofill::test::CreateStatesProtoAsString("US", {test_state_entry}));

  autofill::AutofillProfile alternative_state_map_profile;
  alternative_state_map_profile.SetInfo(autofill::ADDRESS_HOME_STATE, u"MD",
                                        "en-US");
  alternative_state_map_profile.SetInfo(autofill::ADDRESS_HOME_COUNTRY, u"US",
                                        "en-US");

  base::RunLoop run_loop;
  autofill::MockAlternativeStateNameMapUpdater
      mock_alternative_state_name_updater(run_loop.QuitClosure(),
                                          autofill_client_.GetPrefs(),
                                          &personal_data_manager_);
  personal_data_manager_.AddObserver(&mock_alternative_state_name_updater);
  personal_data_manager_.AddProfile(alternative_state_map_profile);
  run_loop.Run();
  personal_data_manager_.RemoveObserver(&mock_alternative_state_name_updater);

  EXPECT_FALSE(autofill::AlternativeStateNameMap::GetInstance()
                   ->IsLocalisedStateNamesMapEmpty());

  // State handling from abbreviation.
  autofill::AutofillProfile state_abbr_profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(&state_abbr_profile, "John", "", "Doe", "", "",
                                 "", "", "", "MD", "", "US", "");
  EXPECT_THAT(CreateAutofillMappings(state_abbr_profile, "en-US"),
              IsSupersetOf({Pair(Key(34), "MD"), Pair(Key(-6), "Maryland")}));

  // State handling from state name.
  autofill::AutofillProfile state_name_profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(&state_name_profile, "John", "", "Doe", "", "",
                                 "", "", "", "Maryland", "", "US", "");
  EXPECT_THAT(CreateAutofillMappings(state_name_profile, "en-US"),
              IsSupersetOf({Pair(Key(34), "MD"), Pair(Key(-6), "Maryland")}));
}

class FieldFormatterStringTest : public ::testing::Test {
 public:
  FieldFormatterStringTest() = default;
  void SetUp() override {}

 protected:
  std::string GetKeyAsString(const Key& key) {
    if (key.string_key.has_value()) {
      return *key.string_key;
    }
    if (key.int_key.has_value()) {
      return base::NumberToString(*key.int_key);
    }
    NOTREACHED();
    return std::string();
  }

  // |FormatString| requires a base::flat_map<std::string, std::string>, while
  // |GetAutofillMappings| returns a base::flat_map<Key, std::string>. This
  // transforms the mappings from Key to std::string.
  template <typename T>
  base::flat_map<std::string, std::string> CreateAutofillTestMappings(
      const T& form_group) {
    auto autofill_mappings = CreateAutofillMappings(form_group, "en-US");
    std::vector<std::pair<std::string, std::string>> test_mappings;
    for (const auto& it : autofill_mappings) {
      test_mappings.emplace_back(GetKeyAsString(it.first), it.second);
    }
    return base::flat_map<std::string, std::string>(std::move(test_mappings));
  }
};

TEST_F(FieldFormatterStringTest, FormatString) {
  base::flat_map<std::string, std::string> mappings = {
      {"keyA", "valueA"},
      {"keyB", "valueB"},
      {"keyC", "valueC"},
      {"keyD", "$30.5"},
      {"keyE", "30.5$"}
      /* keyF does not exist */};

  EXPECT_EQ(*FormatString("", mappings), "");
  EXPECT_EQ(*FormatString("input", mappings), "input");
  EXPECT_EQ(*FormatString("prefix ${keyA}", mappings), "prefix valueA");
  EXPECT_EQ(*FormatString("prefix ${keyA}${keyB}${keyC} suffix", mappings),
            "prefix valueAvalueBvalueC suffix");
  EXPECT_EQ(*FormatString("keyA = ${keyA}", mappings), "keyA = valueA");
  EXPECT_EQ(*FormatString("Price: $${keyA}", mappings), "Price: $valueA");
  EXPECT_EQ(*FormatString("Price: ${keyD}", mappings), "Price: $30.5");
  EXPECT_EQ(*FormatString("Price: ${keyE}", mappings), "Price: 30.5$");
  EXPECT_EQ(FormatString("${keyF}", mappings), absl::nullopt);
  EXPECT_EQ(FormatString("${keyA}${keyF}", mappings), absl::nullopt);

  EXPECT_EQ(*FormatString("${keyF}", mappings, /*strict = */ false), "${keyF}");
  EXPECT_EQ(*FormatString("${keyA}${keyF}", mappings, /*strict = */ false),
            "valueA${keyF}");
  EXPECT_EQ(*FormatString("${keyF}${keyA}", mappings, /*strict = */ false),
            "${keyF}valueA");
}

TEST_F(FieldFormatterStringTest, AutofillProfile) {
  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(
      &profile, "John", "", "Doe", "editor@gmail.com", "", "203 Barfield Lane",
      "", "Mountain View", "CA", "94043", "US", "+12345678901");

  // NAME_FIRST NAME_LAST
  EXPECT_EQ(*FormatString("${3} ${5}", CreateAutofillTestMappings(profile)),
            "John Doe");

  // PHONE_HOME_COUNTRY_CODE, PHONE_HOME_CITY_CODE, PHONE_HOME_NUMBER
  EXPECT_EQ(*FormatString("(+${12}) (${11}) ${10}",
                          CreateAutofillTestMappings(profile)),
            "(+1) (234) 5678901");

  // Country and country code.
  EXPECT_EQ(*FormatString("${36}", CreateAutofillTestMappings(profile)),
            "United States");
  EXPECT_EQ(*FormatString("${-8}", CreateAutofillTestMappings(profile)), "US");

  // State handling from abbreviation.
  EXPECT_EQ(*FormatString("${34}", CreateAutofillTestMappings(profile)), "CA");
  EXPECT_EQ(*FormatString("${-6}", CreateAutofillTestMappings(profile)),
            "California");

  // State handling from state name.
  autofill::AutofillProfile state_name_profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(&state_name_profile, "John", "", "Doe", "", "",
                                 "", "", "", "California", "", "US", "");
  EXPECT_EQ(
      *FormatString("${34}", CreateAutofillTestMappings(state_name_profile)),
      "CA");
  EXPECT_EQ(
      *FormatString("${-6}", CreateAutofillTestMappings(state_name_profile)),
      "California");

  // Unknown state.
  autofill::AutofillProfile unknown_state_profile(base::GenerateGUID(),
                                                  kFakeUrl);
  autofill::test::SetProfileInfo(&unknown_state_profile, "John", "", "Doe", "",
                                 "", "", "", "", "XY", "", "US", "");
  EXPECT_EQ(
      FormatString("${34}", CreateAutofillTestMappings(unknown_state_profile)),
      absl::nullopt);
  EXPECT_EQ(
      FormatString("${-6}", CreateAutofillTestMappings(unknown_state_profile)),
      "XY");

  // UNKNOWN_TYPE
  EXPECT_EQ(FormatString("${1}", CreateAutofillTestMappings(profile)),
            absl::nullopt);
}

TEST_F(FieldFormatterStringTest, CreditCard) {
  autofill::CreditCard credit_card(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card, "John Doe",
                                    "4111 1111 1111 1111", "01", "2050", "");

  // CREDIT_CARD_NAME_FULL
  EXPECT_EQ(*FormatString("${51}", CreateAutofillTestMappings(credit_card)),
            "John Doe");

  // CREDIT_CARD_NUMBER
  EXPECT_EQ(*FormatString("${52}", CreateAutofillTestMappings(credit_card)),
            "4111111111111111");

  // CREDIT_CARD_NUMBER_LAST_FOUR_DIGITS
  EXPECT_EQ(
      *FormatString("**** ${-4}", CreateAutofillTestMappings(credit_card)),
      "**** 1111");

  // CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR
  EXPECT_EQ(
      *FormatString("${53}/${54}", CreateAutofillTestMappings(credit_card)),
      "01/50");

  // CREDIT_CARD_NETWORK, CREDIT_CARD_NETWORK_FOR_DISPLAY
  EXPECT_EQ(
      *FormatString("${-2} ${-5}", CreateAutofillTestMappings(credit_card)),
      "visa Visa");

  // CREDIT_CARD_NON_PADDED_EXP_MONTH
  EXPECT_EQ(*FormatString("${-7}", CreateAutofillTestMappings(credit_card)),
            "1");
}

TEST_F(FieldFormatterStringTest, SpecialCases) {
  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(
      &profile, "John", "", "Doe", "editor@gmail.com", "", "203 Barfield Lane",
      "", "Mountain View", "CA", "94043", "US", "+12345678901");

  EXPECT_EQ(*FormatString("", CreateAutofillTestMappings(profile)),
            std::string());
  EXPECT_EQ(*FormatString("${3}", CreateAutofillTestMappings(profile)), "John");
  EXPECT_EQ(FormatString("${-1}", CreateAutofillTestMappings(profile)),
            absl::nullopt);
  EXPECT_EQ(
      FormatString(
          "${" + base::NumberToString(autofill::MAX_VALID_FIELD_TYPE) + "}",
          CreateAutofillTestMappings(profile)),
      absl::nullopt);

  // Second {} is not prefixed with $.
  EXPECT_EQ(*FormatString("${3} {10}", CreateAutofillTestMappings(profile)),
            "John {10}");
}

TEST(FieldFormatterTest, FormatExpression) {
  base::flat_map<Key, std::string> mappings = {{Key(1), "valueA"},
                                               {Key(2), "val.ueB"}};
  std::string result;

  ValueExpression value_expression_1;
  value_expression_1.add_chunk()->set_text("text");
  value_expression_1.add_chunk()->set_text(" ");
  value_expression_1.add_chunk()->set_key(1);
  EXPECT_EQ(ACTION_APPLIED, FormatExpression(value_expression_1, mappings,
                                             /* quote_meta= */ false, &result)
                                .proto_status());
  EXPECT_EQ("text valueA", result);

  ValueExpression value_expression_2;
  value_expression_2.add_chunk()->set_text("^");
  value_expression_2.add_chunk()->set_key(2);
  value_expression_2.add_chunk()->set_text("$");
  EXPECT_EQ(ACTION_APPLIED, FormatExpression(value_expression_2, mappings,
                                             /* quote_meta= */ false, &result)
                                .proto_status());
  EXPECT_EQ("^val.ueB$", result);
  EXPECT_EQ(ACTION_APPLIED, FormatExpression(value_expression_2, mappings,
                                             /* quote_meta= */ true, &result)
                                .proto_status());
  EXPECT_EQ("^val\\.ueB$", result);

  ValueExpression value_expression_3;
  value_expression_3.add_chunk()->set_key(3);
  EXPECT_EQ(AUTOFILL_INFO_NOT_AVAILABLE,
            FormatExpression(value_expression_3, mappings,
                             /* quote_meta= */ false, &result)
                .proto_status());

  ValueExpression value_expression_4;
  EXPECT_EQ(ACTION_APPLIED, FormatExpression(value_expression_4, mappings,
                                             /* quote_meta= */ false, &result)
                                .proto_status());
  EXPECT_EQ(std::string(), result);
}

TEST(FieldFormatterTest, FormatExpressionWithReplacements) {
  base::flat_map<Key, std::string> mappings = {
      {Key(1), "A"}, {Key(2), "B"}, {Key(3), "+41"}};
  std::string result;

  ValueExpression value_expression;
  auto* static_chunk = value_expression.add_chunk();
  static_chunk->set_text("static");
  AddReplacement(static_chunk, "static", "replacement1");
  auto* match_chunk = value_expression.add_chunk();
  match_chunk->set_key(1);
  AddReplacement(match_chunk, "a", "replacement2");
  AddReplacement(match_chunk, "A", "replacement3");
  auto* no_match_chunk = value_expression.add_chunk();
  no_match_chunk->set_key(2);
  AddReplacement(no_match_chunk, "b", "replacement4");
  EXPECT_EQ(ACTION_APPLIED, FormatExpression(value_expression, mappings,
                                             /* quote_meta= */ false, &result)
                                .proto_status());
  EXPECT_EQ("replacement1replacement3B", result);

  ValueExpression value_expression_regexp;
  auto* regexp_chunk = value_expression_regexp.add_chunk();
  regexp_chunk->set_key(3);
  AddReplacement(regexp_chunk, "+41", "+0041");
  EXPECT_EQ(ACTION_APPLIED, FormatExpression(value_expression_regexp, mappings,
                                             /* quote_meta= */ false, &result)
                                .proto_status());
  EXPECT_EQ("+0041", result);
  EXPECT_EQ(ACTION_APPLIED, FormatExpression(value_expression_regexp, mappings,
                                             /* quote_meta= */ true, &result)
                                .proto_status());
  EXPECT_EQ("\\+41", result);
  AddReplacement(regexp_chunk, "\\+41", "\\+0041");
  EXPECT_EQ(ACTION_APPLIED, FormatExpression(value_expression_regexp, mappings,
                                             /* quote_meta= */ true, &result)
                                .proto_status());
  EXPECT_EQ("\\+0041", result);
}

TEST(FieldFormatterTest, FormatExpressionWithRegexpReplacement) {
  base::flat_map<Key, std::string> mappings = {
      {Key(1), "AA"}, {Key(2), "Bb"}, {Key(3), "Cc"}, {Key(4), "DD"}};
  std::string result;

  ValueExpression value_expression;
  auto* chunk = value_expression.add_chunk();
  // AA -> rA
  chunk->set_key(1);
  auto* case_insensitive_local = chunk->add_regexp_replacements();
  case_insensitive_local->mutable_text_filter()->set_re2("a");
  case_insensitive_local->mutable_text_filter()->set_case_sensitive(false);
  case_insensitive_local->set_global(false);
  case_insensitive_local->set_replacement("r");
  // Bb -> Br
  chunk = value_expression.add_chunk();
  chunk->set_key(2);
  auto* case_sensitive_local = chunk->add_regexp_replacements();
  case_sensitive_local->mutable_text_filter()->set_re2("b");
  case_sensitive_local->mutable_text_filter()->set_case_sensitive(true);
  case_sensitive_local->set_global(false);
  case_sensitive_local->set_replacement("r");
  // Cc -> rr
  chunk = value_expression.add_chunk();
  chunk->set_key(3);
  auto* case_insensitive_global = chunk->add_regexp_replacements();
  case_insensitive_global->mutable_text_filter()->set_re2("c");
  case_insensitive_global->mutable_text_filter()->set_case_sensitive(false);
  case_insensitive_global->set_global(true);
  case_insensitive_global->set_replacement("r");
  // DD -> rr
  chunk = value_expression.add_chunk();
  chunk->set_key(4);
  auto* case_insensitive_local_first = chunk->add_regexp_replacements();
  case_insensitive_local_first->mutable_text_filter()->set_re2("d");
  case_insensitive_local_first->mutable_text_filter()->set_case_sensitive(
      false);
  case_insensitive_local_first->set_global(false);
  case_insensitive_local_first->set_replacement("r");
  auto* case_insensitive_local_second = chunk->add_regexp_replacements();
  case_insensitive_local_second->mutable_text_filter()->set_re2("d");
  case_insensitive_local_second->mutable_text_filter()->set_case_sensitive(
      false);
  case_insensitive_local_second->set_global(false);
  case_insensitive_local_second->set_replacement("r");

  EXPECT_EQ(ACTION_APPLIED, FormatExpression(value_expression, mappings,
                                             /* quote_meta= */ false, &result)
                                .proto_status());
  EXPECT_EQ("rABrrrrr", result);
}

TEST(FieldFormatterTest, FormatExpressionWithRegexpReplacementCapturingGroups) {
  std::string result;

  ValueExpression value_expression;
  auto* chunk = value_expression.add_chunk();
  chunk->set_text("John Doe");
  auto* group_replacement = chunk->add_regexp_replacements();
  group_replacement->mutable_text_filter()->set_re2("(\\w+)\\s(\\w+)");
  group_replacement->set_replacement("\\2 \\1");

  EXPECT_EQ(ACTION_APPLIED,
            FormatExpression(value_expression,
                             /* mappings= */ base::flat_map<Key, std::string>(),
                             /* quote_meta= */ false, &result)
                .proto_status());
  EXPECT_EQ("Doe John", result);
}

TEST(FieldFormatterTest, FormatExpressionWithInvalidRegexpReplacement) {
  base::flat_map<Key, std::string> mappings = {{Key(1), "AA"}};
  std::string result;

  ValueExpression value_expression;
  auto* chunk = value_expression.add_chunk();
  // AA -> ?
  chunk->set_key(1);
  auto* invalid_replacement = chunk->add_regexp_replacements();
  invalid_replacement->mutable_text_filter()->set_re2("^*");
  invalid_replacement->mutable_text_filter()->set_case_sensitive(false);
  invalid_replacement->set_global(false);
  invalid_replacement->set_replacement("");

  EXPECT_EQ(ACTION_APPLIED, FormatExpression(value_expression, mappings,
                                             /* quote_meta= */ false, &result)
                                .proto_status());
  EXPECT_EQ("AA", result);
}

TEST(FieldFormatterTest, FormatExpressionWithMemoryKey) {
  base::flat_map<Key, std::string> mappings = {{Key("_var0"), "valueA"},
                                               {Key("_var1"), "val.ueB"}};
  std::string result;

  ValueExpression value_expression;
  value_expression.add_chunk()->set_memory_key("_var0");
  value_expression.add_chunk()->set_text(" ");
  value_expression.add_chunk()->set_memory_key("_var1");
  EXPECT_EQ(ACTION_APPLIED, FormatExpression(value_expression, mappings,
                                             /* quote_meta= */ false, &result)
                                .proto_status());
  EXPECT_EQ("valueA val.ueB", result);

  ValueExpression value_expression_regexp;
  value_expression_regexp.add_chunk()->set_memory_key("_var0");
  value_expression_regexp.add_chunk()->set_text(" ");
  value_expression_regexp.add_chunk()->set_memory_key("_var1");
  EXPECT_EQ(ACTION_APPLIED, FormatExpression(value_expression_regexp, mappings,
                                             /* quote_meta= */ true, &result)
                                .proto_status());
  EXPECT_EQ("valueA val\\.ueB", result);

  ValueExpression value_expression_not_found;
  value_expression_regexp.add_chunk()->set_memory_key("_var2");
  EXPECT_EQ(CLIENT_MEMORY_KEY_NOT_AVAILABLE,
            FormatExpression(value_expression_regexp, mappings,
                             /* quote_meta= */ true, &result)
                .proto_status());
}

TEST(FieldFormatterTest, NoKeyConflicts) {
  base::flat_map<Key, std::string> mappings = {{Key(1), "a"}, {Key("1"), "b"}};
  std::string result;

  ValueExpression value_expression;
  value_expression.add_chunk()->set_key(1);
  value_expression.add_chunk()->set_text(" ");
  value_expression.add_chunk()->set_memory_key("1");
  EXPECT_EQ(ACTION_APPLIED, FormatExpression(value_expression, mappings,
                                             /* quote_meta= */ false, &result)
                                .proto_status());
  EXPECT_EQ("a b", result);
}

TEST(FieldFormatterTest, GetHumanReadableValueExpression) {
  ValueExpression value_expression;
  value_expression.add_chunk()->set_text("+");
  value_expression.add_chunk()->set_key(1);
  value_expression.add_chunk()->set_memory_key("_var0");
  EXPECT_EQ(GetHumanReadableValueExpression(value_expression), "+${1}${_var0}");
}

TEST(FieldFormatterTest, DifferentLocales) {
  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(
      &profile, "John", "", "Doe", "editor@gmail.com", "", "203 Barfield Lane",
      "", "Mountain View", "CA", "94043", "US", "+12345678901");

  // 36 == ADDRESS_HOME_COUNTRY
  EXPECT_THAT(CreateAutofillMappings(profile, "en-US"),
              Contains(Pair(Key(36), "United States")));
  EXPECT_THAT(CreateAutofillMappings(profile, "de-DE"),
              Contains(Pair(Key(36), "Vereinigte Staaten")));

  // Invalid locales default to "en-US".
  // Android and Desktop use a different default.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_THAT(CreateAutofillMappings(profile, ""),
              Contains(Pair(Key(36), "United States")));
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)
  EXPECT_THAT(CreateAutofillMappings(profile, ""),
              Contains(Pair(Key(36), "US")));
#endif
  EXPECT_THAT(CreateAutofillMappings(profile, "invalid"),
              Contains(Pair(Key(36), "United States")));
}

TEST(FieldFormatterTest, AddsAllProfileFieldsUsAddress) {
  base::flat_map<Key, std::string> expected_values = {
      {Key(-8), "US"},
      {Key(-6), "California"},
      {Key(3), "Alpha"},
      {Key(4), "Beta"},
      {Key(5), "Gamma"},
      {Key(6), "B"},
      {Key(7), "Alpha Beta Gamma"},
      {Key(9), "alpha@google.com"},
      {Key(10), "1234567890"},
      {Key(12), "1"},
      {Key(13), "1234567890"},
      {Key(14), "+11234567890"},
      {Key(30), "Amphitheatre Parkway 1600"},
      {Key(31), "Google Building 1"},
      {Key(33), "Mountain View"},
      {Key(34), "CA"},
      {Key(35), "94043"},
      {Key(36), "United States"},
      {Key(60), "Google"},
      {Key(77), "Amphitheatre Parkway 1600\nGoogle Building 1"}};

  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(
      &profile, "Alpha", "Beta", "Gamma", "alpha@google.com", "Google",
      "Amphitheatre Parkway 1600", "Google Building 1", "Mountain View", "CA",
      "94043", "US", "+1 123-456-7890");

  EXPECT_THAT(CreateAutofillMappings(profile, "en-US"),
              IsSupersetOf(expected_values));
}

TEST(FieldFormatterTest, AddsAllProfileFieldsForNonUsAddress) {
  base::flat_map<Key, std::string> expected_values = {
      {Key(-8), "CH"},
      {Key(-6), "Canton Zurich"},
      {Key(3), "Alpha"},
      {Key(4), "Beta"},
      {Key(5), "Gamma"},
      {Key(6), "B"},
      {Key(7), "Alpha Beta Gamma"},
      {Key(9), "alpha@google.com"},
      {Key(10), "1234567"},
      {Key(11), "79"},
      {Key(12), "41"},
      {Key(13), "0791234567"},
      {Key(14), "+41791234567"},
      {Key(30), "Brandschenkestrasse 110"},
      {Key(31), "Google Building 110"},
      {Key(33), "Zurich"},
      {Key(35), "8002"},
      {Key(36), "Switzerland"},
      {Key(60), "Google"},
      {Key(77), "Brandschenkestrasse 110\nGoogle Building 110"}};

  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(
      &profile, "Alpha", "Beta", "Gamma", "alpha@google.com", "Google",
      "Brandschenkestrasse 110", "Google Building 110", "Zurich",
      "Canton Zurich", "8002", "CH", "+41791234567");

  EXPECT_THAT(CreateAutofillMappings(profile, "en-US"),
              IsSupersetOf(expected_values));
}

TEST(FieldFormatterTest, AddsAllCreditCardFields) {
  base::flat_map<Key, std::string> expected_values = {
      {Key(-7), "8"},
      {Key(-5), "Visa"},
      {Key(-4), "1111"},
      {Key(-2), "visa"},
      {Key(51), "Alpha Beta Gamma"},
      {Key(52), "4111111111111111"},
      {Key(53), "08"},
      {Key(54), "50"},
      {Key(55), "2050"},
      {Key(56), "08/50"},
      {Key(57), "08/2050"},
      {Key(58), "Visa"},
      {Key(91), "Alpha"},
      {Key(92), "Gamma"}};

  autofill::CreditCard credit_card(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card, "Alpha Beta Gamma",
                                    "4111111111111111", "8", "2050", "");

  EXPECT_THAT(CreateAutofillMappings(credit_card, "en-US"),
              IsSupersetOf(expected_values));
}

TEST(FieldFormatterTest, KeyComparison) {
  Key int_key_1(1);
  Key int_key_2(2);

  EXPECT_TRUE(int_key_1 < int_key_2);
  EXPECT_FALSE(int_key_2 < int_key_1);

  Key string_key_a("a");
  Key string_key_b("b");

  EXPECT_TRUE(string_key_a < string_key_b);
  EXPECT_FALSE(string_key_b < string_key_a);
}

TEST(FieldFormatterTest, KeyEquality) {
  Key int_key_1(1);
  Key int_key_2(2);
  Key string_key_a("a");
  Key string_key_b("b");

  EXPECT_FALSE(int_key_1 == string_key_a);
  EXPECT_FALSE(string_key_a == int_key_1);

  EXPECT_FALSE(int_key_1 == int_key_2);
  EXPECT_FALSE(int_key_2 == int_key_1);

  EXPECT_FALSE(string_key_a == string_key_b);
  EXPECT_FALSE(string_key_b == string_key_a);

  EXPECT_TRUE(int_key_1 == int_key_1);
  EXPECT_TRUE(string_key_a == string_key_a);
}

}  // namespace field_formatter
}  // namespace autofill_assistant
