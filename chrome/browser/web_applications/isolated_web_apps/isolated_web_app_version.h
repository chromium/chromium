// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_VERSION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_VERSION_H_

#include <vector>

#include "base/strings/string_piece_forward.h"
#include "base/types/expected.h"

namespace web_app {

enum class IwaVersionParseError {
  kNoComponents,
  kEmptyComponent,
  kLeadingZero,
  kNonDigit,
  kCannotConvertToNumber,
};

// Parses a string representing the version of an Isolated Web App. Returns the
// parsed version components on success.
base::expected<std::vector<uint32_t>, IwaVersionParseError>
ParseIwaVersionIntoComponents(base::StringPiece version_string);

std::string IwaVersionParseErrorToString(IwaVersionParseError error);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_VERSION_H_
