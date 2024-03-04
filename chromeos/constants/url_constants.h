// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_URL_CONSTANTS_H_
#define CHROMEOS_CONSTANTS_URL_CONSTANTS_H_

#include <string_view>

namespace chromeos {

// The URI scheme for triggering app installs from Almanac.
inline constexpr std::string_view kAppInstallUriScheme = "almanac";

}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_URL_CONSTANTS_H_
