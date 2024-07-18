// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_hats_trigger_helper.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/check_is_test.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/messages/android/message_enums.h"
#include "components/permissions/constants.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/pref_names.h"
#include "components/permissions/request_type.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

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
                              [string](std::string_view current_filter) {
                                return base::EqualsCaseInsensitiveASCII(
                                    string, current_filter);
                              });
}

std::map<std::string, std::pair<std::string, std::string>>
GetKeyToValueFilterPairMap(
    PermissionHatsTriggerHelper::PromptParametersForHats prompt_parameters) {
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
      {kPermissionPromptSurveyUrlKey, {prompt_parameters.url, ""}},
      {kPermissionPromptSurveyPepcPromptPositionKey,
       {prompt_parameters.pepc_prompt_position.has_value()
            ? feature_params::kPermissionElementPromptPositioningParam.GetName(
                  prompt_parameters.pepc_prompt_position.value())
            : "",
        feature_params::kPermissionPromptSurveyPepcPromptPositionFilter.Get()}},
      {kPermissionPromptSurveyInitialPermissionStatusKey,
       {content_settings::ContentSettingToString(
            prompt_parameters.initial_permission_status),
        feature_params::kPermissionPromptSurveyInitialPermissionStatusFilter
            .Get()}}};
}

// Typos in the gcl configuration cannot be verified and may be missed by
// reviewers. In the worst case, no filters are configured. By definition of
// our filters, this would match all requests. To safeguard against this kind
// of misconfiguration (which would lead to very high HaTS QPS), we enforce
// that at least one valid filter must be configured.
bool IsValidConfiguration(
    PermissionHatsTriggerHelper::PromptParametersForHats prompt_parameters) {
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
}  // namespace

PermissionHatsTriggerHelper::PromptParametersForHats::PromptParametersForHats(
    RequestType request_type,
    std::optional<PermissionAction> action,
    PermissionPromptDisposition prompt_disposition,
    PermissionPromptDispositionReason prompt_disposition_reason,
    PermissionRequestGestureType gesture_type,
    const std::string& channel,
    const std::string& survey_display_time,
    std::optional<base::TimeDelta> prompt_display_duration,
    OneTimePermissionPromptsDecidedBucket one_time_prompts_decided_bucket,
    std::optional<GURL> gurl,
    std::optional<permissions::feature_params::PermissionElementPromptPosition>
        pepc_prompt_position,
    ContentSetting initial_permission_status)
    : request_type(request_type),
      action(action),
      prompt_disposition(prompt_disposition),
      prompt_disposition_reason(prompt_disposition_reason),
      gesture_type(gesture_type),
      channel(channel),
      survey_display_time(survey_display_time),
      prompt_display_duration(prompt_display_duration),
      one_time_prompts_decided_bucket(one_time_prompts_decided_bucket),
      url(gurl.has_value() ? gurl->spec() : ""),
      pepc_prompt_position(pepc_prompt_position),
      initial_permission_status(initial_permission_status) {}

PermissionHatsTriggerHelper::SurveyParametersForHats::SurveyParametersForHats(
    double trigger_probability,
    std::optional<std::string> supplied_trigger_id,
    std::optional<std::u16string> custom_survey_invitation,
    std::optional<messages::MessageIdentifier> message_identifier)
    : trigger_probability(trigger_probability),
      supplied_trigger_id(supplied_trigger_id),
      custom_survey_invitation(custom_survey_invitation),
      message_identifier(message_identifier) {}

PermissionHatsTriggerHelper::SurveyParametersForHats::
    ~SurveyParametersForHats() = default;

PermissionHatsTriggerHelper::SurveyParametersForHats::SurveyParametersForHats(
    const SurveyParametersForHats& other) = default;

PermissionHatsTriggerHelper::PromptParametersForHats::PromptParametersForHats(
    const PromptParametersForHats& other) = default;
PermissionHatsTriggerHelper::PromptParametersForHats::
    ~PromptParametersForHats() = default;

PermissionHatsTriggerHelper::SurveyProductSpecificData::
    SurveyProductSpecificData(SurveyBitsData survey_bits_data,
                              SurveyStringData survey_string_data)
    : survey_bits_data(survey_bits_data),
      survey_string_data(survey_string_data) {}

PermissionHatsTriggerHelper::SurveyProductSpecificData::
    ~SurveyProductSpecificData() = default;

PermissionHatsTriggerHelper::SurveyProductSpecificData
PermissionHatsTriggerHelper::SurveyProductSpecificData::PopulateFrom(
    PromptParametersForHats prompt_parameters) {
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
      kPermissionPromptSurveyPepcPromptPositionKey,
      kPermissionPromptSurveyInitialPermissionStatusKey,
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
    PromptParametersForHats prompt_parameters) {
  std::optional<SurveyParametersForHats> survey_parameters =
      PermissionHatsTriggerHelper::GetSurveyParametersForRequestType(
          prompt_parameters.request_type);

  if (!survey_parameters.has_value() ||
      base::RandDouble() >= survey_parameters->trigger_probability) {
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
      PermissionUtil::DoesSupportTemporaryGrants(type)) {
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
      NOTREACHED_IN_MIGRATION();
  }
}

