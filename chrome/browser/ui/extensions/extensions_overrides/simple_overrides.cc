// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_overrides/simple_overrides.h"

#include "extensions/common/api/incognito.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"

namespace {

// Only the following manifest keys are allowed in an extension for it to be
// considered a simple override extension.
// --- When to add to this list ---
// The features in this list should be those that do not give the extension
// *any* additional capability beyond what the corresponding site would have if
// the user manually changed the search. This means these fields can be:
//   a) required (e.g. "name") and internal (e.g. "differential_fingerprint")
//      fields
//   b) trivial appearance (e.g. "icons") and metadata (e.g. "short_name")
//      fields
//   c) localization (e.g. "default_locale") and customization (e.g.
//      "options_page") fields
//   d) the search engine override fields (we don't consider any other overrides
//      to be simple overrides).
// If the field controls anything else, it should be disallowed, and added to
// this file's corresponding unittest.cc.
const char* kAllowlistedManifestKeys[] = {
    "author",  // "author" is a recognized key, but never used as a constant.
    extensions::manifest_keys::kAboutPage,
    extensions::manifest_keys::kCurrentLocale,
    extensions::manifest_keys::kDefaultLocale,
    extensions::manifest_keys::kDescription,
    extensions::manifest_keys::kDifferentialFingerprint,
    extensions::manifest_keys::kHomepageURL,
    extensions::manifest_keys::kIcons,
    extensions::manifest_keys::kIconVariants,
    extensions::manifest_keys::kKey,
    extensions::manifest_keys::kManifestVersion,
    extensions::manifest_keys::kMinimumChromeVersion,
    extensions::manifest_keys::kName,
    extensions::manifest_keys::kOfflineEnabled,
    extensions::manifest_keys::kOptionsPage,
    extensions::manifest_keys::kOptionsUI,
    extensions::manifest_keys::kSettingsOverride,
    extensions::manifest_keys::kShortName,
    extensions::manifest_keys::kUpdateURL,
    extensions::manifest_keys::kVersion,
    extensions::manifest_keys::kVersionName,
    extensions::api::incognito::ManifestKeys::kIncognito,
};

}  // namespace

namespace simple_overrides {

bool IsSimpleOverrideExtension(const extensions::Extension& extension) {
  // Return true only if the extension has exclusively allowlisted keys in the
  // manifest.
  for (const auto [key, value] : extension.manifest()->available_values()) {
    if (base::ranges::find(kAllowlistedManifestKeys, key) ==
        std::end(kAllowlistedManifestKeys)) {
      return false;
    }
  }

  return true;
}

std::vector<std::string> GetAllowlistedManifestKeysForTesting() {
  return std::vector<std::string>(std::begin(kAllowlistedManifestKeys),
                                  std::end(kAllowlistedManifestKeys));
}

}  // namespace simple_overrides
