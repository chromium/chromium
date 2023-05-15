// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_hats_trigger_helper.h"

#include <utility>

#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/permissions/constants.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace {

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
    permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS
        prompt_parameters) {
  // configuration key -> {current value for key, configured filter for key}
  return {
      {permissions::kPermissionsPromptSurveyPromptDispositionKey,
       {permissions::PermissionUmaUtil::GetPromptDispositionString(
            prompt_parameters.prompt_disposition),
        permissions::feature_params::
            kPermissionsPromptSurveyPromptDispositionFilter.Get()}},
      {permissions::kPermissionsPromptSurveyPromptDispositionReasonKey,
       {permissions::PermissionUmaUtil::GetPromptDispositionReasonString(
            prompt_parameters.prompt_disposition_reason),
        permissions::feature_params::
            kPermissionsPromptSurveyPromptDispositionReasonFilter.Get()}},
      {permissions::kPermissionsPromptSurveyActionKey,
       {prompt_parameters.action.has_value()
            ? permissions::PermissionUmaUtil::GetPermissionActionString(
                  prompt_parameters.action.value())
            : "",
        permissions::feature_params::kPermissionsPromptSurveyActionFilter
            .Get()}},
      {permissions::kPermissionsPromptSurveyRequestTypeKey,
       {permissions::PermissionUmaUtil::GetRequestTypeString(
            prompt_parameters.request_type),
        permissions::feature_params::kPermissionsPromptSurveyRequestTypeFilter
            .Get()}},
      {permissions::kPermissionsPromptSurveyHadGestureKey,
       {prompt_parameters.gesture_type ==
                permissions::PermissionRequestGestureType::GESTURE
            ? permissions::kTrueStr
            : permissions::kFalseStr,
        permissions::feature_params::kPermissionsPromptSurveyHadGestureFilter
            .Get()}},
      {permissions::kPermissionsPromptSurveyReleaseChannelKey,
       {prompt_parameters.channel,
        permissions::feature_params::kPermissionPromptSurveyReleaseChannelFilter
            .Get()}},
      {permissions::kPermissionsPromptSurveyDisplayTimeKey,
       {prompt_parameters.survey_display_time,
        permissions::feature_params::kPermissionsPromptSurveyDisplayTime
            .Get()}},
      {permissions::kPermissionPromptSurveyOneTimePromptsDecidedBucketKey,
       {permissions::PermissionHatsTriggerHelper::
            GetOneTimePromptsDecidedBucketString(
                prompt_parameters.one_time_prompts_decided_bucket),
        permissions::feature_params::
            kPermissionPromptSurveyOneTimePromptsDecidedBucket.Get()}},
      {permissions::kPermissionPromptSurveyUrlKey,
       {prompt_parameters.url, ""}}};
}

// Typos in the gcl configuration cannot be verified and may be missed by
// reviewers. In the worst case, no filters are configured. By definition of
// our filters, this would match all requests. To safeguard against this kind
// of misconfiguration (which would lead to very high HaTS QPS), we enforce
// that at least one valid filter must be configured.
bool IsValidConfiguration(
    permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS
        prompt_parameters) {
  auto filter_pair_map = GetKeyToValueFilterPairMap(prompt_parameters);

  if (filter_pair_map[permissions::kPermissionsPromptSurveyDisplayTimeKey]
          .second.empty()) {
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

}  // namespace

namespace permissions {

PermissionHatsTriggerHelper::PromptParametersForHaTS::PromptParametersForHaTS(
    permissions::RequestType request_type,
    absl::optional<permissions::PermissionAction> action,
    permissions::PermissionPromptDisposition prompt_disposition,
    permissions::PermissionPromptDispositionReason prompt_disposition_reason,
    permissions::PermissionRequestGestureType gesture_type,
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
  const static std::vector<std::string> product_specific_bits_fields = {
      kPermissionsPromptSurveyHadGestureKey};
  const static std::vector<std::string> product_specific_string_fields{
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
  for (auto product_specific_bits_field : product_specific_bits_fields) {
    auto value_type =
        key_to_value_filter_pair.find(product_specific_bits_field);
    if (value_type != key_to_value_filter_pair.end()) {
      bits_data.insert(
          {value_type->first, value_type->second.first == kTrueStr});
    }
  }

  std::map<std::string, std::string> string_data;
  for (auto product_specific_string_field : product_specific_string_fields) {
    auto value_type =
        key_to_value_filter_pair.find(product_specific_string_field);
    if (value_type != key_to_value_filter_pair.end()) {
      string_data.insert({value_type->first, value_type->second.first});
    }
  }

  return SurveyProductSpecificData(bits_data, string_data);
}

// static
void PermissionHatsTriggerHelper::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      permissions::prefs::kOneTimePermissionPromptsDecidedCount, 0);
}

bool PermissionHatsTriggerHelper::ArePromptTriggerCriteriaSatisfied(
    PromptParametersForHaTS prompt_parameters) {
  if (!IsValidConfiguration(prompt_parameters)) {
    return false;
  }

  if (prompt_parameters.action == permissions::PermissionAction::IGNORED &&
      prompt_parameters.prompt_display_duration >
          permissions::feature_params::
              kPermissionPromptSurveyIgnoredPromptsMaximumAge.Get()) {
    return false;
  }

  auto key_to_value_filter_pair = GetKeyToValueFilterPairMap(prompt_parameters);
  for (auto value_type : key_to_value_filter_pair) {
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
  if (base::FeatureList::IsEnabled(permissions::features::kOneTimePermission) &&
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

}  // namespace permissions
