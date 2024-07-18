// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_HATS_TRIGGER_HELPER_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_HATS_TRIGGER_HELPER_H_

#include <map>
#include <optional>
#include <utility>

#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/messages/android/message_enums.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_util.h"
#include "constants.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefService;
namespace permissions {

constexpr char kTrueStr[] = "true";
constexpr char kFalseStr[] = "false";

constexpr char kOnPromptAppearing[] = "OnPromptAppearing";
constexpr char kOnPromptResolved[] = "OnPromptResolved";

// A static class that handles permission HaTS survey trigger configuration and
// evaluation.
class PermissionHatsTriggerHelper {
  // Key-value mapping type for a HaTS survey's product specific bits data.
  typedef std::map<std::string, bool> SurveyBitsData;

  // Key-value mapping type for HaTS survey's product specific string data.
  typedef std::map<std::string, std::string> SurveyStringData;

 public:
  enum OneTimePermissionPromptsDecidedBucket {
    BUCKET_0_1,    // 0-1
    BUCKET_2_3,    // 2-3
    BUCKET_4_5,    // 4-5
    BUCKET_6_10,   // 6-10
    BUCKET_11_20,  // 11-20
    BUCKET_GT20    // >20
  };

  struct PromptParametersForHats {
    PromptParametersForHats(
        permissions::RequestType request_type,
        std::optional<permissions::PermissionAction> action,
        permissions::PermissionPromptDisposition prompt_disposition,
        permissions::PermissionPromptDispositionReason
            prompt_disposition_reason,
        permissions::PermissionRequestGestureType gesture_type,
        const std::string& channel,
        const std::string& survey_display_time,
        std::optional<base::TimeDelta> prompt_display_duration,
        OneTimePermissionPromptsDecidedBucket one_time_prompts_decided_bucket,
        std::optional<GURL> gurl,
        std::optional<
            permissions::feature_params::PermissionElementPromptPosition>
            pepc_prompt_position,
        ContentSetting initial_permission_status);
    PromptParametersForHats(const PromptParametersForHats& other);
    ~PromptParametersForHats();

    permissions::RequestType request_type;
    std::optional<permissions::PermissionAction> action;
    permissions::PermissionPromptDisposition prompt_disposition;
    permissions::PermissionPromptDispositionReason prompt_disposition_reason;
    permissions::PermissionRequestGestureType gesture_type;
    std::string channel;
    std::string survey_display_time;
    std::optional<base::TimeDelta> prompt_display_duration;
    OneTimePermissionPromptsDecidedBucket one_time_prompts_decided_bucket;
    std::string url;
    std::optional<permissions::feature_params::PermissionElementPromptPosition>
        pepc_prompt_position;
    ContentSetting initial_permission_status;
  };

  struct SurveyParametersForHats {
    explicit SurveyParametersForHats(
        double trigger_probability,
        std::optional<std::string> supplied_trigger_id = std::nullopt,
        std::optional<std::u16string> custom_survey_invitation = std::nullopt,
        std::optional<messages::MessageIdentifier> message_identifier =
            std::nullopt);
    SurveyParametersForHats(const SurveyParametersForHats& other);
    ~SurveyParametersForHats();

    double trigger_probability;
    std::optional<std::string> supplied_trigger_id;
    std::optional<std::u16string> custom_survey_invitation;
    std::optional<messages::MessageIdentifier> message_identifier;
  };

  struct SurveyProductSpecificData {
   public:
    ~SurveyProductSpecificData();

    static SurveyProductSpecificData PopulateFrom(
        PromptParametersForHats prompt_parameters);

    const SurveyBitsData survey_bits_data;
    const SurveyStringData survey_string_data;

   private:
    explicit SurveyProductSpecificData(SurveyBitsData survey_bits_data,
                                       SurveyStringData survey_string_data);
  };

  ~PermissionHatsTriggerHelper() = delete;
  PermissionHatsTriggerHelper(const PermissionHatsTriggerHelper&) = delete;
  PermissionHatsTriggerHelper& operator=(const PermissionHatsTriggerHelper&) =
      delete;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static bool ArePromptTriggerCriteriaSatisfied(
      PromptParametersForHats prompt_parameters);

  static OneTimePermissionPromptsDecidedBucket GetOneTimePromptsDecidedBucket(
      PrefService* pref_service);

  // Increments the count representing the one time permission prompts seen by
  // the user.
  static void IncrementOneTimePermissionPromptsDecidedIfApplicable(
      ContentSettingsType type,
      PrefService* profile);

  // Bucketing used to categorize users by how many one time permission they
  // have decided.
  static std::string GetOneTimePromptsDecidedBucketString(
      OneTimePermissionPromptsDecidedBucket bucket);

  // Returns the survey parameters corresponding to a specific
  // request type. Returns empty value if there is a configuration error or the
  // passed request type is not configured.
  static std::optional<SurveyParametersForHats>
  GetSurveyParametersForRequestType(RequestType request_type);

  static void SetIsTest();
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_HATS_TRIGGER_HELPER_H_
