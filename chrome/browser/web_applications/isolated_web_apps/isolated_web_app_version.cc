// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_version.h"

#include <string_view>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/version.h"

constexpr uint32_t kMaxNumberOfComponents = 4;

namespace web_app {

// This parser validates that the given version matches `<version core>` in the
// Semantic Versioning specification: https://semver.org.
base::expected<base::Version, IwaVersionParseError> ParseIwaVersion(
    std::string_view version_string) {
  std::vector<uint32_t> components;

  std::vector<std::string_view> component_strings = base::SplitStringPiece(
      version_string, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (component_strings.empty()) {
    return base::unexpected(IwaVersionParseError::kNoComponents);
  }

  for (const auto& component_string : component_strings) {
    if (component_string.empty()) {
      return base::unexpected(IwaVersionParseError::kEmptyComponent);
    }

    // Disallow leading 0s, but allow a single 0.
    if (component_string.size() > 1 && component_string[0] == '0') {
      return base::unexpected(IwaVersionParseError::kLeadingZero);
    }

    // Check that the component only consists of digits.
    if (!std::ranges::all_of(component_string, &base::IsAsciiDigit<char>)) {
      return base::unexpected(IwaVersionParseError::kNonDigit);
    }

    unsigned int number;
    if (!base::StringToUint(component_string, &number)) {
      return base::unexpected(IwaVersionParseError::kCannotConvertToNumber);
    }
    // StringToUint returns unsigned int but Version fields are uint32_t.
    static_assert(sizeof(uint32_t) == sizeof(unsigned int),
                  "uint32_t must be same as unsigned int");

    components.push_back(number);

    // Version should contain not more than kMaxNumberOfComponents components
    if (components.size() > kMaxNumberOfComponents) {
      return base::unexpected(IwaVersionParseError::kTooManyComponents);
    }
  }

  return base::Version(std::move(components));
}

std::string IwaVersionParseErrorToString(IwaVersionParseError error) {
  switch (error) {
    case IwaVersionParseError::kNoComponents:
      return "A version must consist of at least one number";
    case IwaVersionParseError::kEmptyComponent:
      return "A version component may not be empty";
    case IwaVersionParseError::kNonDigit:
      return "A version component may only contain digits";
    case IwaVersionParseError::kLeadingZero:
      return "A version component may not have leading zeros";
    case IwaVersionParseError::kCannotConvertToNumber:
      return "A version component could not be converted into a number";
    case IwaVersionParseError::kTooManyComponents:
      return base::StringPrintf(
          "A version may not contain more than %d components",
          kMaxNumberOfComponents);
  }
}

}  // namespace web_app
