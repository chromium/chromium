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

namespace content {
class WebContents;
}

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

  // A theme color to use for the given page.
  // This should be used if <meta name="theme_color"> is unset on the page.
  static absl::optional<SkColor> GetFallbackPageThemeColor(
      const AppId& app_id,
      content::WebContents* web_contents);

  // Whether the navigation from |previous_url| to |current_url| should override
  // the default decision for opening URLs in a web app, based on whether the
  // experimental behavior is enabled and whether |current_url| is in the
  // extended scope of the experiment.
  static bool ShouldOverrideUrlLoading(const GURL& previous_url,
                                       const GURL& current_url);

  static void SetAlwaysEnabledForTesting();
  static void SetScopeExtensionsForTesting(
      std::vector<const char* const> scope_extensions_override);
  static void ClearOverridesForTesting();

  ChromeOsWebAppExperiments() = delete;
  ~ChromeOsWebAppExperiments() = delete;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_CHROMEOS_WEB_APP_ExperimentS_H_
