// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

namespace {

constexpr const char* kMicrosoftOfficeWebAppExperimentScopeExtensions[] = {
    // The Office editors (Word, Excel, PowerPoint) are located on the
    // OneDrive origin.
    "https://onedrive.live.com/",

    // Links to opening Office editors go via this URL shortener origin.
    "https://1drv.ms/",
};

struct FallbackPageThemeColor {
  const char* page_url_piece;
  SkColor page_theme_color;
};

constexpr FallbackPageThemeColor
    kMicrosoftOfficeWebAppExperimentFallbackPageThemeColors[] = {
        // Word theme color.
        {.page_url_piece = "file%2cdocx",
         .page_theme_color = SkColorSetRGB(0x18, 0x5A, 0xBD)},

        // Excel theme color.
        {.page_url_piece = "file%2cxlsx",
         .page_theme_color = SkColorSetRGB(0x10, 0x7C, 0x41)},

        // PowerPoint theme color.
        {.page_url_piece = "file%2cpptx",
         .page_theme_color = SkColorSetRGB(0xC4, 0x3E, 0x1C)},
};

bool g_always_enabled_for_testing = false;

bool IsExperimentEnabled(const AppId& app_id) {
  return g_always_enabled_for_testing || app_id == kMicrosoftOfficeAppId;
}

absl::optional<std::vector<const char* const>>&
GetScopeExtensionsOverrideForTesting() {
  static base::NoDestructor<absl::optional<std::vector<const char* const>>>
      scope_extensions;
  return *scope_extensions;
}

}  // namespace

base::span<const char* const> ChromeOsWebAppExperiments::GetScopeExtensions(
    const AppId& app_id) {
  DCHECK(
      base::FeatureList::IsEnabled(features::kMicrosoftOfficeWebAppExperiment));

  if (!IsExperimentEnabled(app_id))
    return {};

  if (GetScopeExtensionsOverrideForTesting())
    return *GetScopeExtensionsOverrideForTesting();

  return kMicrosoftOfficeWebAppExperimentScopeExtensions;
}

size_t ChromeOsWebAppExperiments::GetExtendedScopeScore(
    const AppId& app_id,
    base::StringPiece url_spec) {
  DCHECK(
      base::FeatureList::IsEnabled(features::kMicrosoftOfficeWebAppExperiment));

  size_t best_score = 0;
  for (const char* scope : GetScopeExtensions(app_id)) {
    size_t score =
        base::StartsWith(url_spec, scope, base::CompareCase::SENSITIVE)
            ? strlen(scope)
            : 0;
    best_score = std::max(best_score, score);
  }
  return best_score;
}

absl::optional<SkColor> ChromeOsWebAppExperiments::GetFallbackPageThemeColor(
    const AppId& app_id,
    content::WebContents* web_contents) {
  DCHECK(
      base::FeatureList::IsEnabled(features::kMicrosoftOfficeWebAppExperiment));

  if (!IsExperimentEnabled(app_id))
    return absl::nullopt;

  if (!web_contents)
    return absl::nullopt;

  const GURL& url = web_contents->GetLastCommittedURL();
  if (!url.is_valid())
    return absl::nullopt;

  for (const FallbackPageThemeColor& fallback_theme_color :
       kMicrosoftOfficeWebAppExperimentFallbackPageThemeColors) {
    if (base::Contains(url.spec(), fallback_theme_color.page_url_piece))
      return fallback_theme_color.page_theme_color;
  }

  return absl::nullopt;
}

void ChromeOsWebAppExperiments::SetAlwaysEnabledForTesting() {
  g_always_enabled_for_testing = true;
}

void ChromeOsWebAppExperiments::SetScopeExtensionsForTesting(
    std::vector<const char* const> scope_extensions_override) {
  GetScopeExtensionsOverrideForTesting() = std::move(scope_extensions_override);
}

void ChromeOsWebAppExperiments::ClearOverridesForTesting() {
  g_always_enabled_for_testing = false;
  GetScopeExtensionsOverrideForTesting().reset();
}

}  // namespace web_app
