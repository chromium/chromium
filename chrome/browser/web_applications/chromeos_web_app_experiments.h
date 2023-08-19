// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_CHROMEOS_WEB_APP_EXPERIMENTS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_CHROMEOS_WEB_APP_EXPERIMENTS_H_

#include <vector>

#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(IS_CHROMEOS), "For Chrome OS only");

namespace web_app {

// This class contains short-term experiments to specific web apps for testing
// improvements to the user experience of notable web apps.
// These are not intended for public release; this is for testing capabilities
// that will be available to websites at a later date. All experiments should be
// behind a disabled-by-default feature flag. All experiments should eventually
// be replaced by server-side changes made by the specific web app and/or
// improvements to existing web app APIs.
class ChromeOsWebAppExperiments {
 public:
  // Extensions to the app's primary scope. This may include scopes outside
  // of the web app's primary origin.
  // This is intended to be available to websites using the proposed
  // https://github.com/WICG/manifest-incubations/blob/gh-pages/scope_extensions-explainer.md.
  // At the moment, we are enabling testing of the proposed feature for
  // certain hard-coded web apps.
  static base::span<const char* const> GetScopeExtensions(const AppId& app_id);

  // Returns the max scope score (similar to
  // WebAppRegistrar::GetUrlInAppScopeScore()) for the experimental extended
  // scopes.
  static size_t GetExtendedScopeScore(const AppId& app_id,
                                      base::StringPiece url_spec);

  // Whether the manifest theme_color and background_color should be ignored for
  // `app_id`.
  static bool IgnoreManifestColor(const AppId& app_id);

  static void SetAlwaysEnabledForTesting();
  static void SetScopeExtensionsForTesting(
      std::vector<const char* const> scope_extensions_override);
  static void ClearOverridesForTesting();

  ChromeOsWebAppExperiments() = delete;
  ~ChromeOsWebAppExperiments() = delete;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_CHROMEOS_WEB_APP_ExperimentS_H_
