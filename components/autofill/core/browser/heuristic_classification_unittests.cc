// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The purpose of this file is to perform end-to-end form classifications for
// form structures that were recorded as JSON files. These tests currently don't
// produce 100% fidelity compared to real browsing. E.g. we don't support
// invisible fields, yet. Still, they are helpful for tuning heuristics and
// retionalization against a corpus of observed forms.
//
// Test files are located components/test/data/autofill/heuristics-json/
// and describe the status quo. Not necessarily the correct, expected behavior.
// If the classification changes (for the better or worse), a new file is
// written into this directory and the test fails. You can compare the results
// with `diff -U5 $old_file $new_file`.
//
// The structure of the input files is as follows:
//  {
//    "config": {
//       // 2 letter country code, used to mock the user's current location.
//      "country": "US",
//      // 2 letter language code, used to mock the website's language.
//      "language": "en",
//      // List of fields for which the expected type is verified. Fields not
//      // listed here are presented to the local heuristics but the outputs
//      // are not checked and mismatches are not reported.
//      "fields_in_scope": [
//        "UNKNOWN_TYPE",
//        "ADDRESS_HOME_CITY",
//        ...
//      ],
//    },
//    "sites": [
//      {
//        // URL of the website from which the form was recoreded, useful for
//        // debugging.
//        "site_url": "https://www.example.com",
//        // List of forms recorded for the website (e.g. a website can have an
//        // address form and a payment form).
//        "forms": [
//          {
//            "form_signature": "1234567",
//            "fields": [
//               {
//                 // "{form_sig}_{field_sig}_{field_rank_in_signature_group}
//                 "id": "15461699092647468671_1855613035_0",
//                 "field_signature": "1855613035",
//                 // Absolute position of the field in the form. Fields should
//                 // be sorted by "field_position". Fields are presented to the
//                 // heuristics in the order they appear in the JSON file.
//                 // This field is only used for debugging purposes.
//                 "field_position": 0,
//                 // <label>{label_attr}
//                 // <input id="{id_attr}" name="{name_attr}"
//                 //        type="{type_attr}"
//                 //        autocomplete="{autocomplete_attr}">
//                 // </label>
//                 "id_attr": "first",
//                 "name_attr": "firstName",
//                 "label_attr": "First name",
//                 "type_attr": "text",
//                 "autocomplete_attr": "given-name",
//                 // The field types a human tester considered correct.
//                 // Currently only the first type is considered.
//                 "tester_type": [
//                   "NAME_FIRST"
//                 ],
//                 // Correctness of the last classification. The value can be
//                 // one of:
//                 // - "correct" if the last classification matched the first
//                 //   "tester_type".
//                 // - "not_recognized: {tester_type}, chosen_instead: {type2}"
//                 //   if {tester_type} was not recognized but the heuristics
//                 //   but classification and rationalization produced
//                 //   {type2} instead.
//                 // - "ignored: {tester_type}" if the field type is not in
//                 //   scope of the test.
//                 "last_correctness": "correct|not_recognized: ...",
//                 // ^^^^^^ THIS GETS UPDATED BY RUNNING THE TEST.
//                 // The last field type predicted by the heuristics and
//                 // rationalization.
//                 "last_classification": "NAME_FIRST"
//                 // ^^^^^^ THIS GETS UPDATED BY RUNNING THE TEST.
//              }
//            ]
//          }
//        ]
//      ],
//    // Summary of the classification.
//    "stats": {
//    // ^^^^^^ THIS GETS UPDATED BY RUNNING THE TEST
//      "high_level_stats": {
//        // Which fraction of fields had the heuristic type match the tester
//        // type.
//        "fraction_machtes": 0.7258244384259996,
//        // Number of fields for which the heuristic type matched the tester
//        // type or did not match.
//        "matches": 9112,
//        "mismatches": 3442
//      },
//      // Same staistics as above, drilled down by tester type.
//      "per_type_stats": {
//         "{tester_type}": {
//            "fraction_machtes": 0.9132743362831859,
//            "matches": 1032,
//            "mismatches": 98
//         },
//         ...
//      },
//      "ignored_types_stats": {
//        "{tester_type}": 1
//      }
//    }
//  }

#include <sstream>
#include <string_view>
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/language_code.h"
#include "components/variations/variations_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#endif

using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;

