// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_VERSION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_VERSION_H_

#include <string_view>
#include <vector>

#include "base/types/expected.h"
#include "base/version.h"

namespace web_app {

enum class IwaVersionParseError {
  kNoComponents,
  kEmptyComponent,
  kLeadingZero,
  kNonDigit,
  kCannotConvertToNumber,
};

// Parses a string representing the version of an Isolated Web App. Returns the
// parsed version on success.
base::expected<base::Version, IwaVersionParseError> ParseIwaVersion(
    std::string_view version_string);

std::string IwaVersionParseErrorToString(IwaVersionParseError error);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_VERSION_H_