// static
std::optional<PermissionHatsTriggerHelper::SurveyParametersForHats>
PermissionHatsTriggerHelper::GetSurveyParametersForRequestType(
    permissions::RequestType request_type) {
  auto& probability_vector =
      GetProbabilityVector(feature_params::kProbabilityVector.Get());

  std::vector<std::string> permission_trigger_id_vector(
      base::SplitString(feature_params::kPermissionsPromptSurveyTriggerId.Get(),
                        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));

  std::vector<std::string> custom_invitation_trigger_id_vector(
      base::SplitString(
          feature_params::kPermissionsPromptSurveyCustomInvitationTriggerId
              .Get(),
          ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));

  CHECK(custom_invitation_trigger_id_vector.empty() ||
        custom_invitation_trigger_id_vector.size() ==
            permission_trigger_id_vector.size());

  // If custom_invitation_trigger_id_vector is not empty, the custom invitation
  // experiment is active. In that case, we show custom invitations with the
  // corresponding separate triggerId with probability 50%, and the generic
  // invitation with the corresponding separate triggerId in the other 50% of
  // cases.
  bool is_custom_invitation_experiment =
      custom_invitation_trigger_id_vector.size() != 0;
  bool is_custom_invitation_arm =
      is_custom_invitation_experiment && base::RandDouble() < 0.5;

  std::optional<messages::MessageIdentifier> message_identifier;
  std::optional<std::u16string> custom_invitation;
  if (is_custom_invitation_experiment) {
    int request_type_message_id = -1;
    if (request_type == RequestType::kCameraStream) {
      request_type_message_id = IDS_CAMERA_PERMISSION_NAME_FRAGMENT;
      message_identifier = is_custom_invitation_arm
                               ? messages::MessageIdentifier::
                                     PROMPT_HATS_CAMERA_CUSTOM_INVITATION
                               : messages::MessageIdentifier::
                                     PROMPT_HATS_CAMERA_GENERIC_INVITATION;
    } else if (request_type == RequestType::kGeolocation) {
      request_type_message_id = IDS_GEOLOCATION_NAME_FRAGMENT;
      message_identifier = is_custom_invitation_arm
                               ? messages::MessageIdentifier::
                                     PROMPT_HATS_LOCATION_CUSTOM_INVITATION
                               : messages::MessageIdentifier::
                                     PROMPT_HATS_LOCATION_GENERIC_INVITATION;
    } else if (request_type == RequestType::kMicStream) {
      request_type_message_id = IDS_MICROPHONE_PERMISSION_NAME_FRAGMENT;
      message_identifier = is_custom_invitation_arm
                               ? messages::MessageIdentifier::
                                     PROMPT_HATS_MICROPHONE_CUSTOM_INVITATION
                               : messages::MessageIdentifier::
                                     PROMPT_HATS_MICROPHONE_GENERIC_INVITATION;
    }

    // If request_type_message_id == -1, the request is not part of the custom
    // invitation experiment, hence the custom invitation doesn't need to be
    // set.
    if (request_type_message_id != -1 && is_custom_invitation_arm) {
      custom_invitation =
          std::optional<std::u16string>(l10n_util::GetStringFUTF16(
              IDS_PERMISSION_PROMPT_SURVEY_CUSTOM_INVITATION,
              l10n_util::GetStringUTF16(request_type_message_id)));
    }
  }

  if (permission_trigger_id_vector.size() == 1 &&
      probability_vector.size() <= 1) {
    // If a value is configured, use it, otherwise set it to 1.
    double probability =
        probability_vector.size() == 1 ? probability_vector[0] : 1.0;
    const std::string& supplied_trigger_id =
        is_custom_invitation_arm ? custom_invitation_trigger_id_vector[0]
                                 : permission_trigger_id_vector[0];
    return PermissionHatsTriggerHelper::SurveyParametersForHats(
        probability, supplied_trigger_id, custom_invitation);
  } else if (permission_trigger_id_vector.size() != probability_vector.size()) {
    // Configuration error
    return std::nullopt;
  } else {
    auto& request_filter_vector = GetRequestFilterVector(
        feature_params::kPermissionsPromptSurveyRequestTypeFilter.Get());

    if (request_filter_vector.size() != permission_trigger_id_vector.size()) {
      // Configuration error
      return std::nullopt;
    }

    for (unsigned long i = 0; i < permission_trigger_id_vector.size(); i++) {
      if (base::EqualsCaseInsensitiveASCII(
              permissions::PermissionUmaUtil::GetRequestTypeString(
                  request_type),
              request_filter_vector[i])) {
        double probability = probability_vector[i];
        const std::string& supplied_trigger_id =
            is_custom_invitation_arm ? custom_invitation_trigger_id_vector[i]
                                     : permission_trigger_id_vector[i];
        return PermissionHatsTriggerHelper::SurveyParametersForHats(
            probability, supplied_trigger_id, custom_invitation,
            message_identifier);
      }
    }

    // No matching filter
    return std::nullopt;
  }
}

// static
void PermissionHatsTriggerHelper::SetIsTest() {
  is_test = true;
}

}  // namespace permissions