namespace autofill {
namespace {

// Helper class that aggregates metrics and diagnostic data about field
// classifications that matched or mismatched the expecations.
class ResultAnalyzer {
 public:
  // Uppercase string serializations of types. Eg. "ADDRES_HOME_CITY."
  using TesterTypeAsString = std::string;
  using HeuristicTypeAsString = std::string;

  explicit ResultAnalyzer(std::vector<std::string> fields_in_scope)
      : fields_in_scope_(std::move(fields_in_scope)) {}

  // Records metrics for all fields after classification. `form_structure`
  // stores the result of the classification. `form_dict` corresponds to the
  // section in the JSON file for the form. This may be updated if field
  // classifications diverge from the last run.
  void AnalyzeClassification(const FormStructure& form_structure,
                             base::Value::Dict& form_dict);

  // Returns a dictionary that can be embedded in the output summarizing quality
  // metrics (see "stats" above).
  base::Value GetResult();

 protected:
  // Field types not in `fields_in_scope_` are ignored during the statistics
  // collection.
  const base::flat_set<TesterTypeAsString> fields_in_scope_;

  // Number of fields that matched or mismatched the classification by a human
  // tester.
  int matches_{0};
  int mismatches_{0};
  // As above but keyed by the type a human tester assigned to the field.
  base::flat_map<TesterTypeAsString, int> match_by_type_count_;
  base::flat_map<TesterTypeAsString, int> mismatch_by_type_count_;
  // Frequency at which tester types were ignored because they were out of
  // scope.
  base::flat_map<TesterTypeAsString, int> ignored_by_type_count_;
};

void ResultAnalyzer::AnalyzeClassification(const FormStructure& form_structure,
                                           base::Value::Dict& form_dict) {
  base::Value::List& json_fields = *form_dict.FindList("fields");
  for (size_t i = 0; i < json_fields.size(); ++i) {
    base::Value::List* tester_types =
        json_fields[i].GetDict().FindList("tester_type");

    // Determine the type assigned to the field by a tester.
    std::string tester_type;
    if (tester_types && tester_types->size() >= 1) {
      ASSERT_TRUE((*tester_types)[0].is_string());
      tester_type = (*tester_types)[0].GetString();
    }

    // Determine the type assigned to the field by the heuristic classification.
    std::string heuristic_type =
        AutofillType(form_structure.field(i)->Type().GetStorableType())
            .ToString();

    // Record metrics on the divergence between tester and heuristics.
    if (fields_in_scope_.contains(tester_type)) {
      if (tester_type == heuristic_type) {
        ++matches_;
        ++match_by_type_count_[tester_type];
        json_fields[i].GetDict().Set("last_correctness", "correct");
      } else {
        ++mismatches_;
        ++mismatch_by_type_count_[tester_type];
        json_fields[i].GetDict().Set("last_correctness",
                                     "not_recognized: " + tester_type +
                                         ", chosen_instead: " + heuristic_type);
      }
    } else {
      ++ignored_by_type_count_[tester_type];
      json_fields[i].GetDict().Set("last_correctness",
                                   "ignored: " + tester_type);
    }
    json_fields[i].GetDict().Set("last_classification", heuristic_type);
  }
}

base::Value ResultAnalyzer::GetResult() {
  // Dictionary that summarizes the results of all field classifications for all
  // forms.
  base::Value::Dict result;

  // Highlevel statistics.
  base::Value::Dict high_level_stats;
  high_level_stats.Set("matches", matches_);
  high_level_stats.Set("mismatches", mismatches_);
  high_level_stats.Set("fraction_machtes",
                       matches_ / (double)(matches_ + mismatches_));
  result.Set("high_level_stats", std::move(high_level_stats));

  // Per type stats.
  base::Value::Dict per_type_stats;
  for (const std::string& type : fields_in_scope_) {
    if (match_by_type_count_.contains(type) ||
        mismatch_by_type_count_.contains(type)) {
      int matches = match_by_type_count_[type];
      int mismatches = mismatch_by_type_count_[type];
      base::Value::Dict tester_type_stats;
      tester_type_stats.Set("matches", matches);
      tester_type_stats.Set("mismatches", mismatches);
      tester_type_stats.Set("fraction_machtes",
                            matches / (double)(matches + mismatches));
      per_type_stats.Set(type, std::move(tester_type_stats));
    }
  }
  result.Set("per_type_stats", std::move(per_type_stats));

  // Stats for ignored field types.
  base::Value::Dict ignored_types_stats;
  for (auto it : ignored_by_type_count_) {
    ignored_types_stats.Set(it.first, it.second);
  }
  result.Set("ignored_types_stats", std::move(ignored_types_stats));

  return base::Value(std::move(result));
}

// Returns the path containing test input files,
// components/test/data/autofill/heuristics-json/.
const base::FilePath& GetInputDir() {
  static base::NoDestructor<base::FilePath> dir([]() {
    base::FilePath dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &dir);
    return dir.AppendASCII("components")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("autofill")
        .AppendASCII("heuristics-json");
  }());
  return *dir;
}

// Returns all "*.json" files in `GetInputDir()`.
std::vector<base::FilePath> GetTestFiles() {
  base::FileEnumerator input_files(
      GetInputDir(), /*recursive=*/true, base::FileEnumerator::FILES,
      FILE_PATH_LITERAL("*.json"),
      base::FileEnumerator::FolderSearchPolicy::ALL);
  std::vector<base::FilePath> files;
  input_files.ForEach(
      [&files](const base::FilePath& item) { files.push_back(item); });
  std::sort(files.begin(), files.end());

#if BUILDFLAG(IS_MAC)
  base::apple::ClearAmIBundledCache();
#endif  // BUILDFLAG(IS_MAC)

  return files;
}

// Extracts data of a single field from `field_dict` using the Form `form_data`
// as contextual information.
// `field_dict` corresponds to an entry of `.sites[].forms[].fields[]` in the
// JSON input file in jq syntax (https://jqlang.github.io/jq/).
FormFieldData ParseFieldFromJsonDict(const base::Value::Dict& field_dict,
                                     const FormData& form_data) {
  FormFieldData field;

  if (const std::string* id = field_dict.FindString("id_attr")) {
    field.id_attribute = base::UTF8ToUTF16(*id);
  }
  if (const std::string* name = field_dict.FindString("name_attr")) {
    field.name_attribute = base::UTF8ToUTF16(*name);
  }
  // `FormFieldData::name` is used for form signature calculation and a fallback
  // from a field's name to the field's id.
  field.name = base::TrimWhitespace(field.name_attribute, base::TRIM_ALL);
  if (field.name.empty()) {
    field.name = base::TrimWhitespace(field.id_attribute, base::TRIM_ALL);
  }

  if (const std::string* label = field_dict.FindString("label_attr")) {
    field.label = base::UTF8ToUTF16(*label);
  }
  if (const std::string* type = field_dict.FindString("type_attr")) {
    if (*type == "select") {
      field.form_control_type = "select-one";
    } else if (*type == "input") {
      field.form_control_type = "text";
    } else {
      field.form_control_type = *type;
    }
  }
  if (const std::string* autocomplete =
          field_dict.FindString("autocomplete_attr")) {
    field.autocomplete_attribute = *autocomplete;
    field.parsed_autocomplete = ParseAutocompleteAttribute(*autocomplete);
  }
  if (const std::string* placeholder =
          field_dict.FindString("placeholder_attr")) {
    field.placeholder = base::UTF8ToUTF16(*placeholder);
  }
  if (const std::string* maxlength = field_dict.FindString("maxlength_attr")) {
    base::StringToUint64(*maxlength, &field.max_length);
  }
  field.is_focusable = true;
  field.role = FormFieldData::RoleAttribute::kOther;
  field.origin = form_data.main_frame_origin;
  field.host_frame = form_data.host_frame;
  field.host_form_id = form_data.unique_renderer_id;
  field.unique_renderer_id = test::MakeFieldRendererId();
  return field;
}

[[nodiscard]] AssertionResult ParseFormFromJsonDict(
    const base::Value::Dict& form_dict,
    const std::string& site_url,
    FormData& form_data) {
  form_data.url = GURL(site_url);
  form_data.main_frame_origin = url::Origin::Create(form_data.url);
  form_data.host_frame = test::MakeLocalFrameToken();
  form_data.unique_renderer_id = test::MakeFormRendererId();

  const base::Value::List* fields = form_dict.FindList("fields");
  if (!fields) {
    return AssertionFailure() << "A form has no fields in " << site_url;
  }

  for (const base::Value& field_json : *fields) {
    if (!field_json.is_dict()) {
      return AssertionFailure() << "A field is no dict in " << site_url;
    }
    form_data.fields.push_back(
        ParseFieldFromJsonDict(field_json.GetDict(), form_data));
  }

  return AssertionSuccess();
}

// Tests classifications of a site. The returned test result expresses whether
// the test data could be parsed and the fields could be classified. It does
// not make an assessment of whether the heuristics generated the expected data.
// That is recorded via `result_analyzer`.
// Test field classification resutls are updated in `site` in the
// `.sites[].forms[].fields[].last_classification` field. This is why the `site`
// is a mutable parameter.
// `site` corresponds to an entry of `.sites[]` in the JSON input file in jq
// syntax (https://jqlang.github.io/jq/)
[[nodiscard]] AssertionResult ClassifyFieldsOfSite(
    base::Value::Dict& site,
    const GeoIpCountryCode& client_country,
    LanguageCode page_language,
    ResultAnalyzer& result_analyzer,
    LogManager* log_manager) {
  const std::string* site_url = site.FindString("site_url");
  if (!site_url) {
    return AssertionFailure() << "Missing attribute 'site_url' in" << site;
  }
  base::Value::List* forms = site.FindList("forms");
  if (!forms) {
    return AssertionFailure() << "No 'forms' in " << site;
  }
  for (base::Value& form : *forms) {
    if (!form.is_dict()) {
      return AssertionFailure() << "A form is not a dictionary in " << site;
    }
    FormData form_data;
    if (AssertionResult result =
            ParseFormFromJsonDict(form.GetDict(), *site_url, form_data);
        !result) {
      return result;
    }
    FormStructure form_structure(form_data);
    form_structure.set_current_page_language(page_language);
    form_structure.DetermineHeuristicTypes(client_country, nullptr,
                                           log_manager);
    result_analyzer.AnalyzeClassification(form_structure, form.GetDict());
  }
  return AssertionSuccess();
}

// Creates a textual description of the statistics. This is good for a quick
// view in the delta for an EXPECT_EQ().
[[nodiscard]] std::string SummarizeStatistics(
    const base::Value::Dict& json_file) {
  std::ostringstream result;

  const base::Value::Dict* stats = json_file.FindDict("stats");
  if (!stats) {
    return std::string();
  }

  auto summarize_sub_section = [](const std::string& caption,
                                  const base::Value::Dict& dict) {
    std::ostringstream result;
    result << caption << ": Fraction matches " << std::fixed
           << std::setprecision(2)
           << (*dict.FindDouble("fraction_machtes") * 100.0) << "%, "
           << "Matches: " << *dict.FindInt("matches") << ", "
           << "Mismatches: " << *dict.FindInt("mismatches") << std::endl;
    return result.str();
  };

  if (const auto* high_level_stats = stats->FindDict("high_level_stats");
      high_level_stats) {
    result << summarize_sub_section("Summary", *high_level_stats);
  }
  if (const auto* per_type_stats = stats->FindDict("per_type_stats");
      per_type_stats) {
    for (const auto it : *per_type_stats) {
      result << summarize_sub_section(it.first, it.second.GetDict());
    }
  }

  return result.str();
}

class HeuristicClassificationTests
    : public testing::Test,
      public testing::WithParamInterface<base::FilePath> {
 public:
  void SetUp() override;

 protected:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  LogRouter log_router_;
  std::unique_ptr<LogManager> log_manager_;
};

void HeuristicClassificationTests::SetUp() {
  if (base::FeatureList::IsEnabled(features::test::kAutofillLogToTerminal)) {
    log_router_.LogToTerminal();
  }
  log_manager_ = LogManager::Create(&log_router_, base::NullCallback());
}

TEST_P(HeuristicClassificationTests, EndToEnd) {
  base::FilePath input_file = GetParam();
  SCOPED_TRACE(::testing::Message() << input_file);

  // Read input file.
  std::string input_json_text;
  ASSERT_TRUE(base::ReadFileToString(input_file, &input_json_text));

  // Convert to JSON dictionary.
  absl::optional<base::Value> opt_json_file =
      base::JSONReader::Read(input_json_text);
  ASSERT_TRUE(opt_json_file);
  base::Value::Dict* json_file = opt_json_file->GetIfDict();
  ASSERT_TRUE(json_file);

  std::string old_stats = SummarizeStatistics(*json_file);

  base::Value::Dict* config = json_file->FindDict("config");
  ASSERT_TRUE(config);

  // Configure IP based location.
  const std::string* country = config->FindString("country");
  ASSERT_TRUE(country);
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      variations::switches::kVariationsOverrideCountry, *country);

