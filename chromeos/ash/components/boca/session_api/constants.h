// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_CONSTANTS_H_

namespace ash::boca {

// School tools endpoint for managing session.
inline constexpr char kSchoolToolsApiBaseUrl[] =
    "https://staging-schooltools-pa.sandbox.googleapis.com";

inline constexpr char kCreateSessionUrlTemplate[] = "v1/teachers/$1/sessions";

inline constexpr char kContentTypeApplicationJson[] = "application/json";

inline constexpr char kMainStudentGroupName[] = "main";

inline constexpr char kSchoolToolsAuthScope[] =
    "https://www.googleapis.com/auth/chromeosschooltools";

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_CONSTANTS_H_
