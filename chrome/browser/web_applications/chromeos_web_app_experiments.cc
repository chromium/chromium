// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"

#include <string_view>

#include "ash/constants/web_app_id_constants.h"
#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "chromeos/constants/chromeos_features.h"

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

constexpr const char* kMicrosoftOfficeWebAppExperimentDomainScopeExtensions[] =
    {
        // The OneDrive Business domain (for the extension to match
        // https://<customer>-my.sharepoint.com).
        "https://sharepoint.com",
};

bool g_always_enabled_for_testing = false;

bool IsExperimentEnabled(const webapps::AppId& app_id) {
  return g_always_enabled_for_testing || app_id == kMicrosoft365AppId;
}

std::optional<std::vector<const char*>>&
GetScopeExtensionsOverrideForTesting() {
  static base::NoDestructor<std::optional<std::vector<const char*>>>
      scope_extensions;
  return *scope_extensions;
}

}  // namespace

ScopeExtensions ChromeOsWebAppExperiments::GetScopeExtensions(
    const webapps::AppId& app_id) {
  DCHECK(chromeos::features::IsUploadOfficeToCloudEnabled());

  ScopeExtensions extensions;
  if (!IsExperimentEnabled(app_id))
    return extensions;

  if (GetScopeExtensionsOverrideForTesting()) {
    for (const auto* origin : *GetScopeExtensionsOverrideForTesting()) {
      extensions.insert(
          ScopeExtensionInfo{.origin = url::Origin::Create(GURL(origin))});
    }
    return extensions;
  }

  for (const auto* url : kMicrosoftOfficeWebAppExperimentScopeExtensions) {
    extensions.insert(
        ScopeExtensionInfo{.origin = url::Origin::Create(GURL(url))});
  }
  for (const auto* url :
       kMicrosoftOfficeWebAppExperimentDomainScopeExtensions) {
    extensions.insert(ScopeExtensionInfo{
        .origin = url::Origin::Create(GURL(url)), .has_origin_wildcard = true});
  }
  return extensions;
}

int ChromeOsWebAppExperiments::GetExtendedScopeScore(
    const webapps::AppId& app_id,
    std::string_view url_spec) {
  DCHECK(chromeos::features::IsUploadOfficeToCloudEnabled());

  const GURL url = GURL(url_spec);
  const auto extensions = GetScopeExtensions(app_id);
  int best_score = 0;
  for (const ScopeExtensionInfo& scope : extensions) {
    const GURL scope_origin = scope.origin.GetURL();
    int score;
    if (scope.has_origin_wildcard) {
      score =
          url.DomainIs(scope_origin.host()) ? scope_origin.spec().length() : 0;
    } else {
      score = base::StartsWith(url_spec, scope_origin.spec(),
                               base::CompareCase::SENSITIVE)
                  ? scope_origin.spec().length()
                  : 0;
    }
    best_score = std::max(best_score, score);
  }
  return best_score;
}

bool ChromeOsWebAppExperiments::IgnoreManifestColor(
    const webapps::AppId& app_id) {
  DCHECK(chromeos::features::IsUploadOfficeToCloudEnabled());
  return IsExperimentEnabled(app_id);
}

void ChromeOsWebAppExperiments::SetAlwaysEnabledForTesting() {
  g_always_enabled_for_testing = true;
}

void ChromeOsWebAppExperiments::SetScopeExtensionsForTesting(
    std::vector<const char*> scope_extensions_override) {
  GetScopeExtensionsOverrideForTesting() = std::move(scope_extensions_override);
}

void ChromeOsWebAppExperiments::ClearOverridesForTesting() {
  g_always_enabled_for_testing = false;
  GetScopeExtensionsOverrideForTesting().reset();
}

}  // namespace web_app
