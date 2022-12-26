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

constexpr char kTrueStr[] = "true";
constexpr char kFalseStr[] = "false";

namespace {

std::vector<std::string> SplitCsvString(const std::string& csv_string) {
  return base::SplitString(csv_string, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

bool StringMatchesFilter(const std::string& string, const std::string& filter) {
  DCHECK(!string.empty());
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
      {permissions::kPermissionsPostPromptSurveyPromptDispositionKey,
       {permissions::PermissionUmaUtil::GetPromptDispositionString(
            prompt_parameters.prompt_disposition),
        permissions::feature_params::
            kPermissionsPostPromptSurveyPromptDispositionFilter.Get()}},
      {permissions::kPermissionsPostPromptSurveyPromptDispositionReasonKey,
       {permissions::PermissionUmaUtil::GetPromptDispositionReasonString(
            prompt_parameters.prompt_disposition_reason),
        permissions::feature_params::
            kPermissionsPostPromptSurveyPromptDispositionReasonFilter.Get()}},
      {permissions::kPermissionsPostPromptSurveyActionKey,
       {permissions::PermissionUmaUtil::GetPermissionActionString(
            prompt_parameters.action),
        permissions::feature_params::kPermissionsPostPromptSurveyActionFilter
            .Get()}},
      {permissions::kPermissionsPostPromptSurveyRequestTypeKey,
       {permissions::PermissionUmaUtil::GetRequestTypeString(
            prompt_parameters.request_type),
        permissions::feature_params::
            kPermissionsPostPromptSurveyRequestTypeFilter.Get()}},
      {permissions::kPermissionsPostPromptSurveyHadGestureKey,
       {prompt_parameters.gesture_type ==
                permissions::PermissionRequestGestureType::GESTURE
            ? kTrueStr
            : kFalseStr,
        permissions::feature_params::
            kPermissionsPostPromptSurveyHadGestureFilter.Get()}},
      {permissions::kPermissionsPostPromptSurveyReleaseChannelKey,
       {prompt_parameters.channel,
        permissions::feature_params::
            kPermissionPostPromptSurveyReleaseChannelFilter.Get()}}};
}

// Typos in the gcl configuration cannot be verified and may be missed by
// reviewers. In the worst case, no filters are configured. By definition of
// our filters, this would match all requests. To safeguard against this kind
// of misconfiguration (which would lead to very high HaTS QPS), we enforce
// that at least one valid filter must be configured.
bool IsValidConfiguration(
    permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS
        prompt_parameters) {
  // Returns false if all filter parameters are empty.
  return !base::ranges::all_of(
      GetKeyToValueFilterPairMap(prompt_parameters),
      [](std::pair<std::string, std::pair<std::string, std::string>> entry) {
        return entry.second.second.empty();
      });
}

}  // namespace

namespace permissions {

PermissionHatsTriggerHelper::PromptParametersForHaTS::PromptParametersForHaTS(
    permissions::RequestType request_type,
    permissions::PermissionAction action,
    permissions::PermissionPromptDisposition prompt_disposition,
    permissions::PermissionPromptDispositionReason prompt_disposition_reason,
    permissions::PermissionRequestGestureType gesture_type,
    std::string channel,
    base::TimeDelta prompt_display_duration)
    : request_type(request_type),
      action(action),
      prompt_disposition(prompt_disposition),
      prompt_disposition_reason(prompt_disposition_reason),
      gesture_type(gesture_type),
      channel(channel),
      prompt_display_duration(prompt_display_duration) {}

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
      kPermissionsPostPromptSurveyHadGestureKey};
  const static std::vector<std::string> product_specific_string_fields{
      kPermissionsPostPromptSurveyPromptDispositionKey,
      kPermissionsPostPromptSurveyPromptDispositionReasonKey,
      kPermissionsPostPromptSurveyActionKey,
      kPermissionsPostPromptSurveyRequestTypeKey,
      kPermissionsPostPromptSurveyReleaseChannelKey};
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

bool PermissionHatsTriggerHelper::ArePostPromptTriggerCriteriaSatisfied(
    PromptParametersForHaTS prompt_parameters) {
  if (!IsValidConfiguration(prompt_parameters)) {
    return false;
  }

  if (prompt_parameters.action == permissions::PermissionAction::IGNORED &&
      prompt_parameters.prompt_display_duration >
          permissions::feature_params::
              kPermissionPostPromptSurveyIgnoredPromptsMaximumAge.Get()) {
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

}  // namespace permissions
