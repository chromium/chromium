// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/pattern_provider/pattern_configuration_parser.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/pattern_provider/pattern_provider.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/language_code.h"
#include "components/grit/components_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace autofill {

namespace field_type_parsing {

namespace {

const char kPositivePatternKey[] = "positive_pattern";
const char kNegativePatternKey[] = "negative_pattern";
const char kPositiveScoreKey[] = "positive_score";
const char kMatchFieldAttributesKey[] = "match_field_attributes";
const char kMatchFieldInputTypesKey[] = "match_field_input_types";
const char kVersionKey[] = "version";

bool ParseMatchingPattern(PatternProvider::Map& patterns,
                          const std::string& field_type,
                          const LanguageCode& language,
                          const base::Value& value) {
  if (!value.is_dict())
    return false;

  const std::string* positive_pattern =
      value.FindStringKey(kPositivePatternKey);
  const std::string* negative_pattern =
      value.FindStringKey(kNegativePatternKey);
  base::Optional<double> positive_score =
      value.FindDoubleKey(kPositiveScoreKey);
  base::Optional<int> match_field_attributes =
      value.FindIntKey(kMatchFieldAttributesKey);
  base::Optional<int> match_field_input_types =
      value.FindIntKey(kMatchFieldInputTypesKey);

  if (!positive_pattern || !positive_score || !match_field_attributes ||
      !match_field_input_types)
    return false;

  autofill::MatchingPattern new_pattern;
  new_pattern.positive_pattern = *positive_pattern;
  new_pattern.positive_score = *positive_score;
  if (negative_pattern != nullptr) {
    new_pattern.negative_pattern = *negative_pattern;
  } else {
    new_pattern.negative_pattern = "";
  }
  new_pattern.match_field_attributes = match_field_attributes.value();
  new_pattern.match_field_input_types = match_field_input_types.value();
  new_pattern.language = language;

  // Shift to the right to match the MatchFieldTypes enum, which temporarily
  // starts at 1<<2 instead of 1<<0.
  new_pattern.match_field_input_types <<= 2;

  std::vector<MatchingPattern>* pattern_list = &patterns[field_type][language];
  pattern_list->push_back(new_pattern);

  DVLOG(2) << "Correctly parsed MatchingPattern with with type " << field_type
           << ", language " << language.value() << ", pattern "
           << new_pattern.positive_pattern << ".";

  return true;
}

// Callback which is used once the JSON is parsed. If the configuration versions
// are equal or both unspecified (i.e. set to 0) this prioritizes the remote
// configuration over the local one.
void OnJsonParsed(data_decoder::DataDecoder::ValueOrError result) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillParsingPatternsFromRemote)) {
    DVLOG(1) << "Remote patterns are disabled.";
    return;
  }

  if (!result.value) {
    DVLOG(1) << "Failed to parse PatternProvider configuration JSON string.";
    return;
  }

  base::Version version = ExtractVersionFromJsonObject(result.value.value());
  base::Optional<PatternProvider::Map> patterns =
      GetConfigurationFromJsonObject(result.value.value());

  if (patterns && version.IsValid()) {
    DVLOG(1) << "Successfully parsed PatternProvider configuration.";
    PatternProvider& pattern_provider = PatternProvider::GetInstance();
    pattern_provider.SetPatterns(std::move(patterns.value()),
                                 std::move(version));
  } else {
    DVLOG(1) << "Failed to parse PatternProvider configuration JSON object.";
  }
}

}  // namespace

base::Optional<PatternProvider::Map> GetConfigurationFromJsonObject(
    const base::Value& root) {
  PatternProvider::Map patterns;

  if (!root.is_dict()) {
    DVLOG(1) << "JSON object is not a dictionary.";
    return base::nullopt;
  }

  for (const auto& kv : root.DictItems()) {
    const std::string& field_type = kv.first;
    const base::Value* field_type_dict = &kv.second;

    if (!field_type_dict->is_dict()) {
      DVLOG(1) << "|" << field_type << "| does not contain a dictionary.";
      return base::nullopt;
    }

    for (const auto& value : field_type_dict->DictItems()) {
      LanguageCode language(value.first);
      const base::Value* inner_list = &value.second;

      if (!inner_list->is_list()) {
        DVLOG(1) << "Language |" << language << "| in |" << field_type
                 << "| does not contain a list.";
        return base::nullopt;
      }

      for (const auto& matchingPatternObj : inner_list->GetList()) {
        bool success = ParseMatchingPattern(patterns, field_type, language,
                                            matchingPatternObj);
        if (!success) {
          DVLOG(1) << "Found incorrect |MatchingPattern| object in list |"
                   << field_type << "|, language |" << language.value() << "|.";
          return base::nullopt;
        }
      }
    }
  }

  return base::make_optional(patterns);
}

base::Version ExtractVersionFromJsonObject(base::Value& root) {
  if (!root.is_dict())
    return base::Version("0");

  base::Optional<base::Value> version_str = root.ExtractKey(kVersionKey);
  if (!version_str || !version_str.value().is_string())
    return base::Version("0");

  base::Version version = base::Version(version_str.value().GetString());
  if (!version.IsValid())
    return base::Version("0");

  return version;
}

void PopulateFromJsonString(std::string json_string) {
  data_decoder::DataDecoder::ParseJsonIsolated(std::move(json_string),
                                               base::BindOnce(&OnJsonParsed));
}

base::Optional<PatternProvider::Map>
GetPatternsFromResourceBundleSynchronously() {
  if (!ui::ResourceBundle::HasSharedInstance()) {
    VLOG(1) << "Resource Bundle unavailable to load Autofill Matching Pattern "
               "definitions.";
    return base::nullopt;
  }

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  std::string resource_string =
      bundle.LoadDataResourceString(IDR_AUTOFILL_REGEX_JSON);
  base::Optional<base::Value> json_object =
      base::JSONReader::Read(resource_string);

  // Discard version, since this is the only getter used in unit tests.
  base::Version version = ExtractVersionFromJsonObject(json_object.value());
  base::Optional<PatternProvider::Map> configuration_map =
      GetConfigurationFromJsonObject(json_object.value());

  return configuration_map;
}

}  // namespace field_type_parsing

}  // namespace autofill
