// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_hats_trigger_helper.h"

#include <utility>

#include "base/check_is_test.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/permissions/constants.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace permissions {

namespace {

bool is_test = false;

std::vector<std::string> SplitCsvString(const std::string& csv_string) {
  return base::SplitString(csv_string, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

bool StringMatchesFilter(const std::string& string, const std::string& filter) {
  return filter.empty() ||
         base::ranges::any_of(SplitCsvString(filter),
                              [string](base::StringPiece current_filter) {
                                return base::EqualsCaseInsensitiveASCII(
                                    string, current_filter);
                              });
}

std::map<std::string, std::pair<std::string, std::string>>
GetKeyToValueFilterPairMap(
    PermissionHatsTriggerHelper::PromptParametersForHaTS prompt_parameters) {
  // configuration key -> {current value for key, configured filter for key}
  return {
      {kPermissionsPromptSurveyPromptDispositionKey,
       {PermissionUmaUtil::GetPromptDispositionString(
            prompt_parameters.prompt_disposition),
        feature_params::kPermissionsPromptSurveyPromptDispositionFilter.Get()}},
      {kPermissionsPromptSurveyPromptDispositionReasonKey,
       {PermissionUmaUtil::GetPromptDispositionReasonString(
            prompt_parameters.prompt_disposition_reason),
        feature_params::kPermissionsPromptSurveyPromptDispositionReasonFilter
            .Get()}},
      {kPermissionsPromptSurveyActionKey,
       {prompt_parameters.action.has_value()
            ? PermissionUmaUtil::GetPermissionActionString(
                  prompt_parameters.action.value())
            : "",
        feature_params::kPermissionsPromptSurveyActionFilter.Get()}},
      {kPermissionsPromptSurveyRequestTypeKey,
       {PermissionUmaUtil::GetRequestTypeString(prompt_parameters.request_type),
        feature_params::kPermissionsPromptSurveyRequestTypeFilter.Get()}},
      {kPermissionsPromptSurveyHadGestureKey,
       {prompt_parameters.gesture_type == PermissionRequestGestureType::GESTURE
            ? kTrueStr
            : kFalseStr,
        feature_params::kPermissionsPromptSurveyHadGestureFilter.Get()}},
      {kPermissionsPromptSurveyReleaseChannelKey,
       {prompt_parameters.channel,
        feature_params::kPermissionPromptSurveyReleaseChannelFilter.Get()}},
      {kPermissionsPromptSurveyDisplayTimeKey,
       {prompt_parameters.survey_display_time,
        feature_params::kPermissionsPromptSurveyDisplayTime.Get()}},
      {kPermissionPromptSurveyOneTimePromptsDecidedBucketKey,
       {PermissionHatsTriggerHelper::GetOneTimePromptsDecidedBucketString(
            prompt_parameters.one_time_prompts_decided_bucket),
        feature_params::kPermissionPromptSurveyOneTimePromptsDecidedBucket
            .Get()}},
      {kPermissionPromptSurveyUrlKey, {prompt_parameters.url, ""}}};
}

// Typos in the gcl configuration cannot be verified and may be missed by
// reviewers. In the worst case, no filters are configured. By definition of
// our filters, this would match all requests. To safeguard against this kind
// of misconfiguration (which would lead to very high HaTS QPS), we enforce
// that at least one valid filter must be configured.
bool IsValidConfiguration(
    PermissionHatsTriggerHelper::PromptParametersForHaTS prompt_parameters) {
  auto filter_pair_map = GetKeyToValueFilterPairMap(prompt_parameters);

  if (filter_pair_map[kPermissionsPromptSurveyDisplayTimeKey].second.empty()) {
    // When no display time is configured, the survey should never be triggered.
    return false;
  }

  // Returns false if all filter parameters are empty.
  return !base::ranges::all_of(
      filter_pair_map,
      [](std::pair<std::string, std::pair<std::string, std::string>> entry) {
        return entry.second.second.empty();
      });
}

std::vector<double> ParseProbabilityVector(std::string probability_vector_csv) {
  std::vector<std::string> probability_string_vector =
      base::SplitString(feature_params::kProbabilityVector.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<double> checked_probability_vector;
  double probability;
  for (std::string probability_string : probability_string_vector) {
    if (!base::StringToDouble(probability_string, &probability)) {
      // Parsing failed, configuration error. Return empty array.
      return std::vector<double>();
    }
    checked_probability_vector.push_back(probability);
  }
  return checked_probability_vector;
}

std::vector<double>& GetProbabilityVector(std::string probability_vector_csv) {
  static base::NoDestructor<std::vector<double>> probability_vector(
      [probability_vector_csv] {
        return ParseProbabilityVector(probability_vector_csv);
      }());

  if (is_test) {
    CHECK_IS_TEST();
    *probability_vector = ParseProbabilityVector(probability_vector_csv);
  }
  return *probability_vector;
}

std::vector<std::string> ParseRequestFilterVector(
    std::string request_vector_csv) {
  return base::SplitString(
      feature_params::kPermissionsPromptSurveyRequestTypeFilter.Get(), ",",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

std::vector<std::string>& GetRequestFilterVector(
    std::string request_vector_csv) {
  static base::NoDestructor<std::vector<std::string>> request_filter_vector(
      [request_vector_csv] {
        return ParseRequestFilterVector(request_vector_csv);
      }());
  if (is_test) {
    CHECK_IS_TEST();
    *request_filter_vector = ParseRequestFilterVector(request_vector_csv);
  }
  return *request_filter_vector;
}

std::vector<std::pair<std::string, std::string>>
ComputePermissionPromptTriggerIdPairs(const std::string& trigger_name_base) {
  std::vector<std::string> permission_trigger_id_vector(
      base::SplitString(feature_params::kPermissionsPromptSurveyTriggerId.Get(),
                        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));
  int trigger_index = 0;
  std::vector<std::pair<std::string, std::string>> pairs;
  pairs.clear();
  for (const auto& trigger_id : permission_trigger_id_vector) {
    pairs.emplace_back(
        trigger_name_base + base::NumberToString(trigger_index++), trigger_id);
  }
  return pairs;
}

}  // namespace

PermissionHatsTriggerHelper::PromptParametersForHaTS::PromptParametersForHaTS(
    RequestType request_type,
    absl::optional<PermissionAction> action,
    PermissionPromptDisposition prompt_disposition,
    PermissionPromptDispositionReason prompt_disposition_reason,
    PermissionRequestGestureType gesture_type,
    const std::string& channel,
    const std::string& survey_display_time,
    absl::optional<base::TimeDelta> prompt_display_duration,
    OneTimePermissionPromptsDecidedBucket one_time_prompts_decided_bucket,
    absl::optional<GURL> gurl)
    : request_type(request_type),
      action(action),
      prompt_disposition(prompt_disposition),
      prompt_disposition_reason(prompt_disposition_reason),
      gesture_type(gesture_type),
      channel(channel),
      survey_display_time(survey_display_time),
      prompt_display_duration(prompt_display_duration),
      one_time_prompts_decided_bucket(one_time_prompts_decided_bucket),
      url(gurl.has_value() ? gurl->spec() : "") {}

PermissionHatsTriggerHelper::PromptParametersForHaTS::PromptParametersForHaTS(
    const PromptParametersForHaTS& other) = default;
PermissionHatsTriggerHelper::PromptParametersForHaTS::
    ~PromptParametersForHaTS() = default;

PermissionHatsTriggerHelper::SurveyProductSpecificData::
    SurveyProductSpecificData(SurveyBitsData survey_bits_data,
                              SurveyStringData survey_string_data)
    : survey_bits_data(survey_bits_data),
      survey_string_data(survey_string_data) {}

PermissionHatsTriggerHelper::SurveyProductSpecificData::
    ~SurveyProductSpecificData() = default;

PermissionHatsTriggerHelper::SurveyProductSpecificData
PermissionHatsTriggerHelper::SurveyProductSpecificData::PopulateFrom(
    PromptParametersForHaTS prompt_parameters) {
  static const char* const kProductSpecificBitsFields[] = {
      kPermissionsPromptSurveyHadGestureKey};
  static const char* const kProductSpecificStringFields[] = {
      kPermissionsPromptSurveyPromptDispositionKey,
      kPermissionsPromptSurveyPromptDispositionReasonKey,
      kPermissionsPromptSurveyActionKey,
      kPermissionsPromptSurveyRequestTypeKey,
      kPermissionsPromptSurveyReleaseChannelKey,
      kPermissionsPromptSurveyDisplayTimeKey,
      kPermissionPromptSurveyOneTimePromptsDecidedBucketKey,
      kPermissionPromptSurveyUrlKey};

  auto key_to_value_filter_pair = GetKeyToValueFilterPairMap(prompt_parameters);
  std::map<std::string, bool> bits_data;
  for (const char* product_specific_bits_field : kProductSpecificBitsFields) {
    auto it = key_to_value_filter_pair.find(product_specific_bits_field);
    if (it != key_to_value_filter_pair.end()) {
      bits_data.insert({it->first, it->second.first == kTrueStr});
    }
  }

  std::map<std::string, std::string> string_data;
  for (const char* product_specific_string_field :
       kProductSpecificStringFields) {
    auto it = key_to_value_filter_pair.find(product_specific_string_field);
    if (it != key_to_value_filter_pair.end()) {
      string_data.insert({it->first, it->second.first});
    }
  }

  return SurveyProductSpecificData(bits_data, string_data);
}

// static
void PermissionHatsTriggerHelper::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kOneTimePermissionPromptsDecidedCount,
                                0);
}

bool PermissionHatsTriggerHelper::ArePromptTriggerCriteriaSatisfied(
    PromptParametersForHaTS prompt_parameters,
    const std::string& trigger_name_base) {
  auto trigger_and_probability = PermissionHatsTriggerHelper::
      GetPermissionPromptTriggerNameAndProbabilityForRequestType(
          trigger_name_base, PermissionUmaUtil::GetRequestTypeString(
                                 prompt_parameters.request_type));

  if (!trigger_and_probability.has_value() ||
      base::RandDouble() >= trigger_and_probability->second) {
    return false;
  }

  if (!IsValidConfiguration(prompt_parameters)) {
    return false;
  }

  if (prompt_parameters.action == PermissionAction::IGNORED &&
      prompt_parameters.prompt_display_duration >
          feature_params::kPermissionPromptSurveyIgnoredPromptsMaximumAge
              .Get()) {
    return false;
  }

  auto key_to_value_filter_pair = GetKeyToValueFilterPairMap(prompt_parameters);
  for (const auto& value_type : key_to_value_filter_pair) {
    const auto& value = value_type.second.first;
    const auto& filter = value_type.second.second;
    if (!StringMatchesFilter(value, filter)) {
      // if any filter doesn't match, no survey should be triggered
      return false;
    }
  }

  return true;
}

// static
void PermissionHatsTriggerHelper::
    IncrementOneTimePermissionPromptsDecidedIfApplicable(
        ContentSettingsType type,
        PrefService* pref_service) {
  if (base::FeatureList::IsEnabled(features::kOneTimePermission) &&
      PermissionUtil::CanPermissionBeAllowedOnce(type)) {
    pref_service->SetInteger(
        prefs::kOneTimePermissionPromptsDecidedCount,
        pref_service->GetInteger(prefs::kOneTimePermissionPromptsDecidedCount) +
            1);
  }
}

// static
PermissionHatsTriggerHelper::OneTimePermissionPromptsDecidedBucket
PermissionHatsTriggerHelper::GetOneTimePromptsDecidedBucket(
    PrefService* pref_service) {
  int count =
      pref_service->GetInteger(prefs::kOneTimePermissionPromptsDecidedCount);
  if (count <= 1) {
    return OneTimePermissionPromptsDecidedBucket::BUCKET_0_1;
  } else if (count <= 3) {
    return OneTimePermissionPromptsDecidedBucket::BUCKET_2_3;
  } else if (count <= 5) {
    return OneTimePermissionPromptsDecidedBucket::BUCKET_4_5;
  } else if (count <= 10) {
    return OneTimePermissionPromptsDecidedBucket::BUCKET_6_10;
  } else if (count <= 20) {
    return OneTimePermissionPromptsDecidedBucket::BUCKET_11_20;
  } else {
    return OneTimePermissionPromptsDecidedBucket::BUCKET_GT20;
  }
}

// static
std::string PermissionHatsTriggerHelper::GetOneTimePromptsDecidedBucketString(
    OneTimePermissionPromptsDecidedBucket bucket) {
  switch (bucket) {
    case BUCKET_0_1:
      return "0_1";
    case BUCKET_2_3:
      return "2_3";
    case BUCKET_4_5:
      return "4_5";
    case BUCKET_6_10:
      return "6_10";
    case BUCKET_11_20:
      return "11_20";
    case BUCKET_GT20:
      return "GT20";
    default:
      NOTREACHED();
  }
}

// static
std::vector<std::pair<std::string, std::string>>&
PermissionHatsTriggerHelper::GetPermissionPromptTriggerIdPairs(
    const std::string& trigger_name_base) {
  static base::NoDestructor<std::vector<std::pair<std::string, std::string>>>
      trigger_id_pairs([trigger_name_base] {
        return ComputePermissionPromptTriggerIdPairs(trigger_name_base);
      }());
  if (is_test) {
    CHECK_IS_TEST();
    *trigger_id_pairs =
        ComputePermissionPromptTriggerIdPairs(trigger_name_base);
  }
  return *trigger_id_pairs;
}

// static
absl::optional<std::pair<std::string, double>> PermissionHatsTriggerHelper::
    GetPermissionPromptTriggerNameAndProbabilityForRequestType(
        const std::string& trigger_name_base,
        const std::string& request_type) {
  auto& trigger_id_pairs = GetPermissionPromptTriggerIdPairs(trigger_name_base);
  auto& probability_vector =
      GetProbabilityVector(feature_params::kProbabilityVector.Get());

  if (trigger_id_pairs.size() == 1 && probability_vector.size() <= 1) {
    // If a value is configured, use it, otherwise set it to 1.
    return std::make_pair(
        trigger_id_pairs[0].first,
        probability_vector.size() == 1 ? probability_vector[0] : 1.0);
  } else if (trigger_id_pairs.size() != probability_vector.size()) {
    // Configuration error
    return absl::nullopt;
  } else {
    auto& request_filter_vector = GetRequestFilterVector(
        feature_params::kPermissionsPromptSurveyRequestTypeFilter.Get());

    if (request_filter_vector.size() != trigger_id_pairs.size()) {
      // Configuration error
      return absl::nullopt;
    }

    for (unsigned long i = 0; i < trigger_id_pairs.size(); i++) {
      if (base::EqualsCaseInsensitiveASCII(request_type,
                                           request_filter_vector[i])) {
        return std::make_pair(trigger_id_pairs.at(i).first,
                              probability_vector[i]);
      }
    }

    // No matching filter
    return absl::nullopt;
  }
}

// static
void PermissionHatsTriggerHelper::SetIsTest() {
  is_test = true;
}

}  // namespace permissions
