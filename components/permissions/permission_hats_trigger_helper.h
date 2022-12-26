// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_HATS_TRIGGER_HELPER_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_HATS_TRIGGER_HELPER_H_

#include <map>
#include <utility>

#include "components/keyed_service/core/keyed_service.h"
#include "components/permissions/permission_util.h"
#include "constants.h"

namespace permissions {

// A static class that handles permission HaTS survey trigger configuration and
// evaluation.
class PermissionHatsTriggerHelper {
  // Key-value mapping type for a HaTS survey's product specific bits data.
  typedef std::map<std::string, bool> SurveyBitsData;

  // Key-value mapping type for HaTS survey's product specific string data.
  typedef std::map<std::string, std::string> SurveyStringData;

 public:
  struct PromptParametersForHaTS {
    PromptParametersForHaTS(
        permissions::RequestType request_type,
        permissions::PermissionAction action,
        permissions::PermissionPromptDisposition prompt_disposition,
        permissions::PermissionPromptDispositionReason
            prompt_disposition_reason,
        permissions::PermissionRequestGestureType gesture_type,
        std::string channel,
        base::TimeDelta prompt_display_duration);
    ~PromptParametersForHaTS();

    permissions::RequestType request_type;
    permissions::PermissionAction action;
    permissions::PermissionPromptDisposition prompt_disposition;
    permissions::PermissionPromptDispositionReason prompt_disposition_reason;
    permissions::PermissionRequestGestureType gesture_type;
    std::string channel;
    base::TimeDelta prompt_display_duration;
  };

  struct SurveyProductSpecificData {
   public:
    ~SurveyProductSpecificData();

    static SurveyProductSpecificData PopulateFrom(
        PromptParametersForHaTS prompt_parameters);

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

  static bool ArePostPromptTriggerCriteriaSatisfied(
      PromptParametersForHaTS prompt_parameters);
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_HATS_TRIGGER_HELPER_H_
