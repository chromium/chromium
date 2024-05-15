// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/minimum_chrome_version_checker.h"

#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/grit/branded_strings.h"
#include "components/version_info/version_info.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

MinimumChromeVersionChecker::MinimumChromeVersionChecker() {
}

MinimumChromeVersionChecker::~MinimumChromeVersionChecker() {
}

bool MinimumChromeVersionChecker::Parse(Extension* extension,
                                        std::u16string* error) {
  const std::string* minimum_version_string =
      extension->manifest()->FindStringPath(keys::kMinimumChromeVersion);
  if (minimum_version_string == nullptr) {
    *error = errors::kInvalidMinimumChromeVersion;
    return false;
  }

  base::Version minimum_version(*minimum_version_string);
  if (!minimum_version.IsValid()) {
    *error = errors::kInvalidMinimumChromeVersion;
    return false;
  }

  const base::Version& current_version = version_info::GetVersion();
  if (!current_version.IsValid()) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  if (current_version.CompareTo(minimum_version) < 0) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kChromeVersionTooLow,
        l10n_util::GetStringUTF8(IDS_PRODUCT_NAME), *minimum_version_string);
    return false;
  }
  return true;
}

base::span<const char* const> MinimumChromeVersionChecker::Keys() const {
  static constexpr const char* kKeys[] = {keys::kMinimumChromeVersion};
  return kKeys;
}

}  // namespace extensions