  std::vector<base::test::FeatureRef> enabled_features = {
      // This is always enabled to classify autocomplete=invalid fields.
      features::kAutofillPredictionsForAutocompleteUnrecognized,
      // Support for new field types.
      features::kAutofillEnableSupportForBetweenStreets,
      features::kAutofillEnableSupportForAdminLevel2,
      features::kAutofillEnableSupportForAddressOverflow,
      features::kAutofillEnableSupportForAddressOverflowAndLandmark,
      features::kAutofillEnableSupportForLandmark,
      features::kAutofillEnableSupportForApartmentNumbers,
      features::kAutofillEnableDependentLocalityParsing,
      features::kAutofillEnableExpirationDateImprovements,
      // Allow local heuristics to take precedence.
      features::kAutofillStreetNameOrHouseNumberPrecedenceOverAutocomplete,
      // Other improvements.
      features::kAutofillEnableZipOnlyAddressForms,
      features::kAutofillDefaultToCityAndNumber};
  std::vector<base::test::FeatureRef> disabled_features = {};

  auto init_feature_to_value = [&](base::test::FeatureRef feature, bool value) {
    if (value) {
      enabled_features.push_back(feature);
    } else {
      disabled_features.push_back(feature);
    }
  };

  std::vector<std::string> structured_fields_disable_address_lines = {"BR",
                                                                      "MX"};
  init_feature_to_value(
      features::kAutofillStructuredFieldsDisableAddressLines,
      base::Contains(structured_fields_disable_address_lines, *country));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(enabled_features, disabled_features);

