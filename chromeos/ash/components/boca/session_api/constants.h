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
