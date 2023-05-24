// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

namespace {

constexpr const char* kMicrosoftOfficeWebAppExperimentScopeExtensions[] = {
    // The Office editors (Word, Excel, PowerPoint) are located on the
    // OneDrive origin.
    "https://onedrive.live.com/",

    // Links to opening Office editors go via this URL shortener origin.
    "https://1drv.ms/",

    // The old branding of the Microsoft 365 web app. Many links within
    // Microsoft 365 still link to the old www.office.com origin.
    "https://www.office.com/",
};

const char kOneDriveBusinessDomain[] = "sharepoint.com";

struct FallbackPageThemeColor {
  std::vector<std::string> page_url_pieces;
  SkColor page_theme_color;
};

const std::vector<FallbackPageThemeColor>&
GetMicrosoftOfficeWebAppExperimentFallbackPageThemeColors() {
  static base::NoDestructor<std::vector<FallbackPageThemeColor>>
      page_theme_colours({
          // Word theme color.
          {.page_url_pieces = {"file%2cdocx", "app=Word"},
           .page_theme_color = SkColorSetRGB(0x18, 0x5A, 0xBD)},

          // Excel theme color.
          {.page_url_pieces = {"file%2cxlsx", "app=Excel"},
           .page_theme_color = SkColorSetRGB(0x10, 0x7C, 0x41)},

          // PowerPoint theme color.
          {.page_url_pieces = {"file%2cpptx", "app=PowerPoint"},
           .page_theme_color = SkColorSetRGB(0xC4, 0x3E, 0x1C)},
      });
  return *page_theme_colours;
}

bool g_always_enabled_for_testing = false;

bool IsExperimentEnabled(const AppId& app_id) {
  return g_always_enabled_for_testing || app_id == kMicrosoft365AppId;
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
  DCHECK(chromeos::features::IsUploadOfficeToCloudEnabled());

  if (!IsExperimentEnabled(app_id))
    return {};

  if (GetScopeExtensionsOverrideForTesting())
    return *GetScopeExtensionsOverrideForTesting();

  return kMicrosoftOfficeWebAppExperimentScopeExtensions;
}

size_t ChromeOsWebAppExperiments::GetExtendedScopeScore(
    const AppId& app_id,
    base::StringPiece url_spec) {
  DCHECK(chromeos::features::IsUploadOfficeToCloudEnabled());

  size_t best_score = 0;
  for (const char* scope : GetScopeExtensions(app_id)) {
    size_t score =
        base::StartsWith(url_spec, scope, base::CompareCase::SENSITIVE)
            ? strlen(scope)
            : 0;
    best_score = std::max(best_score, score);
  }

  // Check the OneDrive Business domain separately as this has a different URL
  // format.
  GURL url = GURL(url_spec);
  if (url.DomainIs(kOneDriveBusinessDomain)) {
    best_score = std::max(best_score, strlen(kOneDriveBusinessDomain));
  }
  return best_score;
}

absl::optional<SkColor> ChromeOsWebAppExperiments::GetFallbackPageThemeColor(
    const AppId& app_id,
    content::WebContents* web_contents) {
  DCHECK(chromeos::features::IsUploadOfficeToCloudEnabled());

  if (!IsExperimentEnabled(app_id))
    return absl::nullopt;

  if (!web_contents)
    return absl::nullopt;

  const GURL& url = web_contents->GetLastCommittedURL();
  if (!url.is_valid())
    return absl::nullopt;

  for (const FallbackPageThemeColor& fallback_theme_color :
       GetMicrosoftOfficeWebAppExperimentFallbackPageThemeColors()) {
    for (const std::string& page_url_piece :
         fallback_theme_color.page_url_pieces) {
      if (base::Contains(url.spec(), page_url_piece)) {
        return fallback_theme_color.page_theme_color;
      }
    }
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