  // Configure page language.
  const std::string* language = config->FindString("language");
  ASSERT_TRUE(language);
  LanguageCode page_language(*language);

  // Configure list of fields that are in scope for reporting.
  const base::Value::List* fields_in_scope_json =
      config->FindList("fields_in_scope");
  ASSERT_TRUE(fields_in_scope_json);
  std::vector<std::string> fields_in_scope;
  for (const base::Value& field : *fields_in_scope_json) {
    ASSERT_TRUE(field.is_string());
    fields_in_scope.push_back(field.GetString());
  }

  // Test all sites.
  base::Value::List* sites = json_file->FindList("sites");
  ASSERT_TRUE(sites);
  ResultAnalyzer result_analyzer(std::move(fields_in_scope));
  for (base::Value& site : *sites) {
    ASSERT_TRUE(site.is_dict());
    ASSERT_TRUE(ClassifyFieldsOfSite(site.GetDict(), GeoIpCountryCode(*country),
                                     page_language, result_analyzer,
                                     log_manager_.get()));
  }

  // Update statistics
  json_file->Set("stats", result_analyzer.GetResult());

  std::string new_stats = SummarizeStatistics(*json_file);

  // Serialize the result.
  absl::optional<std::string> output_json_text =
      base::WriteJsonWithOptions(*opt_json_file, base::OPTIONS_PRETTY_PRINT);
  ASSERT_TRUE(output_json_text);

  // Replace \r\n on windows with \n to get a canonical representation.
  base::RemoveChars(*output_json_text, "\r", &(*output_json_text));

  // Write output if and only if it is different.
  if (input_json_text != output_json_text) {
    base::FilePath output_file =
        GetParam().AddExtension(FILE_PATH_LITERAL(".new"));
    LOG(ERROR) << "Classifications changed. Writing new file " << output_file;
    EXPECT_TRUE(base::WriteFile(output_file, *output_json_text));
  }

  EXPECT_EQ(old_stats, new_stats);

  // Too large inputs crash the test.
  if (input_json_text.size() < 20000) {
    EXPECT_EQ(input_json_text, output_json_text);
  } else {
    EXPECT_TRUE(input_json_text == output_json_text);
  }
}

INSTANTIATE_TEST_SUITE_P(AllForms,
                         HeuristicClassificationTests,
                         testing::ValuesIn(GetTestFiles()));

}  // namespace
}  // namespace autofill
