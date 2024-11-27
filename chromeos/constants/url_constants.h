// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_URL_CONSTANTS_H_
#define CHROMEOS_CONSTANTS_URL_CONSTANTS_H_

#include <string_view>

namespace chromeos {

// The URI scheme for triggering app installs from Almanac.
inline constexpr std::string_view kAppInstallUriScheme = "cros-apps";
// TODO(b/333983839): Remove legacy scheme when M124 is out of circulation.
inline constexpr std::string_view kLegacyAppInstallUriScheme = "almanac";

// The base URL for the Mall app.
inline constexpr std::string_view kAppMallBaseUrl =
    "https://discover.apps.chrome/";

}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_URL_CONSTANTS_H_
