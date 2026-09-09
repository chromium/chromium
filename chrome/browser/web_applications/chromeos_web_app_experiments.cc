// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"

#include <string_view>

#include "ash/constants/web_app_id_constants.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace web_app {

namespace {

constexpr const char* kMicrosoft365ManifestId = "?from=Homescreen";

constexpr const char* const kMicrosoft365ScopeExtensionUrls[] = {
    // The Office editors (Word, Excel, PowerPoint) are located on the
    // OneDrive origin.
    "https://onedrive.live.com/",

    // Links to opening Office editors go via this URL shortener origin.
    "https://1drv.ms/",

    // The old branding of the Microsoft 365 web app. Many links within
    // Microsoft 365 still link to the old www.office.com origin.
    "https://www.office.com/",

    // The new branding for the Microsoft 365 web app.
    "https://m365.cloud.microsoft/",

    // The current Microsoft 365 web app. The scope of the new Microsoft 365
    // Copilot web app remains unclear, so this is added for safety.
    "https://www.microsoft365.com/",
};

constexpr const char* const kMicrosoft365ScopeExtensionDomains[] = {
    // The OneDrive Business domain (for the extension to match
    // https://<customer>-my.sharepoint.com).
    "https://sharepoint.com",

    // The new branding for Microsoft 365 web apps. Word, PowerPoint and Excel
    // can be accessed under https://word.cloud.microsoft/,
    // https://powerpoint.cloud.microsoft/ and https://excel.cloud.microsoft/
    // respectively.
    "https://cloud.microsoft",
};

constexpr const char* const kMicrosoft365ManifestUrls[] = {
    // The current Microsoft 365 web app.
    "https://www.microsoft365.com/",

    // The new branding for the Microsoft 365 web app.
    "https://m365.cloud.microsoft/",
};

bool g_always_enabled_for_testing = false;

bool IsExperimentEnabled(const webapps::AppId& app_id) {
  return g_always_enabled_for_testing || app_id == ash::kMicrosoft365AppId;
}

// IsValidScopeExtension returns whether a url can be successfully turned into
// a scope extension or not.
bool IsValidScopeExtension(const GURL& url) {
  return url.is_valid() && url.IsStandard() && url.has_host() &&
         !base::StartsWith(url.GetHost(), ".");
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
  ScopeExtensions extensions;
  if (!IsExperimentEnabled(app_id)) {
    return extensions;
  }

  if (GetScopeExtensionsOverrideForTesting()) {
    for (const auto* origin : *GetScopeExtensionsOverrideForTesting()) {
      extensions.insert(ScopeExtensionInfo::CreateForOrigin(
          url::Origin::Create(GURL(origin))));
    }
    return extensions;
  }

  for (const auto* url_string : kMicrosoft365ScopeExtensionUrls) {
    const GURL url = GURL(url_string);
    if (!IsValidScopeExtension(url)) {
      LOG(WARNING) << "Skipping invalid M365 scope extension URL: "
                   << url_string;
      continue;
    }
    extensions.insert(
        ScopeExtensionInfo::CreateForOrigin(url::Origin::Create(GURL(url))));
  }
  for (const auto* url_string : kMicrosoft365ScopeExtensionDomains) {
    const GURL url = GURL(url_string);
    if (!IsValidScopeExtension(url)) {
      LOG(WARNING) << "Skipping invalid M365 scope extension domain: "
                   << url_string;
      continue;
    }
    extensions.insert(ScopeExtensionInfo::CreateForOrigin(
        url::Origin::Create(GURL(url)), /*has_origin_wildcard=*/true));
  }
  return extensions;
}

bool ChromeOsWebAppExperiments::ShouldAddLinkPreference(
    const webapps::AppId& app_id,
    Profile* profile) {
  return IsExperimentEnabled(app_id) &&
         chromeos::cloud_upload::IsMicrosoftOfficeOneDriveIntegrationAutomated(
             profile);
}

int ChromeOsWebAppExperiments::GetExtendedScopeScore(
    const webapps::AppId& app_id,
    std::string_view url_spec) {
  const GURL url = GURL(url_spec);
  const auto extensions = GetScopeExtensions(app_id);
  int best_score = 0;
  for (const ScopeExtensionInfo& scope : extensions) {
    const GURL scope_origin = scope.origin.GetURL();
    int score;
    if (scope.has_origin_wildcard) {
      score = url.DomainIs(scope_origin.GetHost())
                  ? scope_origin.spec().length()
                  : 0;
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
  return IsExperimentEnabled(app_id);
}

bool ChromeOsWebAppExperiments::IsNavigationCapturingReimplEnabledForTargetApp(
    const webapps::AppId& target_app_id) {
  return IsExperimentEnabled(target_app_id);
}

bool ChromeOsWebAppExperiments::IsNavigationCapturingReimplEnabledForSourceApp(
    const webapps::AppId& source_app_id,
    const GURL& url) {
  // Until Navigation Capturing Reimplementation is fully enabled, hardcode
  // specific destination URLs for the typical scenarios in which we want the
  // user to stay inside the Office PWA (note that URLs that are already within
  // the PWA's scope are covered by
  // `IsNavigationCapturingReimplEnabledForTargetApp()`).
  return IsExperimentEnabled(source_app_id) && url == url::kAboutBlankURL;
}

bool ChromeOsWebAppExperiments::ShouldLaunchForRedirectedNavigation(
    const webapps::AppId& target_app_id) {
  return IsExperimentEnabled(target_app_id);
}

void ChromeOsWebAppExperiments::MaybeOverrideManifest(
    content::RenderFrameHost* frame_host,
    blink::mojom::ManifestPtr& manifest) {
  const auto pwa_start_url_origin = url::Origin::Create(manifest->start_url);
  std::string pwa_start_url_path =
      manifest->start_url.GetWithoutFilename().GetPath();

  for (const auto* url_string : kMicrosoft365ManifestUrls) {
    GURL microsoft365_manifest_url = GURL(url_string);

    if (pwa_start_url_origin.IsSameOriginWith(microsoft365_manifest_url) &&
        pwa_start_url_path == microsoft365_manifest_url.GetPath()) {
      manifest->id =
          GURL(pwa_start_url_origin.GetURL().spec() + kMicrosoft365ManifestId);
    }
  }
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
