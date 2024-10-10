// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_CONSTANTS_H_

#include "google_apis/gaia/gaia_auth_fetcher.h"

namespace ash::boca {

inline constexpr char kSchoolToolsApiBaseUrl[] =
    "https://staging-schooltools-pa.sandbox.googleapis.com";

inline constexpr char kCreateSessionUrlTemplate[] = "v1/teachers/$1/sessions";

inline constexpr char kContentTypeApplicationJson[] = "application/json";

inline constexpr char kMainStudentGroupName[] = "main";

inline constexpr char kSchoolToolsAuthScope[] =
    "https://www.googleapis.com/auth/chromeosschooltools";

inline constexpr char kGetSessionUrlTemplate[] =
    "v1/users/$1/sessions:getActive";

inline constexpr char kUploadFCMTokenTemplate[] = "v1/users/$1";

inline constexpr char kUpdateSessionUrlTemplate[] =
    "v1/teachers/$1/sessions/$2?updateMask=$3";

inline constexpr char kInsertStudentActivity[] =
    "v1/sessions/$1/students/$2/devices/$3/activities:insert";

inline constexpr char kSessionId[] = "sessionId";
inline constexpr char kTeacher[] = "teacher";
inline constexpr char kRoster[] = "roster";
inline constexpr char kRosterTitle[] = "title";
inline constexpr char kStudentGroupTitle[] = "title";
inline constexpr char kStudentGroups[] = "studentGroups";
inline constexpr char kStudents[] = "students";
inline constexpr char kGaiaId[] = "gaiaId";
inline constexpr char kEmail[] = "email";
inline constexpr char kFullName[] = "fullName";
inline constexpr char kPhotoUrl[] = "photoUrl";
inline constexpr char kStudentGroupsConfig[] = "studentGroupConfigs";
inline constexpr char kStartTime[] = "startTime";
inline constexpr char kDuration[] = "duration";
inline constexpr char kSessionState[] = "sessionState";
inline constexpr char kOnTaskConfig[] = "onTaskConfig";
inline constexpr char kCaptionsConfig[] = "captionsConfig";
inline constexpr char kCaptionsEnabled[] = "captionsEnabled";
inline constexpr char kTranslationsEnabled[] = "translationsEnabled";
inline constexpr char kStudentStatus[] = "studentStatuses";
inline constexpr char kStudentStatusState[] = "state";
inline constexpr char kUrl[] = "url";
inline constexpr char kTitle[] = "title";
inline constexpr char kFavIcon[] = "faviconUrl";
inline constexpr char kContentConfigs[] = "contentConfigs";
inline constexpr char kActiveBundle[] = "activeBundle";
inline constexpr char kLocked[] = "locked";
inline constexpr char kLockedNavigationOptions[] = "lockedNavigationOptions";
inline constexpr char kNavigationType[] = "navigationType";
inline constexpr char kSeconds[] = "seconds";
inline constexpr char kNanos[] = "nanos";
inline constexpr char kRequestTime[] = "requestTime";
inline constexpr char kActivities[] = "activities";
inline constexpr char kActiveTab[] = "activeTab";
inline constexpr char kDevices[] = "devices";
inline constexpr char kDeviceId[] = "deviceId";
inline constexpr char kActivity[] = "activity";

inline constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("boca_classroom_integration", R"(
          semantics: {
            sender: "Boca"
            description: "Provide ChromeOS access to school tools server"
            internal {
              contacts {
                  email: "cros-edu-eng@google.com"
              }
            }
            user_data {
              type: ACCESS_TOKEN
              type: EMAIL
              type: NAME
            }
            trigger: "User opens Boca app and goes through session start flow."
            data: "The request is authenticated with an OAuth2 access token "
                  "identifying the Google account.
            destination: GOOGLE_OWNED_SERVICE
            last_reviewed: "2024-06-26"
          }
          policy: {
            cookies_allowed: NO
            setting: "This feature cannot be disabled by settings yet."
            policy_exception_justification: "Not implemented yet."
          })");

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_CONSTANTS_H_
